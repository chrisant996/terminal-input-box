// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_output.h"
#include "tib_display.h"
#include "wcwidth.h"
#include <assert.h>

namespace tib {

void display_lines::clear()
{
    m_faces.clear();
    m_text.clear();
    m_pos = 0;
    m_change_counter = 0;

    m_lines.clear();
    m_cursor = { -1, -1 };
}

void display_manager::init(layout_info* layout)
{
    m_layout = layout;
    m_old.clear();
    m_new.clear();
}

bool display_manager::set(const cstring& text, const cstring& faces, uint16_t pos, uint32_t change_counter)
{
    if (change_counter == m_old.m_change_counter && pos == m_old.m_pos)
    {
        assert(text == m_old.m_text);
        assert(faces == m_old.m_faces);
        return true;
    }

    display_lines tmp;
    tmp.m_change_counter = change_counter;
    if (!tmp.m_faces.set(faces) || !tmp.m_text.set(text))
        return false;
    tmp.m_pos = pos;

    // TODO: build tmp.m_lines.
    // TODO: set tmp.m_cursor.

    m_new = std::move(tmp);
    return true;
}

bool display_manager::display()
{
    // TODO: if !m_new.m_change_counter return true;
    // TODO: if m_new.m_change_counter == m_old.m_change_counter && m_new.m_pos == m_old.m_pos return true;

    // TODO: display m_new.
    // TODO: transfer m_new to m_old.
    return false;
}

} // namespace tib
