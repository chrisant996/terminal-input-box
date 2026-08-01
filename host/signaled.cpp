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
    static BOOL WINAPI  BreakHandler(DWORD CtrlType);

private:
#ifdef _WIN32
    HANDLE              m_hout = 0;
    DWORD               m_mode_out;
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
#ifdef _WIN32
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hout && !GetConsoleMode(hout, &m_mode_out))
        hout = 0;
    if (!hout)
        return;

    m_hout = hout;

    SetConsoleCtrlHandler(BreakHandler, true);

    if (hout)
        SetConsoleMode(hout, m_mode_out|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
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
#ifdef _WIN32
    if (m_hout)
    {
        if (m_exit_cleanup)
        {
            DWORD dummy;
            if (m_hout && GetConsoleMode(m_hout, &dummy))
                tib::term_out("\x1b[m", 3);
        }
        SetConsoleMode(m_hout, m_mode_out);
    }
    m_hout = 0;
#else
    if (m_exit_cleanup)
    {
        // TODO:  POSIX sigaction alternative.
        // fputs("\x1b[m", stdout);
    }
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
