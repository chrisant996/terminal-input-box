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

key_table::~key_table()
{
    free_trie();
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

    free_trie(); // Regenerate on demand.
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

    free_trie(); // Regenerate on demand.
}

void key_table::clear()
{
    free_trie();
    m_bindings.clear();
}

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

} // namespace tib
