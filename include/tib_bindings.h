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
    none,
    func,
    macro,
};

typedef int32_t (*bindable_func_t)(tib::editor_context& ctx, int32_t key, const char* name);

class key_table;

class binding_target
{
    friend key_table;

public:
                        ~binding_target() noexcept = default;
                        binding_target() = default;
                        binding_target(bindable_func_t func, const char* text=nullptr) noexcept { set_func(func, text); }
                        binding_target(const char* text, size_t len=c_auto_length) noexcept { set_macro(text, len); }
                        binding_target(const binding_target& t) noexcept = default;
                        binding_target(binding_target&& t) noexcept = default;
    binding_target&     operator=(const binding_target& t) noexcept = default;
    binding_target&     operator=(binding_target&& t) noexcept = default;
    bool                operator==(const binding_target& t) const noexcept;

    binding_type        get_type() const noexcept { return m_type; }
    bindable_func_t     get_func() const noexcept { assert(m_type == binding_type::func); return m_func; }
    const char*         get_text() const noexcept { return m_text; }
    size_t              get_length() const noexcept { assert(m_type == binding_type::macro); return m_length; }

    void                clear() noexcept;
    void                set_func(bindable_func_t func, const char* name=nullptr) noexcept;
    void                set_macro(const char* text, size_t len=c_auto_length) noexcept;

protected:
    // Optimize dispatching key bindings by storing a pre-resolved encoding of
    // the operation.  If runtime cost didn't matter, it could be cleaner to
    // store just a target string (`foo` for bindable command name "foo", and
    // `"text"` for macro text "text") and resolve the target on demand.
    // PERF: Analyze actual performance cost of resolving targets on demand.
    binding_type        m_type = binding_type::none;
    bindable_func_t     m_func = nullptr;
    const char*         m_text = nullptr;   // Borrowed, not owned.
    size_t              m_length = 0;
};

class binding_target_copy : public binding_target
{
public:
                        ~binding_target_copy() = default;
                        binding_target_copy() = default;
                        binding_target_copy(const binding_target_copy& t) noexcept = default;
                        binding_target_copy(const binding_target& t) noexcept;
    binding_target_copy& operator=(const binding_target_copy& t) noexcept = default;
    binding_target_copy& operator=(const binding_target& t) noexcept;

private:
    cstring             m_owned_text;
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

    // FUTURE: Need some way to troubleshoot messed up bindings.
    bool                add(key_binding&& binding);
    bool                remove(const cstring& sequence);
    void                clear();

    auto                begin() const noexcept { return m_bindings.cbegin(); }
    auto                end() const noexcept { return m_bindings.cend(); }
    auto                cbegin() const noexcept { return m_bindings.cbegin(); }
    auto                cend() const noexcept { return m_bindings.cend(); }

    // FUTURE: provide an enumeration that filters to only key bindings that
    // match a prefix sequence?

private:
    std::vector<key_binding> m_bindings;
};

typedef std::vector<std::shared_ptr<key_table>> key_table_list;

enum dispatch_outcome { miss, self_insert, more, match };

class dispatcher
{
public:
                        ~dispatcher() = default;
                        dispatcher() = default;

    void                init(std::shared_ptr<const key_table_list> tables);

    void                reset();
    dispatch_outcome    step(char c, editor_context* ctx=nullptr);

    const cstring&      get_sequence() const { return m_sequence; }
    const binding_target* get_target() const { return m_target; }
    dispatch_outcome    get_outcome() const { return m_outcome; }

private:
    dispatch_outcome    step_internal(char c);

private:
    std::shared_ptr<const key_table_list> m_tables;
    cstring             m_sequence;
    const binding_target* m_target = nullptr;
    dispatch_outcome    m_outcome = dispatch_outcome::miss;
};

} // namespace tib
