// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"

#include <memory>
#include <vector>
#include <assert.h>

namespace tib {

class editor_context;

enum class binding_type : uint8_t
{
    func,
    custom,
    macro,
};

typedef int32_t (*bindable_func_t)(tib::editor_context& ctx, const char* name);

#ifdef USE_TRIE
struct key_trie;
#endif

class key_table;

class binding_target
{
    friend key_table;

    union binding_storage
    {
                        ~binding_storage() {}
                        binding_storage() {}

        bindable_func_t m_func;
        const char*     m_custom;
        cstring         m_macro;
    };

public:
                        ~binding_target();
                        binding_target() = delete;
                        binding_target(const binding_target& t);
                        binding_target(binding_target&& t);
                        binding_target& operator=(const binding_target& t);
                        binding_target& operator=(binding_target&& t);

    binding_type        type() const { return m_type; }
    bindable_func_t     get_func() const { assert(m_type == binding_type::func); return m_storage.m_func; }
    const char*         get_custom() const { assert(m_type == binding_type::custom); return m_storage.m_custom; }
    const cstring&      get_macro() const { assert(m_type == binding_type::macro); return m_storage.m_macro; }

    void                set_func(bindable_func_t func);
    void                set_custom(const char* custom);
    void                set_macro(const char* macro, uint16_t len);

private:
    // Optimize dispatching key bindings by storing a pre-resolved encoding of
    // the operation.  If runtime cost didn't matter, it could be cleaner to
    // store just a target string (`foo` for bindable command name "foo", and
    // `"text"` for macro text "text") and resolve the target on demand.
    binding_type        m_type;
    binding_storage     m_storage;
};

struct key_binding
{
                        ~key_binding() = default;

    cstring             sequence;
    binding_target      target;
};

class key_table : public std::enable_shared_from_this<key_table>
{
    friend class dispatcher;

public:
                        ~key_table();
                        key_table() = default;

    void                add(key_binding&& binding);
    void                remove(const cstring& sequence);
    void                clear();

    // TODO:  Enumerate m_bindings to be able to report current available key
    // bindings.
    // TODO:  Optionally filter the enumeration to only key bindings that
    // match a prefix sequence.

#ifdef USE_TRIE
    const key_trie*     get_trie();
#endif

private:
#ifdef USE_TRIE
    void                free_trie();
    void                free_trie(key_trie* node);
#endif

    std::vector<key_binding> m_bindings;
#ifdef USE_TRIE
    key_trie*           m_trie = nullptr;
#endif
};

typedef std::vector<std::shared_ptr<key_table>> key_table_list;

enum dispatch_outcome { miss, more, match };

class dispatcher
{
public:
                        ~dispatcher() = default;
                        dispatcher() = default;

    void                init(const key_table_list& tables);

    void                reset();
    dispatch_outcome    step(char c);

    const cstring&      get_sequence() const { return m_sequence; }
    const binding_target* get_target() const { return m_target; }
    dispatch_outcome    get_outcome() const { return m_outcome; }

private:
    key_table_list      m_tables;
    cstring             m_sequence;
#ifdef USE_TRIE
    // TODO:  Need one m_node per key_table in m_tables.
    const key_trie*     m_node = nullptr;
#endif
    const binding_target* m_target = nullptr;
    dispatch_outcome    m_outcome = dispatch_outcome::miss;
};

}
