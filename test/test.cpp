// Copyright (c) 2021,2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "test.h"

#ifdef _WIN32
// For compatibility with Windows 8.1 SDK.
#if !defined( ENABLE_VIRTUAL_TERMINAL_PROCESSING )
# define ENABLE_VIRTUAL_TERMINAL_PROCESSING  0x0004
#elif ENABLE_VIRTUAL_TERMINAL_PROCESSING != 0x0004
# error ENABLE_VIRTUAL_TERMINAL_PROCESSING must be 0x0004
#endif
#endif

namespace test {

class high_resolution_clock
{
public:
                        high_resolution_clock();
    double              elapsed() const;
private:
    double              m_freq;
    int64_t             m_start;
};

high_resolution_clock::high_resolution_clock()
{
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
}

double high_resolution_clock::elapsed() const
{
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
}

static high_resolution_clock s_clock;

double clock()
{
    return s_clock.elapsed();
}

void colors::initialize()
{
#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif
    DWORD type;
    DWORD data;
    DWORD size;
    LSTATUS status = RegGetValueA(HKEY_CURRENT_USER, "Console", "ForceV2", RRF_RT_REG_DWORD, &type, &data, &size);
    if (status != ERROR_SUCCESS ||
        type != REG_DWORD ||
        size != sizeof(data) ||
        data != 0)
    {
        DWORD mode;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleMode(h, &mode))
        {
#if 0
            // REDUNDANT:  auto_terminal_init in signaled.cpp sets the mode.
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
            *get_colored_storage() = true;
        }
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif
}

} // namespace test
