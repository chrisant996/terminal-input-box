// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et sw=4 cino={0s:

#pragma once

#include "tib_base.h"
#include <vector>

namespace {

enum class binding_type : uint8_t
{
    func,
    custom,
    macro,
};

class binding_target
{
public:
                        ~binding_target();
                        binding_target() = delete;
                        binding_target(const binding_target& t);
                        binding_target(binding_target&& t);
                        binding_target& operator=(const binding_target& t);
                        binding_target& operator=(binding_target&& t);

	binding_type        type() const { return m_type; }
    bindable_func_t     get_func() const { assert(m_type == binding_type::func); return m_func; }
    const char*         get_custom() const { assert(m_type == binding_type::custom); return m_custom; }
    const cstring&      get_macro() const { assert(m_type == binding_type::macro); return m_macro; }

    void                set_func(bindable_func_t func);
    void                set_custom(const char* custom);
    void                set_macro(const char* macro, uint16_t len);

private:
    // Optimize dispatching key bindings by storing a pre-resolved encoding of
    // the operation.  If runtime cost didn't matter, it could be cleaner to
    // store just a target string (`foo` for bindable command name "foo", and
    // `"text"` for macro text "text") and resolve the target on demand.
    binding_type        m_type;
    union storage {
        ~storage() {};
        storage() {};
        bindable_func_t m_func;
        const char*     m_custom;
        cstring         m_macro;
    };
};

struct key_binding
{
	                    ~key_binding() = default;

	cstring             sequence;
	binding_target      target;
};

class key_table
{
public:
                        ~key_table() = default;
                        key_table() = default;

    void                add(key_binding&& binding);
    void                remove(const cstring& sequence);
    void                clear();

private:
    std::vector<key_binding> m_bindings;    // Sorted by sequence.
};

}
