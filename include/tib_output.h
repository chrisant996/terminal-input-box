// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "maybe_windows.h"
#include "tib_base.h"

namespace tib {

size_t resolve_auto_length(size_t len, const char* s);

#ifdef _WIN32
size_t to_utf16(const char* s, size_t len, WCHAR*& out, size_t& capacity);
void set_console_vt_input();
#endif

bool is_console();
void term_out(const char* s, size_t len=c_auto_length);

extern void (*hook_term_out)(const char* s, size_t len);

} // namespace tib
