// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_output.h"
#include "tib_termcap.h"
#include "wcwidth.h"
#include <assert.h>

namespace tib {

void (*hook_term_out)(const char* s, size_t len) = nullptr;
void (*hook_term_ding)() = nullptr;

static bool is_console_raw()
{
    DWORD mode;
    return !!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
}

bool is_console()
{
    static const bool c_is_console = is_console_raw();
    return c_is_console;
}

size_t fits_in_wcwidth(const char* s, const size_t len, const uint16_t truncate_width, uint16_t* truncated_width)
{
    uint16_t length_fits = 0;
    uint16_t width_fits = 0;
    uint16_t width = 0;

    wcwidth_iter iter(s, len);
    while (true)
    {
        const char32_t c = iter.next();
        if (!c)
            break;

        const uint16_t w = iter.character_wcwidth_onectrl();
        width += w;

        if (width > truncate_width)
        {
            if (truncated_width)
                *truncated_width = width_fits;
            return length_fits;
        }

        length_fits = unsigned(iter.get_pointer() - s);
        width_fits = width;
    }

    if (truncated_width)
        *truncated_width = width_fits;
    return length_fits;
}

void term_out(const char* s, size_t len)
{
    len = resolve_auto_length(len, s);

    if (hook_term_out)
        return hook_term_out(s, len);

#ifdef _WIN32
    DWORD written;
    if (is_console())
    {
        static cstring_t<WCHAR> s_buffer;
        if (!to_utf16(s, len, s_buffer))
            return;
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), s_buffer.c_str(), DWORD(s_buffer.length()), &written, nullptr);
    }
    else
    {
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, DWORD(len), &written, nullptr);
    }
#else
    fwrite(s, resolve_auto_length(len, s), 1, stdout);
#endif
}

void ding()
{
    if (hook_term_ding)
        return hook_term_ding();

    term_out("\007", 1);
}

} // namespace tib
