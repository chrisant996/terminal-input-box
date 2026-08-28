// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"
#include "tib_terminal.h"

namespace tib {

int32_t term_in();
int32_t term_in_peek();
bool term_in_avail(DWORD timeout=0);
bool term_push_macro_text(const char* text, size_t len=-1);
bool enable_mouse_input(mouse_input_mode mode, bool sgr_encoding=true);

} // namespace tib
