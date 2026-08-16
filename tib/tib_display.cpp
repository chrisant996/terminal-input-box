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
    "┌",    "─",    "┐",
    "│",            "│",
    "└",    "─",    "┘",

    1,      1,      1,
    1,              1,
    1,      1,      1,
};

int8_t border_definition::get_width(const char* s, int8_t width) const
{
    return (!s || !*s) ? 0 : (width < 0) ? __wcswidth(s, -1) : width;
}

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

void display_line::calculate_multiline_scroll_marker()
{
    assert(!m_trail_scroller_width_displaced);
    assert(!m_trail_scroller_len_displayed);

    // NOTE: build() should always pad to max width, except on last line, and
    // max width should always be at least 8.
    if (width() > c_horz_scroll_indicator_chars)
    {
        const uint32_t num = c_horz_scroll_indicator_chars;
        uint32_t width_displaced = 0;
        uint32_t len_displaced = 0;
        uint32_t pos = uint32_t(m_text.length());
        while (pos > 0 && width_displaced < num)
        {
            uint16_t wc;
            const uint32_t new_pos = backward_one_grapheme(m_text.c_str(), m_text.length(), pos, &wc);
            width_displaced += wc;
            len_displaced += (pos - new_pos);
            pos = new_pos;
        }

        m_trail_scroller_width_displaced = width_displaced;
        m_trail_scroller_len_displayed = len_displaced;
    }
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
    // NOTE:  Horizontal scroll markers work differently and are applied
    // directly in build().
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

        // Apply scroll marker to last row.
        display_line& d = *m_lines[m_top + rows - 1];
        assert(d.m_trail_scroller_width_displaced);
        assert(d.m_trail_scroller_len_displayed);

        // NOTE: build() should always pad to max width, except on last
        // line, and max width should always be at least 8.
        assert(d.width() > c_horz_scroll_indicator_chars);
        // if (d.width() > 2)
        if (d.m_trail_scroller_width_displaced && d.m_trail_scroller_len_displayed)
        {
            const uint32_t num = c_horz_scroll_indicator_chars;
            uint32_t width_displaced = d.m_trail_scroller_width_displaced;
            const uint32_t len_displaced = d.m_trail_scroller_len_displayed;

            d.m_text.set_length(d.m_text.length() - len_displaced);
            d.m_faces.set_length(d.m_faces.length() - len_displaced);

            for (uint32_t i = num; i--;)
            {
                d.append(">", 1, 1, FACE_SCROLLER);
                --width_displaced;
            }
            while (width_displaced--)
                d.append(" ", 1, 1, FACE_DEFAULT);
        }
    }

    // Discard lines before the visible section.
    if (m_lines.size() > rows)
    {
        m_top = int32_t(m_lines.size() - size_t(rows));
        m_lines.erase(m_lines.begin(), m_lines.begin() + m_top);
        assert(m_lines.size() == rows);

        // Apply scroll marker to first row.
        display_line& d = *m_lines[0].get();

        // NOTE: build() should always pad to max width, except on last line.
        assert(d.m_text.length());
        // if (!d.m_text.length())
        // {
        //     for (uint32_t num = c_horz_scroll_indicator_chars; num--;)
        //         d.append("<", 1, 1, FACE_SCROLLER);
        // }
        // else
        {
            const uint32_t num = c_horz_scroll_indicator_chars;
            uint32_t width_displaced = 0;
            uint32_t len_displaced = 0;
            wcwidth_iter iter_top(d.m_text.c_str(), d.m_text.length());
            while (iter_top.next())
            {
                auto wc = iter_top.character_wcwidth_onectrl();
                auto bytes = iter_top.character_length();

                width_displaced += wc;
                len_displaced += bytes;

                if (width_displaced >= num && len_displaced >= num)
                    break;
            }

            // d.m_lead_scroller_width = c_horz_scroll_indicator_chars;

            for (uint32_t i = 0; i < num && i < d.m_text.length(); ++i)
            {
                assert(width_displaced);
                assert(len_displaced);

                d.m_text.set_at(i, '<');
                d.m_faces.set_at(i, FACE_SCROLLER);
                --width_displaced;
                --len_displaced;
            }

            if (len_displaced > 0)
            {
                d.m_text.delete_range(num, len_displaced);
                d.m_faces.delete_range(num, len_displaced);
            }
            while (width_displaced-- > 0)
                d.append(" ", 1, 1, FACE_DEFAULT);
        }
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

void display_manager::init_callbacks(editor_callbacks* callbacks)
{
    m_callbacks = callbacks;
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
    const uint16_t b_left_width = b->get_left_width();
    const uint16_t b_right_width = b->get_right_width();
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

    for (uint16_t i = 0; i < lines.m_lines.size(); ++i)
    {
        auto const& line = lines.m_lines[i];

        // Does the new line exactly match the previously displayed line?
        size_t begin = 0;
        size_t end = line->m_text.length();
        uint16_t begin_width = 0;
        bool reuse_displayed_line = false;
        if (m_displayed.m_change_counter && i < m_displayed.m_lines.size())
        {
            const auto& displayed = m_displayed.m_lines[i];
            if (displayed->m_x1 == line->m_x1)
            {
                reuse_displayed_line = true;

                // First do a simple memcmp comparison to check if the new
                // line exactly matches the previously displayed line.  The
                // grapheme comparison for a whole line is more than 3 orders
                // of magnitude slower than the memcmp comparison.
                if (line->m_text.equals(displayed->m_text) && line->m_faces.equals(displayed->m_faces))
                    continue;

                const size_t limit_forward_skip = min(displayed->m_text.length(), line->m_text.length());

                // Walk forward past a leading portion that exactly matches.
                size_t displayed_begin = 0;
                const size_t displayed_length = displayed->m_text.length();
                while (begin < end && displayed_begin < limit_forward_skip)
                {
                    uint16_t width;
                    const size_t next = forward_one_grapheme(line->m_text.c_str(), end, uint32_t(begin), &width);
                    const size_t displayed_next = forward_one_grapheme(displayed->m_text.c_str(), displayed_length, uint32_t(displayed_begin));
                    const size_t length = next - begin;
                    const size_t displayed_grapheme_length = displayed_next - displayed_begin;
                    if (length != displayed_grapheme_length ||
                        memcmp(line->m_text.c_str() + begin, displayed->m_text.c_str() + displayed_begin, length) != 0 ||
                        memcmp(line->m_faces.c_str() + begin, displayed->m_faces.c_str() + displayed_begin, length) != 0)
                        break;

                    begin = next;
                    displayed_begin = displayed_next;
                    begin_width += width;
                }

                // Walk backward past a trailing portion that exactly matches.
                // REVIEW: Instead of giving up if the x2 columns differ, it
                // could walk backwards to find the greatest column less than
                // or equal to `displayed->m_x2` that is a grapheme boundary
                // for both `displayed` and `line`.  It could always print
                // everything starting from there, but it could also walk
                // backwards from there to find a shorter middle region to
                // update.  But is that really a common enough case to justify
                // the extra complexity and effort?
                // REVIEW: But if the text is the full terminal width, then it
                // could use ICH (CSI Ps @) and DCH (CSI Ps P) to shift
                // characters instead of printing the whole line.  I'm not
                // sure how much it really matters for performance, but it
                // could reduce the number of bytes printed to the terminal.
                if (displayed->m_x2 == line->m_x2)
                {
                    size_t displayed_end = displayed_length;
                    while (end > begin && displayed_end > displayed_begin)
                    {
                        const size_t previous = backward_one_grapheme(line->m_text.c_str(), line->m_text.length(), uint32_t(end));
                        const size_t displayed_previous = backward_one_grapheme(displayed->m_text.c_str(), displayed_length, uint32_t(displayed_end));
                        const size_t length = end - previous;
                        const size_t displayed_grapheme_length = displayed_end - displayed_previous;
                        if (length != displayed_grapheme_length ||
                            memcmp(line->m_text.c_str() + previous, displayed->m_text.c_str() + displayed_previous, length) != 0 ||
                            memcmp(line->m_faces.c_str() + previous, displayed->m_faces.c_str() + displayed_previous, length) != 0)
                            break;

                        end = previous;
                        displayed_end = displayed_previous;
                    }

                    // If the new line exactly matches the previously
                    // displayed line, then there's nothing to do for that
                    // line.
                    if (reuse_displayed_line && begin == end && displayed->m_x2 == line->m_x2)
                        continue;
                }
            }
        }

        // Move the cursor to the start of the text to display.
        move_to_row(cursor, i, lines.m_inner_offset.y);
        move_to_column(cursor, begin_width, lines.m_inner_offset.x);

        // Display the text.
        char face = 0;
        const char* t = line->m_text.c_str() + begin;
        const char* f = line->m_faces.c_str() + begin;
        for (size_t len = end - begin; len > 0;)
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
            if (reuse_displayed_line)
                move_to_column(cursor, line->width(), lines.m_inner_offset.x);
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

    cstring faces;
    faces.append_spaces(text.length());     // FACE_DEFAULT == space.
    if (m_callbacks)
        m_callbacks->provide_faces(*m_buffer, faces);

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
        const border_definition& b = *m_style->border;
        const uint16_t b_height = !!b.has_top() + !!b.has_bottom();
        tmp.m_inner_offset.y = b.has_top() ? 1 : 0;
        tmp.m_inner_offset.x = b.get_left_width();
        tmp.m_extent.x += b.get_left_width() + b.get_right_width();
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
        // Add horizontal scroll marker if needed.
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
    else if (tmp.m_lines.back()->width() == max_size.x)
    {
        // In multiline mode, if the last line takes up the full width, then
        // there's a phantom blank line at the end.
        tmp.m_lines.emplace_back(std::move(std::make_unique<display_line>(m_origin.x)));
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

    if (multiline)
    {
        // Scroll vertically when cursor is on a multiline scroll marker.
        if (y_extent < tmp.m_lines.size())
        {
            for (size_t i = 0; i <= 2; ++i)
            {
                size_t bottom = tmp.m_top + y_extent;
                if (bottom >= i && bottom - i < tmp.m_lines.size())
                    tmp.m_lines[bottom - i]->calculate_multiline_scroll_marker();
            }

            if (tmp.m_top == tmp.m_cursor.y)
            {
                if (tmp.m_top > 0)
                {
                    const display_line& d = *tmp.m_lines[tmp.m_top];
                    if (tmp.m_cursor.x < c_horz_scroll_indicator_chars)
                        --tmp.m_top;
                }
            }
            else if (tmp.m_top + y_extent - 1 == tmp.m_cursor.y)
            {
                if (tmp.m_top + y_extent < tmp.m_lines.size())
                {
                    const display_line& d = *tmp.m_lines[tmp.m_top + y_extent - 1];
                    if (d.m_trail_scroller_width_displaced &&
                        tmp.m_cursor.x >= d.width() - d.m_trail_scroller_width_displaced)
                        ++tmp.m_top;
                }
            }
        }
        else
        {
            tmp.m_top = 0;
        }

        // Apply scroll markers.
        if (y_extent > 1 && y_extent < tmp.m_lines.size())
            tmp.apply_scroll_markers(y_extent);

        // Handle fixed height mode.
        while (tmp.m_lines.size() < y_extent)
            tmp.m_lines.emplace_back(std::move(std::make_unique<display_line>(m_origin.x)));
    }

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
    const uint16_t b_left_width = b.get_left_width();
    const uint16_t b_right_width = b.get_right_width();
    const uint16_t extra_border_width = b_left_width + b_right_width;
    const uint16_t extra_border_height = b.has_top() + b.has_bottom();
    const coord max_size = get_effective_max_size();
    if (max_size.x <= 0 || max_size.y <= 0)
        return;
    assert(extent.x == max_size.x + extra_border_width);
    assert(max_size.y >= extent.y - extra_border_height);

    output_color(m_colors->get_color(tib::color_element::border));

    if (b.has_top())
    {
        outputf("\x1b[%uG", m_origin.x);
        if (b.top_left)
            output(b.top_left);
        const int16_t top_width = b.get_top_width();
        for (int32_t i = extent.x - (b.get_top_left_width() + b.get_top_right_width()); i - top_width >= 0; i -= top_width)
            output(b.top);
        if (b.top_right)
            output(b.top_right);
    }

    for (uint32_t i = extent.y - extra_border_height; i--;)
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

    if (b.has_bottom())
    {
        outputf("\r\n\x1b[%uG", m_origin.x);
        if (b.bottom_left)
            output(b.bottom_left);
        const int16_t bottom_width = b.get_bottom_width();
        for (int32_t i = extent.x - (b.get_bottom_left_width() + b.get_bottom_right_width()); i - bottom_width >= 0; i -= bottom_width)
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
