// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_input.h"
#include <conio.h>
#include <assert.h>

namespace tib {

int32_t (*hook_term_in)() = nullptr;

int32_t term_in()
{
    // TODO:  Pushed input.
    // TODO:  Macro playback.

    if (hook_term_in)
        return hook_term_in();

    return uint8_t(_getch());
}

} // namespace tib
