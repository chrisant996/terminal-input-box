// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"

namespace tib {

extern const char c_hide_cursor[];
extern const char c_show_cursor[];

bool is_console();

size_t fits_in_wcwidth(const char* s, const size_t len, const uint16_t truncate_width, uint16_t* truncated_width);

uint16_t get_terminal_width();
void term_out(const char* s, size_t len=c_auto_length);

extern void (*hook_term_out)(const char* s, size_t len);

} // namespace tib
