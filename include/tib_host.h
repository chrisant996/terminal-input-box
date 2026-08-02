// Copyright (c) 2026 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

namespace tib_host {

#ifdef _WIN32
void set_crt_locale_utf8();
void set_console_vt_input();
#endif

void set_no_exit_cleanup();

bool is_signaled();
void clear_signaled();

}
