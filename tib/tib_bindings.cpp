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

bool g_optimize_self_insert = true;

bool binding_target::operator==(const binding_target& t) const noexcept
{
    if (m_type != t.m_type || m_func != t.m_func)
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

bool binding_target::is_func_ptr(bindable_func_t func) const noexcept
{
    return (func && m_type == binding_type::func && m_func == func);
}

bool binding_target::is_func_name(const char* name) const noexcept
{
    return (name && m_type == binding_type::func && m_text && strcmp(name, m_text) == 0);
}

void binding_target::clear() noexcept
{
    m_type = binding_type::none;
    m_func = nullptr;
    m_text = nullptr;
    m_length = 0;
}

void binding_target::set_func(bindable_func_t func, const char* text) noexcept
{
    assert(func);
    m_type = binding_type::func;
    m_func = func;
    m_text = text;
    m_length = 0;
}

void binding_target::set_macro(const char* text, size_t len) noexcept
{
    assert(text);
    len = resolve_auto_length(len, text);
    m_type = binding_type::macro;
    m_func = nullptr;
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
        set_func(t.get_func(), m_owned_text.c_str());
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

void dispatcher::init(std::shared_ptr<const key_table_list> tables)
{
    // Copy the key_table_list.  This isolates the dispatcher from changes to
    // the caller's key_table_list.  Compromise:  however, it does not isolate
    // from changes to a given key_table in the list.
    m_tables = tables;
    reset();
}

void dispatcher::reset()
{
    m_sequence.clear();
    m_target = nullptr;
    m_outcome = dispatch_outcome::miss;
}

dispatch_outcome dispatcher::step(char c, editor_context* ctx)
{
    dispatch_outcome outcome = step_internal(c);
    if (outcome == dispatch_outcome::miss && !(c & 0xffffff00))
        outcome = dispatch_outcome::self_insert;

    if (ctx)
    {
        switch (outcome)
        {
        case dispatch_outcome::self_insert:
            if (g_optimize_self_insert)
            {
                int32_t peek = term_in_peek();
                if (peek && !(peek & 0xffffff00))
                {
                    ctx->begin_undo_group();
                    ctx->insert_char(c);
                    while (peek && !(peek & 0xffffff00))
                    {
                        assert(term_in() == peek);
                        ctx->insert_char(char(peek));
                        peek = term_in_peek();
                    }
                    ctx->end_undo_group();
                    break;
                }
            }
            ctx->insert_char(c);
            break;
        case dispatch_outcome::match:
            {
                const auto target = get_target();
                assert(target);
                ctx->do_binding_target(target, c);
            }
            break;
        }
    }

    return outcome;
}

dispatch_outcome dispatcher::step_internal(char c)
{
    if (m_outcome != dispatch_outcome::more)
        reset();

    m_sequence.append(&c, 1);

    // Search the key tables in priority order (later tables overlay earlier
    // tables) looking for an exact match or a prefix match.
    bool is_prefix = false;
    for (auto table = m_tables.get()->rbegin(); table != m_tables.get()->rend(); ++table)
    {
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
                m_target = &found->target;
                m_outcome = dispatch_outcome::match;
                return m_outcome;
            }
            is_prefix = true;
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

    m_outcome = dispatch_outcome::miss;
    return m_outcome;
}

} // namespace tib
