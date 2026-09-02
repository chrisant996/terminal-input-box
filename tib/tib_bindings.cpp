// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_bindings.h"
#include "tib_context.h"
#include "tib_terminal.h"
#include <algorithm>
#include <assert.h>

namespace tib {

struct binding_resolver_state
{
    std::weak_ptr<dispatcher_target> quoted_insert_target;
};

binding_target::binding_target(binding_type type, const char* text, size_t len) noexcept
{
    switch (type)
    {
    case binding_type::none:
        assert(m_type == binding_type::none);
        assert(!m_text);
        assert(!m_length);
        break;
    case binding_type::func:
        set_func(text);
        break;
    case binding_type::macro:
        set_macro(text, len);
        break;
    case binding_type::quoted_insert:
        assert(len == 1);
        assert(*text && uint8_t(*text) < c_input_terminal_reserved_begin);
        set_quoted_insert(*text);
        break;
    default:
        assert(false);
        break;
    }
}

binding_target binding_target_func(const char* name)
{
    return binding_target(binding_type::func, name);
}

binding_target binding_target_macro(const char* text, size_t len)
{
    return binding_target(binding_type::macro, text, len);
}

binding_target binding_target_quoted_insert(char c)
{
    return binding_target(binding_type::quoted_insert, &c, 1);
}

bool binding_target::operator==(const binding_target& t) const noexcept
{
    if (m_type != t.m_type)
        return false;
    switch (m_type)
    {
    case binding_type::none:
        break;
    case binding_type::func:
        if (!m_text != !t.m_text)
            return false;
        if (m_text && t.m_text && strcmp(m_text, t.m_text) != 0)
            return false;
        break;
    case binding_type::macro:
        if (!m_text != !t.m_text)
            return false;
        if (m_length != t.m_length)
            return false;
        if (m_text && t.m_text && memcmp(m_text, t.m_text, m_length) != 0)
            return false;
        break;
    default:
        assert(false);
        break;
    }
    return true;
}

bool binding_target::is_func_name(const char* name) const noexcept
{
    return (name && m_type == binding_type::func && m_text && strcmp(name, m_text) == 0);
}

void binding_target::clear() noexcept
{
    m_type = binding_type::none;
    m_text = nullptr;
    m_length = 0;
}

void binding_target::set_func(const char* name) noexcept
{
    assert(name);
    m_type = binding_type::func;
    m_text = name;
    m_length = 0;
}

void binding_target::set_macro(const char* text, size_t len) noexcept
{
    assert(text);
    len = resolve_auto_length(len, text);
    m_type = binding_type::macro;
    m_text = text;
    m_length = len;
}

void binding_target::set_quoted_insert(char c) noexcept
{
    assert(c && uint8_t(c) < c_input_terminal_reserved_begin);
    m_type = binding_type::quoted_insert;
    m_text = nullptr;
    m_length = uint8_t(c);
}

binding_target_copy::binding_target_copy(const binding_target& t) noexcept
{
    *this = t;
}

binding_target_copy& binding_target_copy::operator=(const binding_target& t) noexcept
{
    switch (t.get_type())
    {
    case binding_type::none:
        m_owned_text.clear();
        clear();
        break;
    case binding_type::func:
        m_owned_text.set(t.get_text());
        set_func(m_owned_text.c_str());
        break;
    case binding_type::macro:
        m_owned_text.set(t.get_text(), t.get_length());
        set_macro(m_owned_text.c_str(), m_owned_text.length());
        break;
    case binding_type::quoted_insert:
        m_owned_text.clear();
        set_quoted_insert(char(t.get_char()));
    default:
        assert(false);
        m_owned_text.clear();
        clear();
        break;
    }
    return *this;
}

key_table::~key_table()
{
}

int key_table::sort_predicate(const key_binding& candidate, const key_binding& binding)
{
    // Sort order is:
    //  1.  Literal bindings, in alphabetical order.
    //  2.  Pattern bindings, in alphabetical order.
    int comparison = int(candidate.pattern) - int(binding.pattern);
    if (!comparison)
    {
        const size_t common_length = min(candidate.sequence.length(), binding.sequence.length());
        comparison = memcmp(candidate.sequence.c_str(), binding.sequence.c_str(), common_length);
        if (!comparison)
            comparison = int(candidate.sequence.length()) - int(binding.sequence.length());
    }
    return comparison < 0;
}

bool key_table::add(key_binding&& binding)
{
    assert(binding.sequence.length() > 0);
    if (!binding.sequence.length())
        return false;

    const auto found = std::lower_bound(m_bindings.begin(), m_bindings.end(), binding, sort_predicate);

    if (found != m_bindings.end() && found->sequence == binding.sequence)
        *found = std::move(binding);
    else
        m_bindings.insert(found, std::move(binding));

    return true;
}

bool key_table::remove(const cstring& sequence, bool pattern)
{
    assert(sequence.length() > 0);
    if (!sequence.length())
        return false;

    key_binding binding;
    binding.sequence = sequence;
    binding.pattern = pattern;

    const auto found = std::lower_bound(m_bindings.begin(), m_bindings.end(), binding, sort_predicate);

    if (found != m_bindings.end() && found->sequence == sequence)
        m_bindings.erase(found);

    return true;
}

void key_table::clear()
{
    m_bindings.clear();
}

std::shared_ptr<const key_table_list> dispatcher_target::get_bindings() const
{
    return m_bindings;
}

void dispatcher_target::set_bindings(std::shared_ptr<const key_table_list> bindings)
{
    m_bindings = bindings;
}

resolved_binding::resolved_binding(std::shared_ptr<binding_resolver_state> state)
: m_resolver_state(state)
{
}

resolved_binding::operator bool()
{
    return (outcome == dispatch_outcome::match ||
            outcome == dispatch_outcome::self_insert ||
            outcome == dispatch_outcome::quoted_insert);
}

bool resolved_binding::dispatch()
{
    const bool self_insert = (outcome == dispatch_outcome::self_insert);
    const bool quoted_insert = (outcome == dispatch_outcome::quoted_insert);
    const bool literal_insert = self_insert || quoted_insert;
    assert(implies(self_insert, is_self_insertable(key)));
    assert(implies(literal_insert, sequence.length() == 1));
    assert(implies(literal_insert, uint8_t(sequence.c_str()[0]) == key));

    switch (outcome)
    {
    case dispatch_outcome::self_insert:
    case dispatch_outcome::quoted_insert:
    case dispatch_outcome::match:
        {
            auto ctx = dispatcher_target.lock();
            if (ctx)
            {
                tib::binding_target quoted_target;
                const tib::binding_target* target = binding_target;
                if (quoted_insert)
                {
                    quoted_target.set_quoted_insert(char(key));
                    target = &quoted_target;
                }

                assert(self_insert == !target);
                if (target && target->get_type() == binding_type::macro)
                    term_push_macro_text(target->get_text(), target->get_length());
                else
                {
                    const int32_t result = ctx->dispatch(sequence, key, target, &params);
                    if (result == c_dispatch_request_quoted_insert && m_resolver_state)
                        m_resolver_state->quoted_insert_target = ctx;
                }
                return true;
            }
            else
            {
                outcome = dispatch_outcome::expired;
            }
        }
        break;
    case dispatch_outcome::miss:
        ding();
        break;
    }

    return false;
}

binding_resolver::binding_resolver()
    : m_state(std::make_shared<binding_resolver_state>())
{
}

void binding_resolver::clear_targets()
{
    m_registrants.clear();
    m_state->quoted_insert_target.reset();
}

void binding_resolver::add_target(std::weak_ptr<dispatcher_target> target)
{
    m_registrants.emplace_back(target);
    reset();
}

void binding_resolver::reset()
{
    m_sequence.clear();
}

resolved_binding binding_resolver::step(uint8_t c)
{
    if (!m_state->quoted_insert_target.expired())
    {
        const std::weak_ptr<dispatcher_target> weak = m_state->quoted_insert_target;
        m_state->quoted_insert_target.reset();
        reset();

        resolved_binding resolved(m_state);
        resolved.sequence.append(reinterpret_cast<const char*>(&c), 1);
        resolved.key = c;
        resolved.dispatcher_target = weak;
        resolved.outcome = dispatch_outcome::quoted_insert;
        return resolved;
    }

    m_sequence.append(reinterpret_cast<const char*>(&c), 1);

    // Search the key tables in priority order (later tables overlay earlier
    // tables) looking for an exact match or a prefix match.
    bool is_prefix = false;
    int8_t can_self_insert = -1;
    bool has_self_insert_target = false;
    std::weak_ptr<dispatcher_target> self_insert_target;
    for (auto& weak : m_registrants)
    {
        std::shared_ptr<dispatcher_target> target = weak.lock();
        if (!target)
            continue;

        const auto bindings_list = target->get_bindings();
        if (!bindings_list)
            continue;

        for (auto& table = bindings_list->rbegin(); table != bindings_list->rend(); ++table)
        {
            // Only one table can accept self-insert input; last one in the
            // bindings list wins.
            assert(can_self_insert <= 0);
            if (can_self_insert < 0)
                can_self_insert = (*table)->can_self_insert();

            if (can_self_insert > 0 && m_sequence.length() == 1 && !has_self_insert_target)
            {
                self_insert_target = target;
                has_self_insert_target = true;
            }

            const auto& bindings = (*table)->m_bindings;
            const auto patterns = std::partition_point(bindings.begin(), bindings.end(), [](const key_binding& binding) {
                return !binding.pattern;
            });
            const auto found = std::lower_bound(bindings.begin(), patterns, m_sequence, [](const key_binding& candidate, const cstring& sequence) {
                const size_t common_length = min(candidate.sequence.length(), sequence.length());
                const int comparison = memcmp(candidate.sequence.c_str(), sequence.c_str(), common_length);
                return comparison < 0 || (comparison == 0 && candidate.sequence.length() < sequence.length());
            });

            if (found != patterns &&
                found->sequence.length() >= m_sequence.length() &&
                memcmp(found->sequence.c_str(), m_sequence.c_str(), m_sequence.length()) == 0)
            {
                if (found->sequence.length() == m_sequence.length())
                {
                    resolved_binding resolved(m_state);
                    resolved.sequence = m_sequence;
                    resolved.key = c;
                    resolved.binding_target = &found->target;
                    resolved.dispatcher_target = weak;
                    resolved.outcome = dispatch_outcome::match;
                    reset();
                    return resolved;
                }
                is_prefix = true;
            }

            if (found == patterns ||
                found->sequence.length() != m_sequence.length() ||
                memcmp(found->sequence.c_str(), m_sequence.c_str(), m_sequence.length()) != 0)
            {
                const char* const input = m_sequence.c_str();
                const size_t input_length = m_sequence.length();
                for (auto pattern = patterns; pattern != bindings.end(); ++pattern)
                {
                    const char* const sequence = pattern->sequence.c_str();
                    const size_t sequence_length = pattern->sequence.length();
                    size_t input_pos = 0;
                    size_t sequence_pos = 0;
                    binding_params params;

                    while (input_pos < input_length && sequence_pos < sequence_length)
                    {
                        if (sequence[sequence_pos] == '%')
                        {
                            if (sequence_pos + 1 >= sequence_length)
                            {
                                // Malformed pattern string.
                                assert(false);
                                goto continue_label;
                            }

                            const char op = sequence[sequence_pos + 1];
                            switch (op)
                            {
                            case '#':
                                // Match 1 or more digits.
                                if (input[input_pos] >= '0' && input[input_pos] <= '9')
                                {
                                    const size_t param_begin = input_pos;
                                    do
                                    {
                                        ++input_pos;
                                    } while (input_pos < input_length && input[input_pos] >= '0' && input[input_pos] <= '9');
                                    params.emplace_back(input + param_begin, input_pos - param_begin);
                                    sequence_pos += 2;
                                    continue;
                                }
                                else
                                {
                                    // Not a digit; pattern does not match.
                                    goto continue_label;
                                }
                                break;
                            case '!':
                                // Match 1 character in the range 0x20..0xff or
                                // 0x00.  If >= 0x20 then subtract 0x20 and
                                // convert it to a numeric string.  If 0x00 then
                                // convert 0 to a numeric string.  This lets
                                // consumers of tib support the default mouse
                                // encoding without
                                // needing to parse the raw bytes themselves.
                                if (!input[input_pos] || uint8_t(input[input_pos]) >= 0x20)
                                {
                                    cstring param;
                                    const uint8_t value = uint8_t(input[input_pos]);
                                    if (value >= 0x20)
                                        param.printf("%u", value - 0x20);
                                    params.emplace_back(std::move(param));
                                    sequence_pos += 2;
                                    ++input_pos;
                                    continue;
                                }
                                goto continue_label;
                            case '%':
                            default:
                                ++sequence_pos;
                                break;
                            }
                        }

                        if (input[input_pos] != sequence[sequence_pos])
                            break;
                        ++input_pos;
                        ++sequence_pos;
                    }

                    if (input_pos == input_length)
                    {
                        if (sequence_pos == sequence_length)
                        {
                            resolved_binding resolved(m_state);
                            resolved.sequence = m_sequence;
                            resolved.key = c;
                            resolved.binding_target = &pattern->target;
                            resolved.dispatcher_target = weak;
                            resolved.outcome = dispatch_outcome::match;
                            resolved.params = std::move(params);
                            reset();
                            return resolved;
                        }
                        is_prefix = true;
                    }

continue_label:
                    ;
                }
            }

            // Only one table gets to accept self-insert input.
            if (can_self_insert > 0)
                can_self_insert = 0;
        }
    }

    if (is_prefix)
    {
        resolved_binding resolved;
        resolved.sequence = m_sequence;
        resolved.key = c;
        resolved.outcome = dispatch_outcome::more;
        // Do not reset() yet; there is more...
        return resolved;
    }

    if (m_sequence.length() > 1)
    {
        // Discard the sequence before c and try again.
        reset();
        return step(c);
    }

    if (m_sequence.length() == 1 && has_self_insert_target && is_self_insertable(m_sequence.c_str()[0]))
    {
        resolved_binding resolved;
        resolved.sequence = m_sequence;
        resolved.key = c;
        resolved.dispatcher_target = self_insert_target;
        resolved.outcome = dispatch_outcome::self_insert;
        reset();
        return resolved;
    }

    resolved_binding resolved;
    resolved.sequence = m_sequence;
    resolved.key = c;
    resolved.outcome = dispatch_outcome::miss;
    reset();
    return resolved;
}

bool is_self_insertable(char c)
{
    return (c < 0 || c >= ' ') && !(uint8_t(c) == c_input_terminal_eof || uint8_t(c) == c_input_terminal_resize);
}

bool is_self_insertable(int32_t key)
{
    return (key >= ' ' && key <= 0xff && key != c_input_terminal_eof && key != c_input_terminal_resize);
}

} // namespace tib
