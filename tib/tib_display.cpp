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

bool g_coalesce_output = true;

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

display_line::display_line(uint16_t x1)
: m_x1(x1)
, m_x2(x1)
{
    assert(m_x1);
}

void display_line::append(const char* p, uint32_t len, uint32_t width, char face)
{
    m_text.append(p, len);
    const size_t faces_len = m_faces.length();
    memset(m_faces.reserve(faces_len + len) + faces_len, face, len);
    m_faces.set_length(faces_len + len);

    this->m_x2 += width;
}

void display_lines::clear()
{
    m_pos = 0;
    m_left = 0;
    m_selection_length = 0;
    m_change_counter = 0;

    m_lines.clear();
    m_cursor = { -1, -1 };
}

void display_manager::init_layout(const layout_info* layout)
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

void display_manager::set_origin(int16_t x, int16_t y)
{
    assert(x != 0);
    assert(y != 0);
    m_origin.x = (x == uint16_t(-1)) ? 1 : x;
    m_origin.y = y;
    m_displayed.clear();
    invalidate();
    invalidate_border();
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
    if (uint32_t(m_origin.x) + max_width + extra_border_width >= term_width)
    {
        if (term_width <= m_origin.x + extra_border_width)
            return 0;
        max_width = uint32_t(term_width - (m_origin.x + extra_border_width - 1));
        if (int32_t(max_width) < 8)
            return 0;
    }
    return max_width;
}

coord display_manager::get_extent() const
{
    return m_displayed.m_extent;
}

bool display_manager::display()
{
    assert(m_layout);
    assert(m_buffer);
    if (!m_layout || !m_buffer || !m_buffer->get_change_counter())
        return false;   // Nothing to display.

    // If origin not set yet, then pin it "here".
    if (m_origin.x <= 0)
    {
        m_origin.x = 1;
        m_origin.y = -1;
        m_relative_cursor = { -1, 0 };   // Cursor is relative to origin.
    }

    // Format content into display structures.
// TODO: allow host to add their own display_line rows; that will simplify
// showing/clearing their extra rows (something Clink still struggles with).
    display_lines tmp;
    if (!build(tmp))
        return false;   // Nothing changed since list display (or OOM error).

    if (!m_displayed.m_change_counter || memcmp(&tmp.m_extent, &m_displayed.m_extent, sizeof(tmp.m_extent)) != 0)
        m_border_dirty = true;

    m_accumulator.clear();
    m_coalesce_output = g_coalesce_output;

    coord cursor = m_relative_cursor;
    const uint16_t term_width = get_terminal_width();
    const uint32_t max_width = get_effective_max_width();

    if (cursor.x < 0 && cursor.y < 0)
        cursor = { -1, 0 };             // -1 forces move_to_column.
// BUGBUG: cursor location initialization isn't thorough or fully correct yet.

    // TODO:  Encapsulate terminal codes behind some termcap layer.

    auto erase_row = [&](int16_t width)
    {
        if (width <= 0)
            return;
        if (m_origin.x + max_width - 1 == term_width)
        {
            output("\x1b[K");
        }
        else
        {
            output_spaces(width);
            cursor.x += width;
        }
    };

    output(c_hide_cursor);

    // Draw border if needed.
    if (m_style && m_style->border && m_border_dirty)
    {
        move_to_row(cursor, 0, 0);
        move_to_column(cursor, 0, 0);
        append_border(uint16_t(tmp.m_lines.size()));
        m_border_dirty = false;
    }

    // TODO: differential update of what's different between m_displayed and tmp.
    // TODO: handle variable height input_box.

    for (uint16_t i = 0; i < tmp.m_lines.size(); ++i)
    {
        auto const& line = tmp.m_lines[i];

        move_to_row(cursor, i, tmp.m_inner_offset.y);
        move_to_column(cursor, 0, tmp.m_inner_offset.x);

        char face = 0;
        const char* t = line->m_text.c_str();
        const char* f = line->m_faces.c_str();
        for (size_t len = line->m_text.length(); len > 0;)
        {
            if (*f != face)
            {
                output_color(get_face_def(*f));
                face = *f;
            }

            wcwidth_iter iter(t, len);
            if (!iter.more())
                break;

            // BUGGBUG: does not handle invalid UTF8 correctly.
            const char32_t c = iter.next();
            const uint32_t clen = iter.character_length();
            assert(clen <= len);

            // TODO: optimize to add a run instead of just a grapheme.
            output(iter.character_pointer(), clen);

            t += clen;
            f += clen;
            len -= clen;
            cursor.x += iter.character_wcwidth_twoctrl();
        }

        // Fill remaining width.
        if (line->width() < max_width)
        {
            output_color(get_face_def(m_style ? m_style->empty_face : FACE_EMPTY));
            erase_row(max_width - line->width());
        }
    }

    // Erase rows in m_displayed but not in tmp.
// BUGBUG: not erasing properly.
    if (tmp.m_extent.y < m_displayed.m_extent.y)
    {
        output_color("");
        for (uint16_t i = tmp.m_extent.y; i < m_displayed.m_extent.y; ++i)
        {
            move_to_row(cursor, i, 0);
            move_to_column(cursor, 0, 0);
            erase_row(tmp.m_extent.x);
        }
    }

    // Position cursor at the caret position.
    move_to_row(cursor, tmp.m_cursor.y, tmp.m_inner_offset.y);
    move_to_column(cursor, tmp.m_cursor.x, tmp.m_inner_offset.x);

    output(c_show_cursor);

    if (m_coalesce_output)
    {
        m_coalesce_output = false;
        maybe_flush();
    }

    m_displayed = std::move(tmp);
    m_relative_cursor = cursor;
    return false;
}

void display_manager::move_to_row(coord& cursor, uint16_t y, uint16_t inner_offset)
{
    y += inner_offset;
    if (m_origin.y > 0)
        outputf("\x1b[%uH", m_origin.y + y);
    else if (y < cursor.y)
        outputf("\x1b[%uA", cursor.y - y);
    else if (y > cursor.y)
        outputf("\x1b[%uB", y - cursor.y);
    else
        return;
    cursor.y = y;
}

void display_manager::move_to_column(coord& cursor, uint16_t x, uint16_t inner_offset)
{
    x += inner_offset;
    const uint16_t term_x = m_origin.x + x;
    if (term_x > 0)
        outputf("\x1b[%uG", term_x);
    else
        output("\r");
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

    const cstring& text = m_buffer->get_text();

    // TODO: callback to provide faces.
    cstring faces;
    faces.append_spaces(text.length());     // FACE_DEFAULT == space.

    // Overlay selection color into faces.
    memset(faces.reserve(0) + sel_begin, FACE_SELECTION, sel_end - sel_begin);

    display_lines tmp;
    tmp.m_pos = pos;
    tmp.m_left = left;
    tmp.m_change_counter = change_counter;
    tmp.m_selection_length = sel_end - sel_begin;

    // Set up border.
    if (m_style && m_style->border)
    {
        // TODO: cache border metrics.
        const border_definition& b = *m_style->border;
        tmp.m_inner_offset.y = b.has_top() ? 1 : 0;
        tmp.m_inner_offset.x = b.has_left() ? cell_count(b.left, -1) : 0;
        tmp.m_extent.x += (b.has_left() ? cell_count(b.left, -1) : 0) + (b.has_right() ? cell_count(b.right, -1) : 0);
        tmp.m_extent.y += !!b.has_top() + !!b.has_bottom();
    }

    // Parse text into rows (lines).
    wcwidth_iter iter(text.c_str(), text.length());
    const char* const cursor_ptr = text.c_str() + pos;
    // const char* row_text = text.c_str();
    const char* face = faces.c_str();
    char pending = 0;
    bool expanding = false;
    const uint32_t max_width = get_effective_max_width();
    std::unique_ptr<display_line> line = std::make_unique<display_line>(m_origin.x);
// TODO: handle single line input_box.
// TODO: handle fixed-height input_box.
    while (iter.more())
    {
        // BUGBUG: does not handle invalid UTF8 correctly.
        const char32_t c = iter.next();
        const char* p = iter.character_pointer();
        uint32_t clen = iter.character_length();
        uint32_t cwidth = iter.character_wcwidth_twoctrl();

        if (iter.character_pointer() <= cursor_ptr && cursor_ptr < iter.character_pointer() + clen)
        {
            tmp.m_cursor.x = line->width();
            tmp.m_cursor.y = uint16_t(tmp.m_lines.size());
        }

        if (iter.character_wcwidth_signed() < 0)
        {
            if (*p == '\n' && m_layout->max_height > 1)
            {
                tmp.m_lines.emplace_back(std::move(line));
                line = std::make_unique<display_line>(m_origin.x);
                goto next;
            }
            else
            {
                pending = *p + '@';
                expanding = true;
                p = "^";
                assert(clen == 1);
                cwidth = 1;
            }
        }

again:
        if (line->width() + cwidth > max_width)
        {
            tmp.m_lines.emplace_back(std::move(line));
            line = std::make_unique<display_line>(m_origin.x);
        }

        line->append(p, clen, cwidth, *face);

        if (expanding)
        {
            p = &pending;
            assert(clen == 1);
            assert(cwidth == 1);
            expanding = false;
            goto again;
        }

next:
        face += clen;
    }

    // Add last line.
    if (tmp.m_cursor.x < 0)
    {
        tmp.m_cursor.x = line->width();
        tmp.m_cursor.y = uint16_t(tmp.m_lines.size());
    }
    tmp.m_lines.emplace_back(std::move(line));
    if (uint32_t(tmp.m_cursor.x) >= max_width)
    {
        tmp.m_cursor.x = 0;
        ++tmp.m_cursor.y;
        while (tmp.m_cursor.y >= tmp.m_lines.size())
        {
            line = std::make_unique<display_line>(m_origin.x);
            tmp.m_lines.emplace_back(std::move(line));
        }
    }

    tmp.m_extent.x += max_width;
    tmp.m_extent.y += uint16_t(tmp.m_lines.size());

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

void display_manager::append_border(uint16_t inner_lines)
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

    output_color(m_colors->get_color(tib::color_element::border));

    if (b.top)
    {
        outputf("\x1b[%uG", m_origin.x);
        if (b.top_left)
            output(b.top_left);
        for (uint16_t i = width - ((b.top_left ? cell_count(b.top_left, -1) : 0) + (b.top_right ? cell_count(b.top_right, -1) : 0)); i--;)
            output(b.top);
        if (b.top_right)
            output(b.top_right);
    }

    for (uint16_t i = height - extra_border_height; i--;)
    {
        output("\r\n");
        if (b_left_width)
        {
            outputf("\x1b[%uG", m_origin.x);
            output(b.left);
        }
        if (b_right_width)
        {
            outputf("\x1b[%uG", m_origin.x + width - b_right_width);
            output(b.right);
        }
    }

    if (b.bottom)
    {
        outputf("\r\n\x1b[%uG", m_origin.x);
        if (b.bottom_left)
            output(b.bottom_left);
        for (uint16_t i = width - ((b.bottom_left ? cell_count(b.bottom_left, -1) : 0) + (b.bottom_right ? cell_count(b.bottom_right, -1) : 0)); i--;)
            output(b.bottom);
        if (b.bottom_right)
            output(b.bottom_right);
    }

    if (height > 1)
        outputf("\x1b[%uA", height - 1);
}

void display_manager::output(const char* s, size_t len)
{
    m_accumulator.append(s, len);
    maybe_flush();
}

void display_manager::outputf(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    m_accumulator.printfv(format, args);
    maybe_flush();

    va_end(args);
}

void display_manager::output_color(const char* sgr_params)
{
    m_accumulator.append_color(sgr_params);
    maybe_flush();
}

void display_manager::output_spaces(size_t n)
{
    m_accumulator.append_spaces(n);
    maybe_flush();
}

void display_manager::maybe_flush()
{
    if (m_coalesce_output)
        return;

    term_out(m_accumulator.c_str(), m_accumulator.length());
    m_accumulator.clear();
}

} // namespace tib
