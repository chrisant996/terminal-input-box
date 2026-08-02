// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_bindings.h"
#include <algorithm>
#include <assert.h>

namespace tib {

#ifdef USE_TRIE
struct key_trie;

struct key_trie_entry
{
    bool                m_is_trie;
    union {
        const binding_target* m_target;
        key_trie*       m_trie;
    };
};

struct key_trie
{
    key_trie_entry      entries[256];
};
#endif

key_table::~key_table()
{
#ifdef USE_TRIE
    free_trie();
#endif
}

void key_table::add(key_binding&& binding)
{
    const auto found = std::lower_bound(m_bindings.begin(), m_bindings.end(), binding.sequence, [](const key_binding& candidate, const cstring& sequence) {
        const size_t common_length = min(candidate.sequence.length(), sequence.length());
        const int comparison = memcmp(candidate.sequence.c_str(), sequence.c_str(), common_length);
        return comparison < 0 || (comparison == 0 && candidate.sequence.length() < sequence.length());
    });

    if (found != m_bindings.end() && found->sequence == binding.sequence)
        *found = std::move(binding);
    else
        m_bindings.insert(found, std::move(binding));

#ifdef USE_TRIE
    free_trie(); // Regenerate on demand.
#endif
}

void key_table::remove(const cstring& sequence)
{
    const auto found = std::lower_bound(m_bindings.begin(), m_bindings.end(), sequence, [](const key_binding& candidate, const cstring& sequence) {
        const size_t common_length = min(candidate.sequence.length(), sequence.length());
        const int comparison = memcmp(candidate.sequence.c_str(), sequence.c_str(), common_length);
        return comparison < 0 || (comparison == 0 && candidate.sequence.length() < sequence.length());
    });

    if (found != m_bindings.end() && found->sequence == sequence)
        m_bindings.erase(found);

#ifdef USE_TRIE
    free_trie(); // Regenerate on demand.
#endif
}

void key_table::clear()
{
#ifdef USE_TRIE
    free_trie();
#endif
    m_bindings.clear();
}

#ifdef USE_TRIE
const key_trie* key_table::get_trie()
{
    if (!m_trie)
    {
        m_trie = static_cast<key_trie*>(malloc(sizeof(*m_trie)));
        memset(m_trie, 0, sizeof(*m_trie));

        for (const auto& binding : m_bindings)
        {
            const size_t length = binding.sequence.length();
            const auto* const sequence = reinterpret_cast<const unsigned char*>(binding.sequence.c_str());
            assert(length);
            key_trie* node = m_trie;

            for (size_t i = 0; i < length; ++i)
            {
                auto& entry = node->entries[sequence[i]];
                if (i + 1 == length)
                {
                    assert(!entry.m_is_trie);
                    entry.m_target = &binding.target;
                }
                else
                {
                    if (!entry.m_is_trie)
                    {
                        key_trie* const child = static_cast<key_trie*>(malloc(sizeof(*child)));
                        memset(child, 0, sizeof(*child));
                        entry.m_trie = child;
                        entry.m_is_trie = true;
                    }
                    node = entry.m_trie;
                }
            }
        }
    }
    return m_trie;
}

void key_table::free_trie(key_trie* trie)
{
    for (auto& e : trie->entries)
    {
        if (e.m_is_trie)
        {
            free_trie(e.m_trie);
            free(e.m_trie);
        }
    }
}

void key_table::free_trie()
{
    if (m_trie)
    {
        free_trie(m_trie);
        m_trie = nullptr;
    }
}
#endif

void dispatcher::init(const key_table_list& tables)
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
#ifdef USE_TRIE
    // TODO:  Where does it get the trie from?
    // TODO:  How to handle multiple tries...?
    m_node = nullptr;
#endif
    m_target = nullptr;
    m_outcome = dispatch_outcome::miss;
}

dispatch_outcome dispatcher::step(char c)
{
#ifdef USE_TRIE
    // If the new character doesn't match any existing key binding, then
    // discard any sequence so far (as many *nix input drivers seem to do).
    if (!m_node)
    {
miss:
        assert(!m_target);
        m_sequence.set(&c, 1);
        return dispatch_outcome::miss;
    }

    const uint8_t uc = uint8_t(c);
    const key_trie_entry& e = m_node->entries[uc];

    if (e.m_is_trie)
    {
        assert(e.m_trie);
        m_node = e.m_trie;
        m_target = nullptr;
        m_sequence.append(&c, 1);
        return dispatch_outcome::more;
    }

    // TODO:  Reset m_node.
    m_target = e.m_target;
    if (!m_target)
        goto miss;

    return dispatch_outcome::match;
#else
    if (m_outcome != dispatch_outcome::more)
        reset();

    m_sequence.append(&c, 1);

    // Search they key tables in priority order (later entries overlay earlier
    // entries) looking for an exact match or a prefix match.
    bool is_prefix = false;
    for (auto table = m_tables.rbegin(); table != m_tables.rend(); ++table)
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

    m_sequence.set(&c, 1);
    m_outcome = dispatch_outcome::miss;
    return m_outcome;
#endif
}

} // namespace tib
