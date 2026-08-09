// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include <assert.h>

#ifndef _WIN32
#include <time.h>
#endif

namespace tib {

size_t resolve_auto_length(size_t len, const char* s)
{
    return !s ? 0 : (len == c_auto_length) ? strlen(s) : len;
}

#ifdef _WIN32
size_t resolve_auto_length(size_t len, const WCHAR* s)
{
    return !s ? 0 : (len == c_auto_length) ? wcslen(s) : len;
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

template<>
const char* const cstring_t<char>::c_spaces = "                                ";

#ifdef _WIN32
template<>
const WCHAR* const cstring_t<WCHAR>::c_spaces = L"                                ";
#endif

template<>
const char* cstring_t<char>::c_str() const
{
    return m_text ? m_text : "";
}

#ifdef _WIN32
template<>
const WCHAR* cstring_t<WCHAR>::c_str() const
{
    return m_text ? m_text : L"";
}
#endif

#ifdef _WIN32
bool to_utf8(const WCHAR* s, size_t len, cstring_t<char>& out)
{
    out.clear();

    len = resolve_auto_length(len, s);
    if (len)
    {
        const int needed = WideCharToMultiByte(CP_UTF8, 0, s, int(len), nullptr, 0, nullptr, nullptr);
        if (needed <= 0)
            return false;
        if (!out.reserve(needed))
            return false;

        const int converted = WideCharToMultiByte(CP_UTF8, 0, s, int(len), out.reserve(0), int(out.capacity()), nullptr, nullptr);
        if (converted <= 0)
        {
            out.clear();
            return false;
        }

        out.set_length(converted);
    }

    return true;
}

bool to_utf16(const char* s, size_t len, cstring_t<WCHAR>& out)
{
    out.clear();

    len = resolve_auto_length(len, s);
    if (len)
    {
        const int needed = MultiByteToWideChar(CP_UTF8, 0, s, int(len), nullptr, 0);
        if (needed <= 0)
            return false;
        if (!out.reserve(needed))
            return false;

        const int converted = MultiByteToWideChar(CP_UTF8, 0, s, int(len), out.reserve(0), int(out.capacity()));
        if (converted <= 0)
        {
            out.clear();
            return false;
        }

        out.set_length(converted);
    }

    return true;
}
#endif

class high_resolution_clock
{
public:
                        high_resolution_clock();
    double              elapsed() const;
private:
#ifdef _WIN32
    double              m_freq;
    int64_t             m_start;
#else
    double              m_start;
#endif
};

high_resolution_clock::high_resolution_clock()
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
    if (QueryPerformanceFrequency(&freq) &&
        QueryPerformanceCounter(&start) &&
        freq.QuadPart)
    {
        m_freq = double(freq.QuadPart);
        m_start = start.QuadPart;
    }
    else
    {
        m_freq = 0;
        m_start = 0;
    }
#else
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    m_start = double(start.tv_sec) + (start.tv_nsec * 1e-9);
#endif
}

double high_resolution_clock::elapsed() const
{
#ifdef _WIN32
    if (!m_freq)
        return -1;

    LARGE_INTEGER current;
    if (!QueryPerformanceCounter(&current))
        return -1;

    const int64_t delta = current.QuadPart - m_start;
    if (delta < 0)
        return -1;

    const double result = double(delta) / m_freq;
    return result;
#else
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);

    const double now = double(current.tv_sec) + (current.tv_nsec * 1e-9);
    return now - m_start;
#endif
}

static high_resolution_clock s_clock;

double clock()
{
    return s_clock.elapsed();
}

bool getenv(const char* name, cstring& out)
{
#ifdef _WIN32
    cstring_t<WCHAR> wname;
    cstring_t<WCHAR> wout;

    out.clear();

    if (!to_utf16(name, -1, wname))
        return false;

    const DWORD needed = GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
    if (!needed)
        return false;

    if (!wout.reserve(needed))
        return false;

    const DWORD used = GetEnvironmentVariableW(wname.c_str(), wout.reserve(0), DWORD(wout.capacity()));
    if (!used)
        return false;

    wout.set_length(used);
    return to_utf8(wout.c_str(), wout.length(), out);
#else
    // TODO:  Alternative Linux implementation.
#endif
}

} // namespace tib
