// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib.h"
#include "wcwidth.h"
#include <assert.h>

namespace tib {

void selection_state::set_selection(textpos_t anchor, textpos_t caret)
{
    assert(anchor != static_cast<textpos_t>(-1));
    assert(caret != static_cast<textpos_t>(-1));
    if (anchor != m_anchor || caret != m_caret)
        m_dirty = true;
    m_anchor = anchor;
    m_caret = caret;
}

bool selection_state::clear_selection()
{
    if (!has_selection())
        return false;
    set_selection(m_caret, m_caret);
    return true;
}

} // namespace tib
