// Copyright (c) 2025 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "signaled.h"
#include "tib_output.h"

static bool s_signaled = false;

namespace tib_host {

class CRestoreConsole
{
public:
    CRestoreConsole();
    ~CRestoreConsole();

    void                SetNoExitCleanup() { m_exit_cleanup = false; }

private:
    void                Restore();
#ifdef _WIN32
    static BOOL WINAPI  BreakHandler(DWORD CtrlType);
#endif

private:
#ifdef _WIN32
    DWORD               m_orig_modes[3];
    bool                m_restore_modes = false;
#else
    // TODO:  POSIX sigaction alternative.
#endif
    bool                m_exit_cleanup = true;
};

static CRestoreConsole s_restoreConsole;

void set_no_exit_cleanup()
{
    s_restoreConsole.SetNoExitCleanup();
}

bool is_signaled()
{
    return s_signaled;
}

void clear_signaled()
{
    s_signaled = false;
}

CRestoreConsole::CRestoreConsole()
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

    SetConsoleMode(handles[1], m_orig_modes[1]|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleMode(handles[2], m_orig_modes[2]|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    // TODO:  POSIX sigaction alternative.
#endif
}

CRestoreConsole::~CRestoreConsole()
{
    Restore();
}

void CRestoreConsole::Restore()
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
        // TODO:  POSIX sigaction alternative.
#endif
}

#ifdef _WIN32
BOOL CRestoreConsole::BreakHandler(DWORD CtrlType)
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
// TODO:  POSIX sigaction alternative.
#endif

} // namespace tib_host
