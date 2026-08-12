// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_output.h"
#include "wcwidth.h"
#include <assert.h>

namespace tib {

void (*hook_term_out)(const char* s, size_t len) = nullptr;

// TODO: Initialize appropriately.
bool g_color_emoji = true;

const char c_hide_cursor[] = "\x1b[?25l";
const char c_show_cursor[] = "\x1b[?25h";

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

// TODO: Abstract behind a terminal object.
coord get_terminal_size()
{
    coord size = { 80, 25 };
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hout, &csbi))
    {
        size.x = csbi.dwSize.X;
        size.y = csbi.dwSize.Y;
    }
#else
    // TODO:  Alternative Linux implementation.
#endif
    return size;
}

void term_out(const char* s, size_t len)
{
    len = resolve_auto_length(len, s);

    if (hook_term_out)
        return hook_term_out(s, len);

#ifdef _WIN32
    static cstring_t<WCHAR> s_buffer;
    if (!to_utf16(s, len, s_buffer))
        return;

    DWORD written;
    if (is_console())
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), s_buffer.c_str(), DWORD(s_buffer.length()), &written, nullptr);
    else
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s_buffer.c_str(), DWORD(s_buffer.length()), &written, nullptr);
#else
    fwrite(s, resolve_auto_length(len, s), 1, stdout);
#endif
}

} // namespace tib
