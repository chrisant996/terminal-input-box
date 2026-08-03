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
size_t resolve_auto_length(size_t len, const WCHAR* s)
{
    return (len == c_auto_length) ? wcslen(s) : len;
}
#endif

int __vsnprintf(char* buffer, size_t len, const char* format, va_list args)
{
    return _vsnprintf_s(buffer, len, _TRUNCATE, format, args);
}

#ifdef _WIN32
int __vsnprintf(WCHAR* const buffer, size_t const len, const WCHAR* const format, va_list args)
{
    return _vsnwprintf_s(buffer, len, _TRUNCATE, format, args);
}
#endif

} // namespace tib
