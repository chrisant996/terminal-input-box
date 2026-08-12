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

#ifdef WIDE_HORZ_SCROLL_MARKERS
const uint16_t c_horz_scroll_indicator_chars = 2;
#else
const uint16_t c_horz_scroll_indicator_chars = 1;
#endif

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
    m_top = 0;
    m_pos = 0;
    m_left = 0;
    m_selection_length = 0;
    m_change_counter = 0;

    m_lines.clear();
    m_cursor = { -1, -1 };

    m_inner_offset = { 0, 0 };
    m_extent = { 0, 0 };
}

void display_lines::apply_scroll_markers(int16_t rows)
{
    assert(rows > 0);
    if (m_lines.size() < size_t(rows))
    {
        m_top = 0;
        return;
    }

    // Discard lines after the visible section.
    if (m_lines.size() > size_t(m_top + rows))
    {
        m_lines.erase(m_lines.begin() + m_top + rows, m_lines.end());
// TODO: apply scroll marker to last row.
    }

    // Discard lines before the visible section.
    if (m_lines.size() > rows)
    {
        m_top = int32_t(m_lines.size() - size_t(rows));
        m_lines.erase(m_lines.begin(), m_lines.begin() + m_top);
        assert(m_lines.size() == rows);
// TODO: apply scroll marker to first row.
    }

    m_cursor.y = max(0, m_cursor.y - m_top);
}

void display_manager::init_layout(const layout_info* layout)
{
    m_layout = layout;
    m_displayed.clear();
    m_top = 0;
    invalidate();
    invalidate_border();
}

void display_manager::init_buffer(const input_buffer* buffer)
{
    m_buffer = buffer;
    m_top = 0;
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

void display_manager::set_origin(int32_t x, int32_t y)
{
    assert(x != 0);
    assert(y != 0);
    m_origin.x = (x == uint32_t(-1)) ? 1 : x;
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

coord display_manager::get_effective_max_size(bool omit_scroll_markers) const
{
    assert(m_layout);
    if (!m_layout)
    {
nope:
        return { 0, 0 };
    }

    const coord term_size = get_terminal_size();

    const border_definition* b = m_style ? m_style->border : nullptr;
    // TODO: cache border cell_count metrics.
    const uint16_t b_left_width = (b && b->has_left()) ? cell_count(b->left, -1) : 0;
    const uint16_t b_right_width = (b && b->has_right()) ? cell_count(b->right, -1) : 0;
    const uint16_t extra_border_width = b_left_width + b_right_width;
    const uint16_t b_height = !!b->has_top() + !!b->has_bottom();

    coord max_size;
    max_size.x = m_layout->max_width;
    max_size.y = clamp<int16_t>(m_layout->max_height, 0, term_size.y - b_height);
    if (m_origin.x + max_size.x + extra_border_width >= term_size.x)
    {
        if (term_size.x <= m_origin.x + extra_border_width)
            goto nope;
        max_size.x = term_size.x - (m_origin.x + extra_border_width - 1);
        if (max_size.x < 8)
            goto nope;
    }
    if (max_size.y <= 0)
        goto nope;

    if (omit_scroll_markers && max_size.y == 1 && m_style->horiz_scroll_markers)
        max_size.x = (max_size.x > c_horz_scroll_indicator_chars) ? max_size.x - c_horz_scroll_indicator_chars : 0;

    assert(max_size.x >= 0);
    assert(max_size.y >= 0);
    return max_size;
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
    // showing/clearing its extra rows (something Clink still struggles with).
    display_lines tmp;
    if (!build(tmp))
        return false;   // Nothing changed since list display (or OOM error).

    return display_internal(tmp);
}

bool display_manager::display_internal(display_lines& lines)
{
    if (lines.m_erase)
    {
        assert(lines.m_lines.empty());
        lines.m_extent.x = m_displayed.m_extent.x;
        lines.m_extent.y = 0;
    }

    if (!m_displayed.m_change_counter ||
        memcmp(&lines.m_extent, &m_displayed.m_extent, sizeof(lines.m_extent)) != 0)
        m_border_dirty = true;

    m_accumulator.clear();
    m_coalesce_output = g_coalesce_output;

    coord cursor = m_relative_cursor;
    const coord term_size = get_terminal_size();
    const coord max_size = get_effective_max_size();

    if (cursor.x < 0 && cursor.y < 0)
        cursor = { -1, 0 };             // -1 forces move_to_column.
// BUGBUG: cursor location initialization isn't thorough or fully correct yet.

    // TODO: Encapsulate terminal codes behind some termcap layer.

    auto erase_row = [&](int16_t width)
    {
        if (width <= 0)
            return;
        if (m_origin.x + max_size.x - 1 == term_size.x)
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
    if (!lines.m_erase && m_style && m_style->border && m_border_dirty)
    {
        move_to_row(cursor, 0, 0);
        move_to_column(cursor, 0, 0);
        append_border(lines.m_extent);
    }
    m_border_dirty = false;

    // TODO: differential update of what's different between m_displayed and lines.

    for (uint16_t i = 0; i < lines.m_lines.size(); ++i)
    {
        auto const& line = lines.m_lines[i];

        move_to_row(cursor, i, lines.m_inner_offset.y);
        move_to_column(cursor, 0, lines.m_inner_offset.x);

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
        if (line->width() < max_size.x)
        {
            output_color(get_face_def(m_style ? m_style->empty_face : FACE_EMPTY));
            erase_row(max_size.x - line->width());
        }
    }

    // Erase rows in m_displayed but not in lines.
    if (lines.m_extent.y < m_displayed.m_extent.y)
    {
        output_color("");
        for (uint16_t i = lines.m_extent.y; i < m_displayed.m_extent.y; ++i)
        {
            move_to_row(cursor, i, 0);
            move_to_column(cursor, 0, 0);
            erase_row(lines.m_extent.x);
        }
    }

    // Position cursor at the caret position.
    if (lines.m_erase)
        lines.m_cursor = { 0, 0 };
    move_to_row(cursor, lines.m_cursor.y, lines.m_inner_offset.y);
    move_to_column(cursor, lines.m_cursor.x, lines.m_inner_offset.x);

    output_color("");
    output(c_show_cursor);

    if (m_coalesce_output)
    {
        m_coalesce_output = false;
        maybe_flush();
    }

    m_top = lines.m_top;
    m_displayed = std::move(lines);
    m_relative_cursor = cursor;
    return false;
}

void display_manager::erase_display()
{
    if (m_displayed.m_extent.y > 0)
    {
        display_lines tmp;
        tmp.m_erase = true;
        display_internal(tmp);
    }
}

void display_manager::move_to_end_of_display()
{
    if (m_displayed.m_extent.y > 0)
    {
        move_to_row(m_relative_cursor, m_displayed.m_extent.y - 1, 0);

        output("\r\n", 2);
        m_relative_cursor.x = 0;
        ++m_relative_cursor.y;
    }
}

void display_manager::move_to_caret_position()
{
    if (m_displayed.m_extent.y > 0)
    {
        move_to_row(m_relative_cursor, m_displayed.m_cursor.y, m_displayed.m_inner_offset.y);
        move_to_column(m_relative_cursor, m_displayed.m_cursor.x, m_displayed.m_inner_offset.x);
    }
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
    coord term_size = get_terminal_size();
    if (m_style && m_style->border)
    {
        // TODO: cache border metrics.
        const border_definition& b = *m_style->border;
        const uint16_t b_height = !!b.has_top() + !!b.has_bottom();
        tmp.m_inner_offset.y = b.has_top() ? 1 : 0;
        tmp.m_inner_offset.x = b.has_left() ? cell_count(b.left, -1) : 0;
        tmp.m_extent.x += (b.has_left() ? cell_count(b.left, -1) : 0) + (b.has_right() ? cell_count(b.right, -1) : 0);
        tmp.m_extent.y += b_height;
        term_size.y -= b_height;
    }

    // Set up max height.
    const coord max_size = get_effective_max_size();
    const coord max_size_omit_scroll_markers = get_effective_max_size(true/*omit_scroll_markers*/);
    if (max_size.y < 1)
        return false;
    const bool multiline = (max_size.y > 1);

    wcwidth_iter iter(text.c_str() + left, text.length() - left);
    const char* const cursor_ptr = text.c_str() + pos;
    const char* face = faces.c_str();
    char pending = 0;
    bool expanding = false;
    std::unique_ptr<display_line> line = std::make_unique<display_line>(m_origin.x);

    assert(!(left && multiline));
    if (left && m_style->horiz_scroll_markers)
    {
        iter.next(); // Skip the grapheme that the scroller replaces.
        line->append("<", 1, 1, FACE_SCROLLER);
        if (c_horz_scroll_indicator_chars > 0)
        {
            for (uint16_t num = c_horz_scroll_indicator_chars - 1; num--;)
                line->append("<", 1, 1, FACE_SCROLLER);
        }
    }

    // Parse text into rows (lines).
    // FUTURE: Performance could be improved by first parsing to find row
    // start offsets, then calculating which rows will actually be visible,
    // and finally constructing only display_line instances for the visible
    // rows.
    bool short_circuited = false;
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
            tmp.m_cursor.y = uint32_t(tmp.m_lines.size());
        }

        if (iter.character_wcwidth_signed() < 0)
        {
            if (*p == '\n' && multiline)
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
        if (!multiline)
        {
            if (line->width() + cwidth > uint32_t(max_size_omit_scroll_markers.x) &&
                !(!expanding &&
                  !iter.more() &&
                  line->width() + cwidth <= uint32_t(max_size_omit_scroll_markers.x + c_horz_scroll_indicator_chars)))
            {
                short_circuited = true;
                break;
            }
        }
        else if (line->width() + cwidth > uint32_t(max_size.x))
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
        tmp.m_cursor.y = uint32_t(tmp.m_lines.size());
    }
    tmp.m_lines.emplace_back(std::move(line));
    if (!multiline)
    {
        assert(tmp.m_lines.size() == 1);
        assert(tmp.m_cursor.x >= 0);
        assert(tmp.m_cursor.x <= max_size.x);
        if (short_circuited || iter.more())
        {
            auto& back = tmp.m_lines.back();
            assert(int32_t(back->width()) < max_size.x);
            while (int32_t(back->width() + 1) < max_size.x)
                back->append(" ", 1, 1, FACE_DEFAULT);
            back->append(">", 1, 1, FACE_SCROLLER);
            if (c_horz_scroll_indicator_chars > 0)
            {
                for (uint16_t num = c_horz_scroll_indicator_chars - 1; num--;)
                    back->append(">", 1, 1, FACE_SCROLLER);
            }
        }
    }
    else if (tmp.m_cursor.x >= max_size.x)
    {
        tmp.m_cursor.x = 0;
        ++tmp.m_cursor.y;
        while (tmp.m_cursor.y >= tmp.m_lines.size())
        {
            line = std::make_unique<display_line>(m_origin.x);
            tmp.m_lines.emplace_back(std::move(line));
        }
    }

    // Handle variable height mode.
    int32_t y_extent = max_size.y;
    if (m_layout->variable_height && size_t(y_extent) > tmp.m_lines.size())
        y_extent = int32_t(tmp.m_lines.size());
    assert(y_extent > 0);

    // Record the actual extents.
    tmp.m_extent.x += max_size.x;
    tmp.m_extent.y += y_extent;

    // Scroll to keep cursor in view.
    tmp.m_top = m_top;
    tmp.m_top = clamp<int32_t>(tmp.m_top, tmp.m_cursor.y - (y_extent - 1), tmp.m_cursor.y);
    tmp.m_top = max<int32_t>(tmp.m_top, 0);

    // Scroll when cursor is on a scroll marker.
// TODO: scroll when cursor is on a scroll marker.
#if 0
    if (m_top > m_last_prompt_line_botlin && m_top == m_last_prompt_line_botlin + next->vpos())
    {
        const display_line* d = next->get(m_top);
        if (next->cpos() >= d->m_x && next->cpos() < d->m_x + c_horz_scroll_indicator_chars)
            m_top--;
    }
    else if (m_top + input_botlin_offset < next->count() - 1 && m_top + input_botlin_offset == next->vpos())
    {
        if (next->cpos() + c_horz_scroll_indicator_chars >= _rl_screenwidth && next->cpos() < _rl_screenwidth)
            m_top++;
    }
    assert(m_top >= m_last_prompt_line_botlin);
#endif

    // Handle fixed height mode.
    while (tmp.m_lines.size() < y_extent)
        tmp.m_lines.emplace_back(std::move(std::make_unique<display_line>(m_origin.x)));

    // Apply scroll markers.
    if (y_extent > 1 && y_extent < tmp.m_lines.size())
        tmp.apply_scroll_markers(y_extent);

    assert(tmp.m_cursor.y >= 0);
    assert(size_t(tmp.m_cursor.y) < tmp.m_lines.size());

    out = std::move(tmp);
    return true;
}

void display_manager::append_border(coord extent)
{
    assert(m_layout);
    assert(m_buffer);
    assert(m_style);

    const border_definition& b = *m_style->border;
    const uint16_t b_left_width = b.has_left() ? cell_count(b.left, -1) : 0;
    const uint16_t b_right_width = b.has_right() ? cell_count(b.right, -1) : 0;
    const uint16_t extra_border_width = b_left_width + b_right_width;
    const uint16_t extra_border_height = b.has_top() + b.has_bottom();
    const coord max_size = get_effective_max_size();
    if (max_size.x <= 0 || max_size.y <= 0)
        return;
    assert(extent.x == max_size.x + extra_border_width);
    assert(max_size.y == extent.y - extra_border_height);

    output_color(m_colors->get_color(tib::color_element::border));

    if (b.top)
    {
        outputf("\x1b[%uG", m_origin.x);
        if (b.top_left)
            output(b.top_left);
        for (uint32_t i = extent.x - ((b.top_left ? cell_count(b.top_left, -1) : 0) + (b.top_right ? cell_count(b.top_right, -1) : 0)); i--;)
            output(b.top);
        if (b.top_right)
            output(b.top_right);
    }

    for (uint32_t i = max_size.y; i--;)
    {
        output("\r\n");
        if (b_left_width)
        {
            outputf("\x1b[%uG", m_origin.x);
            output(b.left);
        }
        if (b_right_width)
        {
            outputf("\x1b[%uG", m_origin.x + extent.x - b_right_width);
            output(b.right);
        }
    }

    if (b.bottom)
    {
        outputf("\r\n\x1b[%uG", m_origin.x);
        if (b.bottom_left)
            output(b.bottom_left);
        for (uint32_t i = extent.x - ((b.bottom_left ? cell_count(b.bottom_left, -1) : 0) + (b.bottom_right ? cell_count(b.bottom_right, -1) : 0)); i--;)
            output(b.bottom);
        if (b.bottom_right)
            output(b.bottom_right);
    }

    if (extent.y > 1)
        outputf("\x1b[%uA", extent.y - 1);
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
