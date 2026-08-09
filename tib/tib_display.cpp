// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_buffer.h"
#include "tib_output.h"
#include "tib_display.h"
#include "tib_context.h"
#include "wcwidth.h"
#include <assert.h>

namespace tib {

const border_definition c_light_border =
{
    "┌",
    "─",
    "┐",
    "│",
    "│",
    "└",
    "─",
    "┘",
};

void display_lines::clear()
{
    m_faces.clear();
    m_text.clear();
    m_pos = 0;
    m_left = 0;
    m_selection_length = 0;
    m_change_counter = 0;

    m_lines.clear();
    m_cursor = { -1, -1 };
}

void display_manager::init_layout(layout_info* layout)
{
    m_layout = layout;
    m_displayed.clear();
    invalidate();
    invalidate_border();
}

void display_manager::init_buffer(const input_buffer* buffer)
{
    m_buffer = buffer;
    invalidate();
}

void display_manager::init_style(const style_info* style)
{
    m_style = style;
    invalidate();
    invalidate_border();
}

void display_manager::init_faces(const face_definitions* face_defs)
{
    m_face_defs = face_defs;
    invalidate();
}

std::shared_ptr<const color_table> display_manager::get_color_table() const
{
    return m_colors;
}

void display_manager::set_color_table(std::shared_ptr<const color_table> colors)
{
    m_colors = colors;
    invalidate();
    invalidate_border();
}

uint32_t display_manager::get_effective_max_width() const
{
    assert(m_layout);
    if (!m_layout)
        return 0;

    const uint16_t term_width = get_terminal_width();

    const border_definition* b = m_style ? m_style->border : nullptr;
    // TODO: cache border cell_count metrics.
    const uint16_t b_left_width = (b && b->has_left()) ? cell_count(b->left, -1) : 0;
    const uint16_t b_right_width = (b && b->has_right()) ? cell_count(b->right, -1) : 0;
    const uint16_t extra_border_width = b_left_width + b_right_width;

    uint32_t max_width = m_layout->max_width;
    if (uint32_t(m_layout->origin.x) + max_width + extra_border_width >= term_width)
    {
        if (term_width <= m_layout->origin.x + extra_border_width)
            return 0;
        max_width = uint32_t(term_width - (m_layout->origin.x + extra_border_width - 1));
        if (int32_t(max_width) < 8)
            return 0;
    }
    return max_width;
}

bool display_manager::display()
{
    assert(m_layout);
    assert(m_buffer);
    if (!m_layout || !m_buffer || !m_buffer->get_change_counter())
        return false;   // Nothing to display.

    // Format content into display structures.
// TODO: allow host to add their own display_line rows; that will simplify
// showing/clearing their extra rows (something Clink still struggles with).
    display_lines tmp;
    if (!build(tmp))
        return false;   // Nothing changed since list display (or OOM error).

    if (m_style && m_style->border)
    {
        if (tmp.m_lines.size() != m_displayed.m_lines.size())
            m_border_dirty = true;

        // TODO: cache border metrics.
        const border_definition& b = *m_style->border;
        m_layout->inner_offset.y = b.has_top() ? 1 : 0;
        m_layout->inner_offset.x = b.has_left() ? cell_count(b.left, -1) : 0;
    }

    // If origin not set yet, then pin it "here".
    if (m_layout->origin.x <= 0)
    {
        m_layout->origin.x = 1;
        m_layout->cursor = { 0, 0 };    // Cursor is relative to origin.
        term_out("\r", 1);
    }

    cstring out;
    coord cursor = m_layout->cursor;
    const uint16_t term_width = get_terminal_width();
    const uint32_t max_width = get_effective_max_width();

    if (cursor.x < 0 && cursor.y < 0)
        cursor = { -1, 0 };             // -1 forces move_to_column.
// BUGBUG: cursor location initialization isn't thorough or fully correct yet.

    // TODO:  Encapsulate terminal codes behind some termcap layer.

    out.set(c_hide_cursor);

    // Draw border if needed.
    if (m_style && m_style->border && m_border_dirty)
    {
        move_to_row(out, cursor, 0, false/*inner*/);
        append_border(out, uint16_t(tmp.m_lines.size()));
        m_border_dirty = false;
    }

    // TODO: differential update of what's different between m_displayed and tmp.
    // TODO: handle variable height input_box.

    for (uint16_t i = 0; i < tmp.m_lines.size(); ++i)
    {
        auto const& line = tmp.m_lines[i];

        move_to_row(out, cursor, i);
        move_to_column(out, cursor, 0);

        char face = 0;
        const char* t = line.m_text;
        const char* f = line.m_faces;
        for (size_t len = line.m_length; len > 0;)
        {
            if (*f != face)
            {
                out.append_color(get_face_def(*f));
                face = *f;
            }

            wcwidth_iter iter(t, len);
            if (!iter.more())
                break;

            // BUGGBUG: does not handle invalid UTF8 correctly.
            const char32_t c = iter.next();
            const uint32_t clen = iter.character_length();
            assert(clen <= len);

            if (c >= 0 && c < ' ')
            {
                const char ctrl = c + '@';
                out.append("^", 1);
                out.append(&ctrl, 1);
            }
            else
            {
                out.append(iter.character_pointer(), clen);
            }

            t += clen;
            f += clen;
            len -= clen;
            cursor.x += iter.character_wcwidth_twoctrl();
        }

        // Fill remaining width.
        if (line.m_width < max_width)
        {
            out.append_color(get_face_def(m_style ? m_style->empty_face : FACE_EMPTY));
            if (max_width == term_width)
            {
                out.append("\x1b[K");
            }
            else
            {
                out.append_spaces(max_width - line.m_width);
                cursor.x += max_width - line.m_width;
            }
        }
    }

    // TODO: erase rows in m_displayed but not in tmp (also account for border).

    // Position cursor at the caret position.
    move_to_row(out, cursor, tmp.m_cursor.y);
    move_to_column(out, cursor, tmp.m_cursor.x);

    out.append(c_show_cursor);
    term_out(out.c_str(), out.length());

    m_displayed = std::move(tmp);
    m_layout->cursor = cursor;
    return false;
}

void display_manager::move_to_row(cstring& out, coord& cursor, uint16_t y, bool inner)
{
    y += (inner ? m_layout->inner_offset.y : 0);
    if (m_layout->origin.y > 0)
        out.printf("\x1b[%uH", m_layout->origin.y + y);
    else if (y < cursor.y)
        out.printf("\x1b[%uA", cursor.y - y);
    else if (y > cursor.y)
        out.printf("\x1b[%uB", y - cursor.y);
    else
        return;
    cursor.y = y;
}

void display_manager::move_to_column(cstring& out, coord& cursor, uint16_t x, bool inner)
{
    x += (inner ? m_layout->inner_offset.x : 0);
    const uint16_t term_x = m_layout->origin.x + x;
    if (term_x > 0)
        out.printf("\x1b[%uG", term_x);
    else
        out.append("\r");
    cursor.x = x;
}

const char* display_manager::get_face_def(char face) const
{
    if (!m_face_defs)
        return (face == FACE_SELECTION) ? "0;7" : "";

    const auto def = m_face_defs->find(face);
    if (def == m_face_defs->end())
    {
        if (face == FACE_EMPTY)
            return get_face_def(FACE_DEFAULT);
        return "";
    }

    return def->second;
}

bool display_manager::build(display_lines& out)
{
    assert(m_layout);
    assert(m_buffer);
    if (!m_layout || !m_buffer)
        return false;

    // TODO: redisplay when border changes.
    // TODO: redisplay when layout extents change.
    const uint32_t change_counter = m_buffer->get_change_counter();
    const selection_state& sel_state = m_buffer->get_selection_state();
    const textpos_t sel_begin = sel_state.get_sel_begin();
    const textpos_t sel_end = sel_state.get_sel_end();
    const textpos_t pos = sel_state.get_caret();
    const textpos_t left = m_buffer->get_left();

    if (change_counter == m_displayed.m_change_counter &&
        pos == m_displayed.m_pos &&
        left == m_displayed.m_left &&
        sel_end - sel_begin == m_displayed.m_selection_length)
        return false;

    // TODO: callback to provide faces.

    display_lines tmp;
    if (!tmp.m_text.set(m_buffer->get_text()) || !tmp.m_faces.reserve(m_buffer->get_text().length()))
        return false;
    tmp.m_pos = pos;
    tmp.m_left = left;
    tmp.m_change_counter = change_counter;
    tmp.m_selection_length = sel_end - sel_begin;

    const cstring& text = tmp.m_text;
    tmp.m_faces.append_spaces(text.length());   // FACE_DEFAULT == space.

    // Overlay selection color into faces.
    memset(tmp.m_faces.reserve(0) + sel_begin, FACE_SELECTION, sel_end - sel_begin);

    // Parse text into rows (lines).
    wcwidth_iter iter(text.c_str(), text.length());
    const char* const cursor_ptr = text.c_str() + pos;
    const char* row_text = text.c_str();
    const char* row_faces = tmp.m_faces.c_str();
    size_t row_len = 0;
    uint16_t row_width = 0;
    const uint32_t max_width = get_effective_max_width();
// TODO: handle single line input_box.
// TODO: handle fixed-height input_box.
    while (iter.more())
    {
        // BUGBUG: does not handle invalid UTF8 correctly.
        const char32_t c = iter.next();
        const uint32_t clen = iter.character_length();
        const uint32_t cwidth = iter.character_wcwidth_twoctrl();

        const bool update_cursor = (iter.character_pointer() <= cursor_ptr && cursor_ptr < iter.character_pointer() + clen);

        if (row_width + cwidth > max_width)
        {
            display_line line;
            line.m_text = row_text;
            line.m_faces = row_faces;
            line.m_width = row_width;
            line.m_length = row_len;
            line.m_x1 = m_layout->origin.x;
            line.m_x2 = m_layout->origin.x + max_width - 1;
            tmp.m_lines.emplace_back(std::move(line));

            row_text += row_len;
            row_faces += row_len;
            row_len = 0;
            row_width = 0;
        }

        if (update_cursor)
        {
            tmp.m_cursor.x = row_width;
            tmp.m_cursor.y = uint16_t(tmp.m_lines.size());
        }

        row_len += clen;
        row_width += cwidth;
    }

    // Add last line.
    display_line line;
    line.m_text = row_text;
    line.m_faces = row_faces;
    line.m_width = row_width;
    line.m_length = row_len;
    line.m_x1 = m_layout->origin.x;
    line.m_x2 = m_layout->origin.x + get_effective_max_width() - 1;
    tmp.m_lines.emplace_back(std::move(line));

    if (tmp.m_cursor.x < 0)
    {
        assert(tmp.m_lines.size() > 0);
        tmp.m_cursor.x = row_width;
        tmp.m_cursor.y = uint16_t(tmp.m_lines.size()) - 1;
    }
    if (uint32_t(tmp.m_cursor.x) >= max_width)
    {
        display_line line;
        line.m_text = "";
        line.m_faces = "";
        line.m_width = 0;
        line.m_length = 0;
        line.m_x1 = m_layout->origin.x;
        line.m_x2 = m_layout->origin.x + get_effective_max_width() - 1;
        tmp.m_lines.emplace_back(std::move(line));

        tmp.m_cursor.x = 0;
        ++tmp.m_cursor.y;
    }

#if 0
    m_colors->append_color(out, tib::color_element::base);

    uint16_t max_width = get_effective_max_width();
    bool left_marker = m_style->horiz_scroll_markers && (m_left > 0);
    bool right_marker = false;
    size_t lo_limit = m_left;
    size_t hi_limit = 0;

    if (left_marker)
    {
        wcwidth_iter wi(m_text.c_str() + m_left);
        if (wi.next())
        {
            lo_limit += wi.character_length();
            max_width -= 1; // Width of left marker, not the iter character.
        }
    }

    uint16_t width = 0;
    const size_t len = fits_in_wcwidth(m_text.c_str() + lo_limit, m_text.length() - lo_limit, max_width - m_style->horiz_scroll_markers, &width);
    hi_limit = lo_limit + len;

    if (m_style->horiz_scroll_markers && width > 0)
    {
        wcwidth_iter wi(m_text.c_str() + lo_limit + len);
        if (wi.next())
        {
            if (hi_limit + wi.character_length() == m_text.length() &&
                width + wi.character_wcwidth_onectrl() <= max_width)
            {
                hi_limit = m_text.length();
                width += wi.character_wcwidth_onectrl();
            }
            else
            {
                right_marker = true;
                --max_width;
            }
        }
    }

    if (left_marker)
    {
        m_colors->append_color(out, color_element::base, color_element::input_horiz_scroll);
        out.append("<", 1);
    }
    m_colors->append_color(out, color_element::base, color_element::input);

    out.append(m_text.c_str() + lo_limit, len);

    out.append_spaces(max_width - width);
    if (right_marker)
    {
        m_colors->append_color(out, color_element::base, color_element::input_horiz_scroll);
        out.append(">", 1);
    }

    const int16_t cursor_x = origin.x + left_marker + __wcswidth(m_text.c_str() + lo_limit, m_selection.get_caret() - lo_limit);
#endif

    out = std::move(tmp);
    return true;
}

void display_manager::append_border(cstring& out, uint16_t inner_lines)
{
    assert(m_layout);
    assert(m_buffer);
    assert(m_style);

    const border_definition& b = *m_style->border;
    const uint16_t b_left_width = b.has_left() ? cell_count(b.left, -1) : 0;
    const uint16_t b_right_width = b.has_right() ? cell_count(b.right, -1) : 0;
    const uint16_t extra_border_width = b_left_width + b_right_width;
    const uint16_t extra_border_height = b.has_top() + b.has_bottom();
    const uint16_t _width = get_effective_max_width();
    if (!_width)
        return;
    const uint16_t width = _width + extra_border_width;
    // TODO: fixed height.
    // TODO: variable height not exceeding max_height.
    const uint16_t height = inner_lines + extra_border_height;

    out.append_color(m_colors->get_color(tib::color_element::border));

    if (b.top)
    {
        out.printf("\x1b[%uG", m_layout->origin.x);
        if (b.top_left)
            out.append(b.top_left);
        for (uint16_t i = width - ((b.top_left ? cell_count(b.top_left, -1) : 0) + (b.top_right ? cell_count(b.top_right, -1) : 0)); i--;)
            out.append(b.top);
        if (b.top_right)
            out.append(b.top_right);
    }

    for (uint16_t i = height - extra_border_height; i--;)
    {
        out.append("\r\n");
        if (b_left_width)
        {
            out.printf("\x1b[%uG", m_layout->origin.x);
            out.append(b.left);
        }
        if (b_right_width)
        {
            out.printf("\x1b[%uG", m_layout->origin.x + width - b_right_width);
            out.append(b.right);
        }
    }

    if (b.bottom)
    {
        out.printf("\r\n\x1b[%uG", m_layout->origin.x);
        if (b.bottom_left)
            out.append(b.bottom_left);
        for (uint16_t i = width - ((b.bottom_left ? cell_count(b.bottom_left, -1) : 0) + (b.bottom_right ? cell_count(b.bottom_right, -1) : 0)); i--;)
            out.append(b.bottom);
        if (b.bottom_right)
            out.append(b.bottom_right);
    }

    if (height > 1)
        out.printf("\x1b[%uA", height - 1);
}

} // namespace tib
