// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"

namespace tib {

// TODO: Input driver abstraction.
// TODO: How to represent failure?
// TODO: How to represent EOF?

int32_t term_in();
int32_t term_in_peek();
bool term_in_avail();
bool term_push_macro_text(const char* text, size_t len=-1);

extern int32_t (*hook_term_in)();

} // namespace tib
