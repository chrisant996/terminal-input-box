// Copyright (c) 2026 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

namespace tib_host {

#ifdef _WIN32
void set_crt_locale_utf8();
void set_console_vt_input();
#endif

bool is_signaled();
void clear_signaled();

class auto_terminal_init
{
public:
                        ~auto_terminal_init();
                        auto_terminal_init();

    void                set_no_exit_cleanup() { m_exit_cleanup = false; }

private:
    void                restore();

#ifdef _WIN32
    static BOOL WINAPI  BreakHandler(DWORD CtrlType);
#endif

private:
#ifdef _WIN32
    DWORD               m_orig_modes[3];
    bool                m_restore_modes = false;
#else
    // TODO-LINUX: POSIX sigaction alternative.
#endif
    bool                m_exit_cleanup = true;
};

} // namespace tib_host
