// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"

namespace tib {

// Use some invalid UTF8 bytes for special meanings.
constexpr uint8_t c_input_terminal_resize = 0xfe;
constexpr uint8_t c_input_terminal_eof = 0xfd;

void term_begin();
void term_end();
void term_sigint();

int32_t term_in();
int32_t term_in_peek();
bool term_in_avail(DWORD timeout=0);
bool term_push_macro_text(const char* text, size_t len=-1);

enum class mouse_input_mode { none, VT200, DRAG, ANY };
void enable_mouse_input(mouse_input_mode mode, bool sgr_encoding=true);

// Hooks for custom terminal read behavior.
typedef int32_t (*hook_term_in_func_t)();
typedef bool (*hook_term_in_avail_func_t)(DWORD timeout);
extern hook_term_in_func_t hook_term_in;
extern hook_term_in_avail_func_t hook_term_in_avail;

class pushed_input
{
public:
                        ~pushed_input() noexcept;
                        pushed_input() = default;
                        pushed_input(const pushed_input&) = delete;
    pushed_input&       operator=(const pushed_input&) = delete;
    bool                empty() const noexcept { return !m_count; }
    bool                push(uint8_t c) noexcept;
#ifdef _WIN32
    int32_t             push_utf16(WCHAR c) noexcept;
    int32_t             push_key_event(const KEY_EVENT_RECORD& record) noexcept;
#endif
    bool                push_invalid() noexcept;
    int32_t             peek() const noexcept;
    int32_t             read() noexcept;

private:
    bool                ensure_capacity(size_t num) noexcept;

    uint8_t*            m_data = nullptr;
    size_t              m_size = 0;
    size_t              m_head = 0;
    size_t              m_count = 0;

#ifdef _WIN32
    WCHAR               m_high_surrogate = 0;
    cstring             m_tmp_utf8;
#endif
};

} // namespace tib
