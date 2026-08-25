// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"

namespace tib {

constexpr int32_t c_input_terminal_resize = -2;

void term_begin();
void term_end();

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

} // namespace tib
