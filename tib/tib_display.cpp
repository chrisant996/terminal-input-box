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

// TODO:  Abstract behind a terminal object.
#ifdef _WIN32
    const HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    const uint16_t term_width = (GetConsoleScreenBufferInfo(hout, &csbi) ? csbi.dwSize.X : 80);

    const border_definition* b = m_style ? m_style->border : nullptr;
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
#else
    // TODO:  Alternative Linux implementation.
#endif
}

bool display_manager::build(display_lines& out)
{
    assert(m_layout);
    assert(m_buffer);
    if (!m_layout || !m_buffer)
        return false;

    const uint32_t change_counter = m_buffer->get_change_counter();
    const textpos_t pos = m_buffer->get_selection_state().get_caret();
    const textpos_t left = m_buffer->get_left();

    if (change_counter == m_displayed.m_change_counter && pos == m_displayed.m_pos && left == m_displayed.m_left)
        return false;

    // TODO: callback to provide faces.

    display_lines tmp;
    if (!tmp.m_text.set(m_buffer->get_text()) || !tmp.m_faces.reserve(m_buffer->get_text().length()))
        return false;
    tmp.m_pos = pos;
    tmp.m_left = left;
    tmp.m_change_counter = change_counter;

    tmp.m_text = m_buffer->get_text();
    const cstring& text = tmp.m_text.c_str();
    tmp.m_faces.append_spaces(text.length());   // FACE_DEFAULT == space.

    // Overlay selection color into faces.
    const selection_state& sel_state = m_buffer->get_selection_state();
    const textpos_t sel_begin = sel_state.get_sel_begin();
    const textpos_t sel_end = sel_state.get_sel_end();
    memset(tmp.m_faces.reserve(0) + sel_begin, FACE_SELECTION, sel_end - sel_begin);

    // TODO: parse text into rows (lines).

    // TODO: build display_line structs for each parsed row.
    display_line line;
    line.m_text = text.c_str();
    line.m_faces = tmp.m_faces.c_str();
    line.m_length = text.length();
    line.m_x1 = m_layout->origin.x;
    line.m_x2 = m_layout->origin.x + get_effective_max_width() - 1;
    tmp.m_lines.emplace_back(std::move(line));

    // TODO: calculate m_cursor.
    tmp.m_cursor.x = pos + 1; // BUGBUG: m_cursor is in COLUMNS not CHARS.
    tmp.m_cursor.y = 0;

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

bool display_manager::display()
{
    assert(m_layout);
    assert(m_buffer);
    if (!m_layout || !m_buffer || !m_buffer->get_change_counter())
        return false;   // Nothing to display.

    const uint32_t change_counter = m_buffer->get_change_counter();
    const textpos_t pos = m_buffer->get_selection_state().get_caret();
    const textpos_t left = m_buffer->get_left();
    if (change_counter == m_displayed.m_change_counter &&
        pos == m_displayed.m_pos &&
        left == m_displayed.m_left)
        return false;   // Nothing changed since last display.

    // Format content into display structures.
    display_lines tmp;
    if (!build(tmp))
        return false;

    cstring out;

    // TODO:  Encapsulate terminal codes behind some termcap layer.

    out.set(c_hide_cursor);

    // TODO:  This is a temporary hack for borders with single line tib.
    if (m_style && m_style->border && m_style->border->has_top())
        out.append("\x1b[A");

    auto goto_origin = [&](bool inner, coord& origin)
    {
        origin = m_layout->origin;
        if (inner && m_style && m_style->border)
        {
            if (m_layout->origin.y > 0)
                origin.y += m_style->border->has_top();
            origin.x += m_style->border->has_left() ? cell_count(m_style->border->left, -1) : 0;
        }
        if (m_layout->origin.y > 0)
        {
            out.printf("\x1b[%u;%uH", origin.y, origin.x);
        }
        else
        {
            // TODO:  Move up to the origin row using relative positioning.
            // That's crucial for supporting an input box with variable height.
            if (inner && m_style && m_style->border && m_style->border->has_top())
                out.append("\r\n");
            out.printf("\x1b[%uG", origin.x);
        }
    };

    // Draw border if needed.
    coord origin;
    if (m_style && m_style->border)
    {
        goto_origin(false/*inner*/, origin);
        if (m_border_dirty)
        {
            append_border(origin, out);
            m_border_dirty = false;
        }
    }

    // Position cursor to draw the input text.
    // TODO: avoid unless actually needs to display updated content.
    goto_origin(true/*inner*/, origin);

    // TODO: differential update of what's different between m_displayed and tmp.
    m_colors->append_color(out, tib::color_element::base);
    out.append(tmp.m_text.c_str());

    // Position cursor at the caret position.
    if (origin.y > 0)
    {
        out.printf("\x1b[%u;%uH", origin.y + tmp.m_cursor.y - 1, tmp.m_cursor.x);
    }
    else
    {
        // TODO:  Move up to the origin row using relative positioning.
        // That's crucial for supporting an input box with variable height.
        out.printf("\x1b[%uG", tmp.m_cursor.x);
    }

    out.append(c_show_cursor);
    term_out(out.c_str(), out.length());

    m_displayed = std::move(tmp);
    return false;
}

void display_manager::append_border(const coord& origin, cstring& out)
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
    // TODO:  Multi-line, and variable height.
    const uint16_t height = m_layout->max_height + extra_border_height;

    out.append_color(m_colors->get_color(tib::color_element::border));

    if (b.top)
    {
        out.printf("\x1b[%uG", origin.x);
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
            out.printf("\x1b[%uG", origin.x);
            out.append(b.left);
        }
        if (b_right_width)
        {
            out.printf("\x1b[%uG", origin.x + width - b_right_width);
            out.append(b.right);
        }
    }

    if (b.bottom)
    {
        out.printf("\r\n\x1b[%uG", origin.x);
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
