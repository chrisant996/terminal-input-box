// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"
#include "tib_buffer.h"
#include "tib_colors.h"
#include "tib_output.h"
#include "wcwidth.h"
#include <memory>
#include <vector>
#include <map>

class wcwidth_iter;

namespace tib {

extern bool g_coalesce_output;

struct border_definition
{
    bool                has_top() const { return this && top && *top; }
    bool                has_bottom() const { return this && bottom && *bottom; }
    bool                has_left() const { return this && left && *left; }
    bool                has_right() const { return this && right && *right; }

    // These allow a border_definition to include embedded ANSI escape
    // sequences without requiring terminal-input-box to include an ECMA-48
    // compliant escape sequence parser.
    int8_t              get_top_left_width() const { return !this ? 0 : get_width(top_left, top_left_width); }
    int8_t              get_top_width() const { return !this ? 0 : get_width(top, top_width); }
    int8_t              get_top_right_width() const { return !this ? 0 : get_width(top_right, top_right_width); }
    int8_t              get_left_width() const { return !this ? 0 : get_width(left, left_width); }
    int8_t              get_right_width() const { return !this ? 0 : get_width(right, right_width); }
    int8_t              get_bottom_left_width() const { return !this ? 0 : get_width(bottom_left, bottom_left_width); }
    int8_t              get_bottom_width() const { return !this ? 0 : get_width(bottom, bottom_width); }
    int8_t              get_bottom_right_width() const { return !this ? 0 : get_width(bottom_right, bottom_right_width); }

    const char*         top_left = nullptr;
    const char*         top = nullptr;
    const char*         top_right = nullptr;
    const char*         left = nullptr;
    const char*         right = nullptr;
    const char*         bottom_left = nullptr;
    const char*         bottom = nullptr;
    const char*         bottom_right = nullptr;

    int8_t              top_left_width = -1;
    int8_t              top_width = -1;
    int8_t              top_right_width = -1;
    int8_t              left_width = -1;
    int8_t              right_width = -1;
    int8_t              bottom_left_width = -1;
    int8_t              bottom_width = -1;
    int8_t              bottom_right_width = -1;

private:
    int8_t              get_width(const char* s, int8_t width) const;
};

extern const border_definition c_light_border;

constexpr char FACE_DEFAULT     = 0x20;
constexpr char FACE_SELECTION   = 0x1f;
constexpr char FACE_SCROLLER    = 0x1e;
constexpr char FACE_EMPTY       = 0;
struct editor_callbacks;
typedef std::map<char, const char*> face_definitions;

struct layout_info
{
    uint16_t            max_width = INT16_MAX;
    uint16_t            max_height = 1;
    bool                variable_height = false;
};

struct style_info
{
    bool                horiz_scroll_markers = true;
    const border_definition* border = nullptr;
    char                empty_face = FACE_EMPTY;
};

struct display_line
{
                        ~display_line() = default;
                        display_line(uint16_t x1);
    void                append(const char* p, uint32_t len, uint32_t width, char face);
    uint16_t            width() const { return m_x2 - m_x1; }
    void                calculate_multiline_scroll_marker();

    cstring             m_text;
    cstring             m_faces;
    uint16_t            m_x1 = 0;       // First column in the line (1-based, inclusive).
    uint16_t            m_x2 = 0;       // Last column in the line (1-based, EXCLUSIVE).
#if 0
    uint16_t            m_lead = 0;     // Number of leading columns (e.g. wrapped part of ^X).
    uint16_t            m_trail = 0;    // Number of trailing columns of spaces past m_lastcol.
#endif
    uint8_t             m_trail_scroller_width_displaced = 0;
    uint8_t             m_trail_scroller_len_displayed = 0;
};

struct display_lines
{
    void                clear();
    void                apply_scroll_markers(int16_t rows);

    int32_t             m_top = 0;
    textpos_t           m_pos = 0;
    textpos_t           m_left = 0;
    size_t              m_selection_length = 0;
    uint32_t            m_change_counter = 0;

    std::vector<std::unique_ptr<display_line>> m_lines;
    coord               m_cursor = { -1, -1 };  // Offset from m_inner_offset.

    coord               m_inner_offset = { 0, 0 };
    coord               m_extent = { 0, 0 };

    bool                m_erase = false;
};

class input_buffer;

class display_manager
{
public:
                        ~display_manager() = default;
                        display_manager();

    void                init_layout(const layout_info* layout);
    void                init_buffer(const input_buffer* buffer);
    void                init_style(const style_info* style);
    void                init_faces(const face_definitions* face_defs);
    void                init_callbacks(editor_callbacks* callbacks);

    coord               get_origin() const { return m_origin; }
    void                set_origin(int32_t x=-1, int32_t y=-1);

    std::shared_ptr<const color_table> get_color_table() const;
    void                set_color_table(std::shared_ptr<const color_table> colors);

    coord               get_effective_max_size(bool omit_scroll_markers=false);
    coord               get_relative_cursor() const { return m_relative_cursor; }
    coord               get_extent() const;

    textpos_t           get_left() const { return m_left; }
    void                clear_scroll_offsets();

    void                invalidate() { m_displayed.m_change_counter = 0; }
    void                invalidate_border() { m_border_dirty = true; }
    bool                display();
    void                erase_display();
    void                move_to_end_of_display();
    void                move_to_caret_position();

private:
    void                move_to_row(coord& cursor, uint16_t y, uint16_t inner_offset);
    void                move_to_column(coord& cursor, uint16_t x, uint16_t inner_offset);
    const char*         get_face_def(char face) const;
    bool                display_internal(display_lines& lines);
    void                ensure_left();
    bool                build(display_lines& out);
    void                append_border(coord extent);

    void                output(const char* s, size_t len=-1);
    void                outputf(const char* format, ...);
    void                output_color(const char* color);
    void                output_spaces(size_t n);
    void                maybe_flush();

private:
    const layout_info*  m_layout = nullptr;         // Borrowed.
    const input_buffer* m_buffer = nullptr;         // Borrowed.
    const style_info*   m_style = nullptr;          // Borrowed.
    const face_definitions* m_face_defs = nullptr;  // Borrowed.
    editor_callbacks*   m_callbacks = nullptr;      // Borrowed.
    coord               m_origin = { -1, -1 };
    coord               m_term_size;
    std::shared_ptr<const color_table> m_colors;
    display_lines       m_displayed;
    uint32_t            m_top = 0;                  // Vertical scroll top.
    textpos_t           m_left = 0;                 // Horizontal scroll left.
    bool                m_border_dirty = false;
    coord               m_relative_cursor = { -1, -1 };

    cstring             m_accumulator;
    bool                m_coalesce_output = false;

    std::vector<grapheme_info> m_tmp_graphemes;
};

} // namespace tib
