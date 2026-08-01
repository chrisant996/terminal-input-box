// Copyright (c) 2026 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

/*
 * Restore console mode and attributes on exit or ^C or ^Break.
 */

namespace tib_host {

void set_no_exit_cleanup();

bool is_signaled();
void clear_signaled();

}
