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

class key_table;

class binding_target
{
    friend key_table;

public:
                        ~binding_target() noexcept = default;
                        binding_target() = default;
                        binding_target(binding_type type, const char* text, size_t len=c_auto_length) noexcept;
                        binding_target(const binding_target& t) noexcept = default;
                        binding_target(binding_target&& t) noexcept = default;
    binding_target&     operator=(const binding_target& t) noexcept = default;
    binding_target&     operator=(binding_target&& t) noexcept = default;
    bool                operator==(const binding_target& t) const noexcept;
    bool                is_func_name(const char* name) const noexcept;

    binding_type        get_type() const noexcept { return m_type; }
    const char*         get_text() const noexcept { return m_text; }
    size_t              get_length() const noexcept { assert(m_type == binding_type::macro); return m_length; }

    void                clear() noexcept;
    void                set_func(const char* name) noexcept;
    void                set_macro(const char* text, size_t len=c_auto_length) noexcept;

protected:
    // Optimize dispatching key bindings by storing a pre-resolved encoding of
    // the operation.  If runtime cost didn't matter, it could be cleaner to
    // store just a target string (`foo` for bindable command name "foo", and
    // `"text"` for macro text "text") and resolve the target on demand.
    // PERF: Analyze actual performance cost of resolving targets on demand.
    binding_type        m_type = binding_type::none;
    const char*         m_text = nullptr;   // Borrowed, not owned.
    size_t              m_length = 0;
};

binding_target binding_target_func(const char* name);
binding_target binding_target_macro(const char* text, size_t len=c_auto_length);

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
                        key_table(int8_t can=-1) noexcept : m_can_self_insert(can) {}

    // FUTURE: Need some way to troubleshoot messed up bindings.
    bool                add(key_binding&& binding);
    bool                remove(const cstring& sequence);
    void                clear();

    int8_t              can_self_insert() const noexcept { return m_can_self_insert; }
    void                set_can_self_insert(int8_t can=true) noexcept { m_can_self_insert = can; }

    auto                begin() const noexcept { return m_bindings.cbegin(); }
    auto                end() const noexcept { return m_bindings.cend(); }
    auto                cbegin() const noexcept { return m_bindings.cbegin(); }
    auto                cend() const noexcept { return m_bindings.cend(); }

    // FUTURE: provide an enumeration that filters to only key bindings that
    // match a prefix sequence?

private:
    std::vector<key_binding> m_bindings;
    int8_t              m_can_self_insert = -1;
};

typedef std::vector<std::shared_ptr<key_table>> key_table_list;

enum class dispatch_outcome { miss, self_insert, more, match };

class dispatcher_target : public std::enable_shared_from_this<dispatcher_target>
{
public:
    std::shared_ptr<const key_table_list> get_bindings() const;
    void                set_bindings(std::shared_ptr<const key_table_list> bindings);

    // TODO: someone should get a crack at handling/redirecting key sequences
    // that aren't covered by the current key_table_list.

    // The dispatch() callback is called by dispatcher::step() in two cases:
    //  1.  The input sequence matched a key binding, in which case binding
    //      points at the matched key binding.
    //  2.  A key_table has self-insert enabled and the input sequence is a
    //      single self-insert character, in which case binding is nullptr and
    //      key is the character to be inserted.
    virtual int32_t     dispatch(const cstring& sequence, int32_t key, const binding_target* binding) noexcept = 0;

private:
    std::shared_ptr<const key_table_list> m_bindings;
};

class dispatcher
{
public:
                        ~dispatcher() = default;
                        dispatcher() = default;

    void                clear_targets();
    void                add_target(std::weak_ptr<dispatcher_target> target);

    void                reset();
    dispatch_outcome    step(char c);

    // TODO: Instead make this a binding_resolver that yields a
    // resolved_binding, and have a resolved_binding::dispatch method that
    // forwards to dispatcher_target::dispatch.
    const cstring&      get_sequence() const { return m_sequence; }
    const binding_target* get_binding_target() const { return m_binding_target; }
    std::weak_ptr<dispatcher_target> get_dispatcher_target() const { return m_dispatcher_target; }
    dispatch_outcome    get_outcome() const { return m_outcome; }

private:
    dispatch_outcome    step_internal(char c);
    void                maybe_recalc_can_self_insert();

private:
    std::vector<std::weak_ptr<dispatcher_target>> m_registrants;
    bool                m_recalc_can_self_insert = true;
    int8_t              m_can_self_insert = -1;

    cstring             m_sequence;
    const binding_target* m_binding_target = nullptr;
    std::weak_ptr<dispatcher_target> m_dispatcher_target;
    dispatch_outcome    m_outcome = dispatch_outcome::miss;
};

std::shared_ptr<tib::key_table_list> make_default_key_table();
inline bool is_self_insertable(int32_t key) { return (key >= ' ' && key <= 0xff); }

} // namespace tib
