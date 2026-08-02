// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "tib_base.h"

namespace tib {

// TODO:  Input driver abstraction.

char term_in();

extern char (*hook_term_in)();

} // namespace tib
