// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_buffer.h"
#include "tib_output.h"
#include "tib_termcap.h"
#include "tib_display.h"
#include "tib_context.h"
#include "wcwidth.h"
#include <assert.h>

namespace tib {

bool g_coalesce_output = true;
bool g_show_hide_cursor = true;

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

static textpos_t back_up_by_amount(textpos_t pos, const char* s, size_t len, size_t backup)
{
    while (pos > 0 && backup)
    {
        uint16_t width;
        const textpos_t prev = backward_one_grapheme(s, len, pos, &width);
        if (backup < width)
            break;
        pos = prev;
        backup -= width;
    }
    return pos;
}

#ifdef WIDE_HORZ_SCROLL_MARKERS
const uint16_t c_horz_scroll_indicator_chars = 2;
#else
const uint16_t c_horz_scroll_indicator_chars = 1;
#endif

static int16_t get_horiz_scrolled_width(uint16_t width, uint16_t replaced_width)
{
    if (width < replaced_width)
        return width;
    return width - replaced_width + c_horz_scroll_indicator_chars;
}

display_line::display_line(uint16_t x1)
: m_x1(x1)
, m_x2(x1)
{
    assert(m_x1);
}

void display_line::append(const char* p, uint32_t len, uint32_t width, char face)
{
    assert(len != c_auto_length);
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
    m_rows.clear();
    m_cursor = { -1, -1 };

    m_inner_offset = { 0, 0 };
    m_extent = { 0, 0 };
}

void display_lines::apply_scroll_markers(int32_t y_extent, int32_t total_rows)
{
    // NOTE:  Horizontal scroll markers work differently and are applied
    // separately.
    assert(!m_lines.empty());
    if (y_extent <= 1 || y_extent >= total_rows)
        return;

    const bool above = (m_top > 0);
    const bool below = (m_top + y_extent < total_rows);

    // Apply scroll marker to last row.
    if (below)
    {
        display_line& d = *m_lines.back();
        if (!d.m_trail_scroller_width_displaced && !d.m_trail_scroller_len_displayed)
            d.calculate_multiline_scroll_marker();

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

    // Apply scroll marker to first row.
    if (above)
    {
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
}

display_manager::display_manager()
{
    m_term_size = get_terminal_size();
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

coord display_manager::get_effective_max_size(bool omit_scroll_markers)
{
    assert(m_layout);
    if (!m_layout)
    {
nope:
        return { 0, 0 };
    }

    const border_definition* b = m_style ? m_style->border : nullptr;
    const uint16_t b_left_width = b->get_left_width();
    const uint16_t b_right_width = b->get_right_width();
    const uint16_t extra_border_width = b_left_width + b_right_width;
    const uint16_t b_height = !!b->has_top() + !!b->has_bottom();

    coord max_size;
    max_size.x = m_layout->max_width;
    max_size.y = clamp<int16_t>(m_layout->max_height, 0, m_term_size.y - b_height);
    if (m_origin.x + max_size.x + extra_border_width > m_term_size.x)
    {
        if (m_term_size.x <= m_origin.x + extra_border_width)
            goto nope;
        max_size.x = m_term_size.x - (m_origin.x + extra_border_width - 1);
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

coord display_manager::get_inner_extent() const
{
    const border_definition* b = m_style ? m_style->border : nullptr;
    const uint16_t b_left_width = b->get_left_width();
    const uint16_t b_right_width = b->get_right_width();
    const uint16_t extra_border_width = b_left_width + b_right_width;
    const uint16_t b_height = !!b->has_top() + !!b->has_bottom();

    coord inner_extent = m_displayed.m_extent;
    inner_extent.x -= extra_border_width;
    inner_extent.y -= b_height;
    return inner_extent;
}

void display_manager::clear_scroll_offsets()
{
    set_scroll_offsets(0, 0);
}

void display_manager::set_scroll_offsets(textpos_t left, uint32_t top)
{
    m_left = left;
    m_top = top;
    m_hwheel_exclusion = false;
}

bool display_manager::scroll_horizontally(int32_t columns, int32_t cursor_column, selection_state& selection, bool exclude_auto_scroll)
{
    if (!columns || get_effective_max_size().y != 1)
        return false;

    cursor_column -= m_displayed.m_inner_offset.x;

    const cstring& text = m_buffer->get_text();
    const textpos_t length = textpos_t(text.length());
    textpos_t left = m_left;
    uint32_t remaining = uint32_t(columns < 0 ? -columns : columns);

    if (columns > 0)
    {
        while (left < length && remaining)
        {
            uint16_t width;
            const textpos_t next = forward_one_grapheme(text.c_str(), text.length(), left, &width);
            if (next >= length || remaining < width)
                break;

            textpos_t test = next;
            uint32_t available = 0;
            if (m_style->horiz_scroll_markers)
            {
                test = forward_one_grapheme(text.c_str(), text.length(), test, nullptr);
                available = c_horz_scroll_indicator_chars;
            }
            while (test < length && available < uint32_t(max(cursor_column, 0)))
            {
                uint16_t test_width;
                test = forward_one_grapheme(text.c_str(), text.length(), test, &test_width);
                available += test_width;
            }
            if (available < uint32_t(max(cursor_column, 0)))
                break;

            left = next;
            remaining -= width;
        }
    }
    else
    {
        while (left > 0 && remaining)
        {
            uint16_t width;
            const textpos_t prev = backward_one_grapheme(text.c_str(), text.length(), left, &width);
            if (remaining < width)
                break;
            left = prev;
            remaining -= width;
        }
    }

    textpos_t caret = left;
    uint32_t screen_column = 0;
    if (left && m_style->horiz_scroll_markers)
    {
        caret = forward_one_grapheme(text.c_str(), text.length(), left, nullptr);
        screen_column = c_horz_scroll_indicator_chars;
    }

    while (caret < length && screen_column < uint32_t(max(cursor_column, 0)))
    {
        uint16_t width;
        const textpos_t next = forward_one_grapheme(text.c_str(), text.length(), caret, &width);
        if (screen_column + width > uint32_t(cursor_column))
            break;
        caret = next;
        screen_column += width;
    }

    const bool changed = left != m_left || caret != selection.get_caret();
    m_left = left;
    selection.set_caret(caret);
    if (exclude_auto_scroll)
        suppress_auto_horizontal_scroll(selection);
    return changed;
}

void display_manager::suppress_auto_horizontal_scroll(const selection_state& selection)
{
    if (get_effective_max_size().y != 1)
        return;

    m_hwheel_exclusion = true;
    m_hwheel_exclusion_left = m_left;
    m_hwheel_exclusion_caret = selection.get_caret();
    m_hwheel_exclusion_change_counter = m_buffer->get_change_counter();
}

bool display_manager::move_caret_vertically(int32_t rows, int32_t cursor_column, selection_state& selection)
{
    const coord max_size = get_effective_max_size();
    if (!rows || max_size.y <= 1 || !m_displayed.m_change_counter)
        return false;

    const int32_t current_row = m_displayed.m_top + m_displayed.m_cursor.y;
    const int32_t wanted_row = max(current_row + rows, 0);
    const cstring& text = m_buffer->get_text();
    display_row_start target = { 0, false };
    display_row_start last = target;
    int32_t row = 0;
    uint16_t row_width = 0;
    wcwidth_iter scan(text.c_str(), text.length());

    while (scan.more() && row < wanted_row)
    {
        scan.next();
        const char* const p = scan.character_pointer();
        const textpos_t offset = textpos_t(p - text.c_str());
        if (scan.character_wcwidth_signed() < 0)
        {
            if (*p == '\n')
            {
                last = { textpos_t(offset + scan.character_length()), false };
                ++row;
                row_width = 0;
                continue;
            }
            if (row_width + 1 > uint16_t(max_size.x))
            {
                last = { offset, false };
                ++row;
                row_width = 0;
                if (row >= wanted_row)
                    break;
            }
            ++row_width;
            if (row_width + 1 > uint16_t(max_size.x))
            {
                last = { offset, true };
                ++row;
                row_width = 0;
                if (row >= wanted_row)
                    break;
            }
            ++row_width;
        }
        else
        {
            const uint16_t width = scan.character_wcwidth_twoctrl();
            if (row_width + width > uint16_t(max_size.x))
            {
                last = { offset, false };
                ++row;
                row_width = 0;
                if (row >= wanted_row)
                    break;
            }
            row_width += width;
        }
    }

    target = last;
    if (row < wanted_row && row_width == uint16_t(max_size.x))
    {
        target = { textpos_t(text.length()), false };
        ++row;
    }

    cursor_column = max(cursor_column - m_displayed.m_inner_offset.x, 0);
    textpos_t caret = target.offset;
    uint32_t screen_column = 0;
    if (target.pending)
    {
        if (cursor_column)
        {
            caret = forward_one_grapheme(text.c_str(), text.length(), caret, nullptr);
            screen_column = 1;
        }
    }
    while (caret < text.length() && screen_column < uint32_t(cursor_column))
    {
        wcwidth_iter iter(text.c_str() + caret, text.length() - caret);
        const char32_t c = iter.next();
        if (c == '\n')
            break;
        const uint32_t width = iter.character_wcwidth_twoctrl();
        if (screen_column + width > uint32_t(cursor_column))
            break;
        caret += iter.character_length();
        screen_column += width;
    }

    const bool changed = caret != selection.get_caret();
    selection.set_caret(caret);
    return changed;
}

bool display_manager::set_caret_from_screen(uint32_t x, uint32_t y, selection_state& selection, uint32_t drag_scroll_chars, bool word_drag)
{
    if (!m_displayed.m_change_counter)
        return false;

    const border_definition* const border = m_style ? m_style->border : nullptr;
    int32_t screen_origin_x = m_origin.x;
    int32_t screen_origin_y = m_origin.y;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return false;
    screen_origin_x = csbi.dwCursorPosition.X - csbi.srWindow.Left + 1 - m_relative_cursor.x;
    screen_origin_y = csbi.dwCursorPosition.Y - csbi.srWindow.Top + 1 - m_relative_cursor.y;
#else
    if (screen_origin_y <= 0)
        return false;
#endif
    const int32_t left = screen_origin_x + m_displayed.m_inner_offset.x;
    const int32_t top = screen_origin_y + m_displayed.m_inner_offset.y;
    const int32_t width = m_displayed.m_extent.x - m_displayed.m_inner_offset.x - border->get_right_width();
    const int32_t height = m_displayed.m_extent.y - m_displayed.m_inner_offset.y - !!border->has_bottom();
    const bool multiline = get_effective_max_size().y > 1;
    const bool drag = drag_scroll_chars != 0;
    const int32_t column = int32_t(x) - left;
    const int32_t row = (!multiline && drag) ? 0 : int32_t(y) - top;
    if (drag)
    {
        if (multiline && (row < 0 || row >= height))
        {
            const int32_t cursor_column = clamp(column, 0, width - 1) + m_displayed.m_inner_offset.x;
            const int32_t current_row = m_displayed.m_top + m_displayed.m_cursor.y;
            const int32_t target_row = row < 0 ? m_displayed.m_top - 1 : m_displayed.m_top + height;
            return move_caret_vertically(target_row - current_row, cursor_column, selection);
        }
        if (!multiline)
        {
            const bool over_left_indicator = m_left && m_style && m_style->horiz_scroll_markers &&
                                             column < c_horz_scroll_indicator_chars;
            if (column < 0 || over_left_indicator)
            {
                scroll_horizontally(-int32_t(drag_scroll_chars), m_displayed.m_inner_offset.x,
                                    selection, !word_drag);
                if (word_drag)
                    selection.set_caret(m_left);
                return true;
            }
            const display_line* const line = m_displayed.m_lines.empty() ? nullptr : m_displayed.m_lines[0].get();
            const bool has_right_indicator = line && line->m_faces.length() &&
                                             line->m_faces.c_str()[line->m_faces.length() - 1] == FACE_SCROLLER;
            const bool over_right_indicator = has_right_indicator &&
                                              column >= width - c_horz_scroll_indicator_chars;
            if (column >= width || over_right_indicator)
            {
                scroll_horizontally(int32_t(drag_scroll_chars),
                                    m_displayed.m_inner_offset.x + width - 1, selection, !word_drag);
                return true;
            }
        }
    }
    if (column < 0 || column >= width || row < 0 || row >= height || size_t(row) >= m_displayed.m_rows.size())
        return false;

    const cstring& text = m_buffer->get_text();
    const auto set_screen_caret = [&](textpos_t caret)
    {
        selection.set_caret(caret);
        return true;
    };
    const display_row_start& start = m_displayed.m_rows[row];
    textpos_t caret = start.offset;
    uint32_t screen_column = 0;

    if (start.pending)
    {
        if (!column)
            return set_screen_caret(caret);
        caret = forward_one_grapheme(text.c_str(), text.length(), caret, nullptr);
        screen_column = 1;
    }
    else if (m_left && m_style && m_style->horiz_scroll_markers)
    {
        if (column < c_horz_scroll_indicator_chars)
            return set_screen_caret(caret);
        caret = forward_one_grapheme(text.c_str(), text.length(), caret, nullptr);
        screen_column = c_horz_scroll_indicator_chars;
    }

    while (caret < text.length() && screen_column <= uint32_t(column))
    {
        wcwidth_iter iter(text.c_str() + caret, text.length() - caret);
        const char32_t c = iter.next();
        if (c == '\n' && multiline)
            break;

        const uint32_t char_width = iter.character_wcwidth_twoctrl();
        if (screen_column + char_width > uint32_t(column) || screen_column + char_width > uint32_t(width))
            break;
        caret += iter.character_length();
        screen_column += char_width;
    }

    return set_screen_caret(caret);
}

void display_manager::ensure_left()
{
    const coord max_size = get_effective_max_size(true/*omit_scroll_markers*/);
    if (max_size.y != 1)
    {
        m_left = 0;
        return;
    }

    const cstring& text = m_buffer->get_text();
    const selection_state& selection = m_buffer->get_selection_state();
    if (m_hwheel_exclusion)
    {
        if (m_left == m_hwheel_exclusion_left &&
            selection.get_caret() == m_hwheel_exclusion_caret &&
            m_buffer->get_change_counter() == m_hwheel_exclusion_change_counter)
        {
            return;
        }
        m_hwheel_exclusion = false;
    }

    m_left = min(m_left, selection.get_caret());

    // Auto-scroll horizontally forward.
    parse_graphemes(text.c_str() + m_left, selection.get_caret() - m_left, 0, m_tmp_graphemes);
    int16_t width = 0;
    for (const auto& g : m_tmp_graphemes)
        width += g.width;
    for (auto g = m_tmp_graphemes.cbegin(); true; ++g)
    {
        int16_t display_width = width;
        if (m_left && m_style->horiz_scroll_markers && g != m_tmp_graphemes.cend())
            display_width = get_horiz_scrolled_width(width, g->width);
        if (display_width < max_size.x)
            break;

        assert(g != m_tmp_graphemes.cend());
        width -= g->width;
        m_left += g->length;
    }

    // Auto-scroll horizontally backward.
    assert(selection.get_caret() >= m_left);
    {
        textpos_t backup_left = back_up_by_amount(selection.get_caret(), text.c_str(), selection.get_caret(), 4);
        if (m_left > backup_left)
            m_left = backup_left;
    }
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

    // Update awareness of the terminal size.
    const coord term_size = get_terminal_size();
    if (m_term_size != term_size)
    {
        invalidate();
        invalidate_border();
        m_term_size = term_size;
    }

    ensure_left();

    // TODO: Allow host to add their own display_line rows.  Add a method on
    // editor_context to let a host (or subclass) set a list of additional
    // lines to display below the input box.  The editor_context can forward
    // the lines to the display_manager, which will be responsible for doing
    // optimized minimal redisplay of the lines, e.g. either when
    // adding/removing lines or when the vertical extent of the input box
    // changes.  The caller should be able to provide a vector of a struct
    // that defines each additional line.  The struct should have a cstring
    // for the line text, a width of the line text in columns, and a boolean
    // saying whether the additional line should be bounded by the horizontal
    // extent (and origin) of the input box (versus starting at column 1 and
    // owning the full terminal width for the line).  The display_manager can
    // optimize display be remembering whether/where it has displayed each
    // line (i.e. vertical and horizontal position and horizontal extent for
    // each line), and only display/erase changed additional lines.  When
    // displaying these additional lines, if the boolean says the line should
    // be bounded by the horizonal extent, then the display_manager will pad
    // to the horizontal extent, otherwise it will clear to the end of that
    // line.  That will simplify the host showing/clearing its extra rows
    // (something Clink still struggles with), and will also let the example
    // program eliminate the cursor flicker when using its --show-keys flag.

    // Format content into display structures.
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
    const coord term_size = m_term_size;
    const coord max_size = get_effective_max_size();

    if (cursor.x < 0 && cursor.y < 0)
        cursor = { -1, 0 };             // -1 forces move_to_column.

    auto erase_row = [&](int16_t width)
    {
        if (width <= 0)
            return;
        if (m_origin.x + max_size.x - 1 == term_size.x)
        {
            output(term_erase_to_eol());
        }
        else
        {
            output_spaces(width);
            cursor.x += width;
        }
    };

    if (g_show_hide_cursor)
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

            const char32_t c = iter.next();
            const uint32_t clen = iter.character_length();
            assert(clen <= len);

            if (c == 0xfffd)
                output(c_replacement_character);
            else
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
    if (g_show_hide_cursor)
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
    {
        output(term_row_col(m_origin.y + y, 1));
        cursor.x = 0; // REVIEW: is this accurate, or is this supposed to be relative to origin?
    }
    else if (y < cursor.y)
        output(term_move_up(cursor.y - y));
    else if (y > cursor.y)
        output(term_move_down(y - cursor.y));
    else
        return;
    cursor.y = y;
}

void display_manager::move_to_column(coord& cursor, uint16_t x, uint16_t inner_offset)
{
    x += inner_offset;
    const uint16_t term_x = m_origin.x + x;
    if (term_x > 0)
        output(term_col(term_x));
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

    // NOTE:  Terminal size change is noted inside get_effective_max_size()
    // inside editor_context::ensure_left() inside editor_context::display().
    // Waking immediately upon terminal resize requires a custom input hook,
    // but even the default input routine will at least allow redisplay the
    // next time some input becomes available.

    const uint32_t change_counter = m_buffer->get_change_counter();
    const selection_state& sel_state = m_buffer->get_selection_state();
    const textpos_t sel_begin = sel_state.get_sel_begin();
    const textpos_t sel_end = sel_state.get_sel_end();
    const textpos_t pos = sel_state.get_caret();
    const textpos_t left = m_left;

    if (change_counter == m_displayed.m_change_counter &&
        pos == m_displayed.m_pos &&
        left == m_displayed.m_left &&
        sel_end - sel_begin == m_displayed.m_selection_length)
        return false;

    const cstring& text = m_buffer->get_text();

    cstring faces;
    faces.append_spaces(text.length());     // FACE_DEFAULT == space.
    if (m_callbacks)
    {
        m_callbacks->provide_faces(*m_buffer, faces);

        assert(text.length() == faces.length());
        if (faces.length() < text.length())
            faces.append_spaces(text.length() - faces.length());
    }

    // Overlay selection color into faces.
    memset(faces.reserve(0) + sel_begin, FACE_SELECTION, sel_end - sel_begin);

    display_lines tmp;
    tmp.m_pos = pos;
    tmp.m_left = left;
    tmp.m_change_counter = change_counter;
    tmp.m_selection_length = sel_end - sel_begin;

    // Set up border.
    coord term_size = m_term_size;
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
    assert(implies(multiline, !left));

    // Calculate row boundaries without allocating display_line strings.
    // Special cases:  (1) a control character can wrap between its '^' and
    // its second displayed byte, and (2) if the final row is full then an
    // extra phantom row is needed for the cursor to land in.
    std::vector<display_row_start> rows;
    rows.push_back({ left, false });
    uint16_t row_width = 0;
    int32_t y_extent = max_size.y;
    wcwidth_iter scan(text.c_str() + left, text.length() - left);
    const char* const cursor_ptr = text.c_str() + pos;
    while (scan.more())
    {
        scan.next();
        const char* const p = scan.character_pointer();
        const uint32_t clen = scan.character_length();
        const textpos_t offset = textpos_t(p - text.c_str());
        const bool cursor_in_character = (p <= cursor_ptr && cursor_ptr < p + clen);

        if (scan.character_wcwidth_signed() < 0)
        {
            if (cursor_in_character)
            {
                tmp.m_cursor.x = row_width;
                tmp.m_cursor.y = int16_t(rows.size() - 1);
            }

            // Newlines in multiline mode are literal line breaks.
            if (*p == '\n' && multiline)
            {
                rows.push_back({ textpos_t(offset + clen), false });
                row_width = 0;
                continue;
            }

            // Otherwise control characters take two cells:  e.g. "^X".
            assert(clen == 1);
            if (row_width + 1 > uint16_t(max_size.x))
            {
                if (!multiline)
                    break;
                rows.push_back({ offset, false });
                row_width = 0;
            }
            ++row_width;
            if (row_width + 1 > uint16_t(max_size.x))
            {
                if (!multiline)
                    break;
                rows.push_back({ offset, true });
                row_width = 0;
            }
            ++row_width;
        }
        else
        {
            const uint16_t cwidth = scan.character_wcwidth_twoctrl();
            if (row_width + cwidth > uint16_t(max_size.x))
            {
                if (!multiline)
                    break;
                rows.push_back({ offset, false });
                row_width = 0;
            }
            if (cursor_in_character)
            {
                tmp.m_cursor.x = row_width;
                tmp.m_cursor.y = int16_t(rows.size() - 1);
            }
            row_width += cwidth;
        }
    }

    // Determine cursor position.
    if (tmp.m_cursor.x < 0)
    {
        tmp.m_cursor.x = row_width;
        tmp.m_cursor.y = int16_t(rows.size() - 1);
    }
    if (!multiline && left && m_style->horiz_scroll_markers)
    {
        // The leading scroll marker replaces a whole grapheme, so account
        // for the difference between their displayed widths.
        wcwidth_iter iter(text.c_str() + left, text.length() - left);
        iter.next();
        const int16_t replaced_width = iter.character_wcwidth_twoctrl();
        tmp.m_cursor.x = get_horiz_scrolled_width(tmp.m_cursor.x, replaced_width);
    }
    if (tmp.m_cursor.x >= max_size.x)
    {
        assert(multiline);
        tmp.m_cursor.x = 0;
        ++tmp.m_cursor.y;
    }
    assert(implies(multiline, tmp.m_cursor.y >= 0));
    assert(implies(!multiline, tmp.m_cursor.y == 0));

    // In multiline mode, if the last line takes up the full width, then
    // there's a phantom blank line at the end.
    if (multiline && row_width == uint32_t(max_size.x))
        rows.push_back({ textpos_t(text.length()), false });
    assert(implies(!multiline, rows.size() == 1));

    // Determine the height.
    const int32_t total_rows = int32_t(rows.size());
    if (m_layout->variable_height && y_extent > total_rows)
        y_extent = total_rows;
    assert(y_extent > 0);

    // Calculate the visible range before constructing its display lines.
    tmp.m_top = clamp<int32_t>(m_top, tmp.m_cursor.y - (y_extent - 1), tmp.m_cursor.y);
    tmp.m_top = max<int32_t>(tmp.m_top, 0);

    // Lambda for building a display line.
    char pending = 0;
    bool expanding = false;
    auto build_row = [&](size_t index)
    {
        const display_row_start& start = rows[index];
        auto line = std::make_unique<display_line>(m_origin.x);
        wcwidth_iter iter(text.c_str() + start.offset, text.length() - start.offset);
        const char* face = faces.c_str() + start.offset;

        if (start.pending)
        {
            iter.next();
            assert(iter.character_length() == 1);
            const char c = *iter.character_pointer();
            assert(uint8_t(c) < ' ' || uint8_t(c) == 0x7F);
            const char ctrl = (uint8_t(c) < ' ') ? c + '@' : '?';
            line->append(&ctrl, 1, 1, *face);
            face += iter.character_length();
        }

        if (left && m_style->horiz_scroll_markers)
        {
            // Skip the grapheme that the scroller replaces.
            iter.next();
            face += iter.character_length();

            // Append the scroller.
            line->append("<", 1, 1, FACE_SCROLLER);
            if (c_horz_scroll_indicator_chars > 0)
            {
                for (uint16_t num = c_horz_scroll_indicator_chars - 1; num--;)
                    line->append("<", 1, 1, FACE_SCROLLER);
            }
        }

        bool short_circuited = false;
        while (iter.more())
        {
            const char32_t c = iter.next();
            const char* p = iter.character_pointer();
            uint32_t clen = iter.character_length();
            uint32_t cwidth = iter.character_wcwidth_twoctrl();

            if (iter.character_wcwidth_signed() < 0)
            {
                if (*p == '\n' && multiline)
                {
                    face += clen;
                    break;
                }
                else
                {
                    assert(uint8_t(*p) < ' ' || uint8_t(*p) == 0x7F);
                    pending = (uint8_t(*p) < ' ') ? *p + '@' : '?';
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
                break;
            }

            if (c == 0xfffd)
                line->append(c_replacement_character, c_replacement_character_length, cwidth, *face);
            else
                line->append(p, clen, cwidth, *face);

            if (expanding)
            {
                p = &pending;
                assert(clen == 1);
                assert(cwidth == 1);
                expanding = false;
                goto again;
            }

            face += clen;
        }

        if (!multiline) // Is inside the lambda because of short_circuited.
        {
            // Add horizontal scroll marker if needed.
            assert(tmp.m_lines.size() == 0);
            if (short_circuited || iter.more())
            {
                assert(!multiline);
                assert(int32_t(line->width()) < max_size.x);
                while (int32_t(line->width() + 1) < max_size.x)
                    line->append(" ", 1, 1, FACE_DEFAULT);
                line->append(">", 1, 1, FACE_SCROLLER);
                if (c_horz_scroll_indicator_chars > 0)
                {
                    for (uint16_t num = c_horz_scroll_indicator_chars - 1; num--;)
                        line->append(">", 1, 1, FACE_SCROLLER);
                }
            }
        }

        return line;
    };

    // Scroll vertically when cursor is on a multiline scroll marker.
    if (y_extent < total_rows)
    {
        if (tmp.m_top == tmp.m_cursor.y)
        {
            if (tmp.m_top > 0 && tmp.m_cursor.x < c_horz_scroll_indicator_chars)
                --tmp.m_top;
        }
        else if (tmp.m_top + y_extent - 1 == tmp.m_cursor.y &&
                 tmp.m_top + y_extent < total_rows)
        {
            // Must build a row to check precisely where the scroller is in
            // the row (it may not be at the very end, depending on grapheme
            // boundaries).
            auto bottom = build_row(tmp.m_top + y_extent - 1);
            bottom->calculate_multiline_scroll_marker();
            if (bottom->m_trail_scroller_width_displaced &&
                tmp.m_cursor.x >= bottom->width() - bottom->m_trail_scroller_width_displaced)
                ++tmp.m_top;
        }
    }
    else
    {
        tmp.m_top = 0;
    }

    // Build only the rows that will be visible.
    const int32_t end = min<int32_t>(tmp.m_top + y_extent, total_rows);
    for (int32_t i = tmp.m_top; i < end; ++i)
    {
        tmp.m_lines.emplace_back(build_row(i));
        tmp.m_rows.emplace_back(rows[i]);
    }

    // Adjust the cursor to be relative to the origin.
    tmp.m_cursor.y -= tmp.m_top;

    // Apply scroll markers.
    tmp.apply_scroll_markers(y_extent, total_rows);

    // Handle fixed height mode.
    while (tmp.m_lines.size() < y_extent)
    {
        tmp.m_lines.emplace_back(std::move(std::make_unique<display_line>(m_origin.x)));
        tmp.m_rows.push_back({ textpos_t(text.length()), false });
    }

    assert(implies(!multiline, !tmp.m_top));
    assert(implies(!multiline, y_extent == 1));
    assert(implies(!multiline, tmp.m_lines.size() == 1));

    tmp.m_extent.x += max_size.x;
    tmp.m_extent.y += y_extent;

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
        output(term_col(m_origin.x));
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
            output(term_col(m_origin.x));
            output(b.left);
        }
        if (b_right_width)
        {
            output(term_col(m_origin.x + extent.x - b_right_width));
            output(b.right);
        }
    }

    if (b.has_bottom())
    {
        output("\r\n");
        output(term_col(m_origin.x));
        if (b.bottom_left)
            output(b.bottom_left);
        const int16_t bottom_width = b.get_bottom_width();
        for (int32_t i = extent.x - (b.get_bottom_left_width() + b.get_bottom_right_width()); i - bottom_width >= 0; i -= bottom_width)
            output(b.bottom);
        if (b.bottom_right)
            output(b.bottom_right);
    }

    if (extent.y > 1)
        output(term_move_up(extent.y - 1));
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
