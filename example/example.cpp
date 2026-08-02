// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include <stdio.h>

#include "tib.h"
#include "tib_host.h"
#include <assert.h>

int main(int argc, const char** argv)
{
    --argc, ++argv;

    tib_host::set_crt_locale_utf8();
    tib_host::set_console_vt_input();

    tib::input_box tib;

    return 0;
}
