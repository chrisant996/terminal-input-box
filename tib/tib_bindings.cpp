// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_bindings.h"
#include "tib_context.h"
#include "tib_input.h"
#include <algorithm>
#include <assert.h>

namespace tib {

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

bool key_table::add(key_binding&& binding)
{
    assert(binding.sequence.length() > 0);
    if (!binding.sequence.length())
        return false;

    const auto found = std::lower_bound(m_bindings.begin(), m_bindings.end(), binding.sequence, [](const key_binding& candidate, const cstring& sequence) {
        const size_t common_length = min(candidate.sequence.length(), sequence.length());
        const int comparison = memcmp(candidate.sequence.c_str(), sequence.c_str(), common_length);
        return comparison < 0 || (comparison == 0 && candidate.sequence.length() < sequence.length());
    });

    if (found != m_bindings.end() && found->sequence == binding.sequence)
        *found = std::move(binding);
    else
        m_bindings.insert(found, std::move(binding));

    return true;
}

bool key_table::remove(const cstring& sequence)
{
    assert(sequence.length() > 0);
    if (!sequence.length())
        return false;

    const auto found = std::lower_bound(m_bindings.begin(), m_bindings.end(), sequence, [](const key_binding& candidate, const cstring& sequence) {
        const size_t common_length = min(candidate.sequence.length(), sequence.length());
        const int comparison = memcmp(candidate.sequence.c_str(), sequence.c_str(), common_length);
        return comparison < 0 || (comparison == 0 && candidate.sequence.length() < sequence.length());
    });

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

void dispatcher::clear_targets()
{
    m_registrants.clear();
    m_recalc_can_self_insert = true;
}

void dispatcher::add_target(std::weak_ptr<dispatcher_target> target)
{
    m_registrants.emplace_back(target);
    m_recalc_can_self_insert = true;

    reset();
}

void dispatcher::reset()
{
    m_sequence.clear();
    m_binding_target = nullptr;
    m_dispatcher_target.reset();
    m_outcome = dispatch_outcome::miss;
}

dispatch_outcome dispatcher::step(uint8_t c)
{
    maybe_recalc_can_self_insert();

    dispatch_outcome outcome = step_internal(c);

    const bool self_insert = (outcome == dispatch_outcome::self_insert);
    assert(implies(self_insert, m_can_self_insert > 0 && is_self_insertable(uint8_t(c))));
    assert(implies(self_insert, get_sequence().length() == 1));
    assert(implies(self_insert, uint8_t(get_sequence().c_str()[0]) == c));

    switch (outcome)
    {
    case dispatch_outcome::self_insert:
    case dispatch_outcome::match:
        {
            auto ctx = m_dispatcher_target.lock();
            if (ctx)
            {
                const auto target = get_binding_target();
                assert(self_insert == !target);
                if (target && target->get_type() == binding_type::macro)
                    term_push_macro_text(target->get_text(), target->get_length());
                else
                    ctx->dispatch(get_sequence(), uint8_t(c), target);
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

    return outcome;
}

dispatch_outcome dispatcher::step_internal(uint8_t c)
{
    if (m_outcome != dispatch_outcome::more)
        reset();

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
            const auto found = std::lower_bound(bindings.begin(), bindings.end(), m_sequence, [](const key_binding& candidate, const cstring& sequence) {
                const size_t common_length = min(candidate.sequence.length(), sequence.length());
                const int comparison = memcmp(candidate.sequence.c_str(), sequence.c_str(), common_length);
                return comparison < 0 || (comparison == 0 && candidate.sequence.length() < sequence.length());
            });

            if (found != bindings.end() &&
                found->sequence.length() >= m_sequence.length() &&
                memcmp(found->sequence.c_str(), m_sequence.c_str(), m_sequence.length()) == 0)
            {
                if (found->sequence.length() == m_sequence.length())
                {
                    m_binding_target = &found->target;
                    m_dispatcher_target = weak;
                    m_outcome = dispatch_outcome::match;
                    return m_outcome;
                }
                is_prefix = true;
            }

            // Only one table gets to accept self-insert input.
            if (can_self_insert > 0)
                can_self_insert = 0;
        }
    }

    if (is_prefix)
    {
        m_outcome = dispatch_outcome::more;
        return m_outcome;
    }

    if (m_sequence.length() > 1)
    {
        // Discard the sequence before c and try again.
        reset();
        return step(c);
    }

    if (m_sequence.length() == 1 && has_self_insert_target && is_self_insertable(m_sequence.c_str()[0]))
    {
        m_binding_target = nullptr;
        m_dispatcher_target = self_insert_target;
        m_outcome = dispatch_outcome::self_insert;
        return m_outcome;
    }

    m_outcome = dispatch_outcome::miss;
    return m_outcome;
}

void dispatcher::maybe_recalc_can_self_insert()
{
    if (!m_recalc_can_self_insert)
        return;

    m_recalc_can_self_insert = false;
    m_can_self_insert = -1;

    for (auto& weak : m_registrants)
    {
        std::shared_ptr<dispatcher_target> target = weak.lock();
        if (!target)
            continue;

        const auto bindings_list = target->get_bindings();
        if (!bindings_list)
            continue;

        for (auto table = bindings_list->rbegin(); table != bindings_list->rend(); ++table)
        {
            const int8_t can = (*table)->can_self_insert();
            if (can >= 0)
            {
                m_can_self_insert = can;
                return;
            }
        }
    }
}

} // namespace tib
