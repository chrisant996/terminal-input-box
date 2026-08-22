// Copyright (c) 2026 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_host.h"
#include "tib_output.h"

static bool s_signaled = false;

namespace tib_host {

bool is_signaled()
{
    return s_signaled;
}

void clear_signaled()
{
    s_signaled = false;
}

auto_terminal_init::~auto_terminal_init()
{
    restore();
}

auto_terminal_init::auto_terminal_init()
{
    if (!tib::is_console())
    {
no_cleanup:
        m_exit_cleanup = false;
        return;
    }

#ifdef _WIN32
    HANDLE handles[3] = {};
    for (int i = 0; i < 3; ++i)
    {
        handles[i] = GetStdHandle(STD_INPUT_HANDLE - i);
        if (!handles[i])
            goto no_cleanup;
        if (!GetConsoleMode(handles[i], &m_orig_modes[i]))
            goto no_cleanup;
    }

    m_restore_modes = true;

    SetConsoleCtrlHandler(BreakHandler, true);

    // TODO: ENABLE_WINDOW_INPUT for terminal resize.
    // TODO: ENABLE_MOUSE_INPUT for mouse input, but use the conditional approach from Clink.
    SetConsoleMode(handles[0], m_orig_modes[0]&~(ENABLE_PROCESSED_INPUT|ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT));
    SetConsoleMode(handles[1], m_orig_modes[1]|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleMode(handles[2], m_orig_modes[2]|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    // TODO-LINUX: POSIX sigaction alternative.
#endif
}

void auto_terminal_init::restore()
{
    if (m_exit_cleanup)
    {
        tib::term_out("\x1b[m", 3);
        m_exit_cleanup = false;
    }

#ifdef _WIN32
    if (m_restore_modes)
    {
        m_restore_modes = false;
        for (int i = 0; i < 3; ++i)
        {
            HANDLE h = GetStdHandle(STD_INPUT_HANDLE - i);
            if (h)
                SetConsoleMode(h, m_orig_modes[i]);
        }
    }
#else
        // TODO-LINUX: POSIX sigaction alternative.
#endif
}

#ifdef _WIN32
BOOL auto_terminal_init::BreakHandler(DWORD CtrlType)
{
    if (CtrlType == CTRL_C_EVENT || CtrlType == CTRL_BREAK_EVENT)
    {
        // Do not terminate on Ctrl-C or Ctrl-Break.
        s_signaled = true;
        return true;
    }
    return false;
}
#else
    // TODO-LINUX: POSIX sigaction alternative.
#endif

} // namespace tib_host
