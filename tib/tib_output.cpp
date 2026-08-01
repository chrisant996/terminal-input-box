// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include <assert.h>

namespace tib {

size_t resolve_auto_length(size_t len, const char* s)
{
    return (len == c_auto_length) ? strlen(s) : len;
}

#ifdef _WIN32
size_t to_utf16(const char* s, size_t len, WCHAR*& out, size_t& capacity)
{
    len = resolve_auto_length(len, s);
    const int needed = len ? MultiByteToWideChar(CP_UTF8, 0, s, int(len), nullptr, 0) : 0;
    if (len && !needed)
        return false;

    if (needed >= capacity)
    {
        WCHAR* tmp = static_cast<WCHAR*>(malloc(needed + 1));
        if (!tmp)
            return false;
        out = tmp;
        capacity = needed + 1;
    }

    if (!len)
    {
        out[0] = 0;
        return 0;
    }

    const int converted = MultiByteToWideChar(CP_UTF8, 0, s, int(len), out, int(capacity));
    if (!converted)
    {
        out[0] = 0;
        return false;
    }
    out[converted] = 0;
    return converted;
}
#endif

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

void raw_term_out(const char* s, size_t len)
{
#ifdef _WIN32
    static WCHAR* s_buffer = nullptr;
    static size_t s_capacity = 0;

    len = resolve_auto_length(len, s);
    const size_t converted = to_utf16(s, len, s_buffer, s_capacity);

    DWORD written;
    if (is_console())
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), s_buffer, DWORD(converted), &written, nullptr);
    else
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s_buffer, DWORD(converted), &written, nullptr);
#else
    fwrite(s, resolve_auto_length(len, s), 1, stdout);
#endif
}

} // namespace tib
