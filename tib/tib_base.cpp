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

}
