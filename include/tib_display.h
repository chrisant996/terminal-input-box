// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"
#include "tib_buffer.h"
#include "tib_colors.h"
#include "tib_output.h"
#include <memory>
#include <vector>
#include <map>

namespace tib {

extern bool g_coalesce_output;

struct border_definition
{
    bool                has_top() const { return top && *top; }
    bool                has_bottom() const { return bottom && *bottom; }
    bool                has_left() const { return left && *left; }
    bool                has_right() const { return right && *right; }

    const char*         top_left;
    const char*         top;
    const char*         top_right;
    const char*         left;
    const char*         right;
    const char*         bottom_left;
    const char*         bottom;
    const char*         bottom_right;
};

extern const border_definition c_light_border;

constexpr char FACE_DEFAULT     = ' ';
constexpr char FACE_SELECTION   = '|';
constexpr char FACE_EMPTY       = 0;
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
    uint16_t            m_x1;
    uint16_t            m_x2;
    const char*         m_faces;
    const char*         m_text;
    uint16_t            m_width;
    size_t              m_length;
};

struct display_lines
{
    void                clear();

    cstring             m_faces;
    cstring             m_text;
    textpos_t           m_pos = 0;
    textpos_t           m_left = 0;
    size_t              m_selection_length = 0;
    uint32_t            m_change_counter = 0;

    std::vector<display_line> m_lines;
    coord               m_cursor = { -1, -1 };

    coord               m_inner_offset = { 0, 0 };
    coord               m_extent = { 0, 0 };
};

class input_buffer;

class display_manager
{
public:
                        ~display_manager() = default;
                        display_manager() = default;

    void                init_layout(const layout_info* layout);
    void                init_buffer(const input_buffer* buffer);
    void                init_style(const style_info* style);
    void                init_faces(const face_definitions* face_defs);
    void                set_origin(int16_t x=-1, int16_t y=-1);

    std::shared_ptr<const color_table> get_color_table() const;
    void                set_color_table(std::shared_ptr<const color_table> colors);

    uint32_t            get_effective_max_width() const;

    void                invalidate() { m_displayed.m_change_counter = 0; }
    void                invalidate_border() { m_border_dirty = true; }
    bool                display();

private:
    void                move_to_row(coord& cursor, uint16_t y, uint16_t inner_offset);
    void                move_to_column(coord& cursor, uint16_t x, uint16_t inner_offset);
    const char*         get_face_def(char face) const;
    bool                build(display_lines& out);
    void                append_border(uint16_t inner_lines);

    void                output(const char* s, size_t len=-1);
    void                outputf(const char* format, ...);
    void                output_color(const char* color);
    void                output_spaces(size_t n);
    void                maybe_flush();

    const layout_info*  m_layout = nullptr;         // Borrowed.
    const input_buffer* m_buffer = nullptr;         // Borrowed.
    const style_info*   m_style = nullptr;          // Borrowed.
    const face_definitions* m_face_defs = nullptr;  // Borrowed.
    coord               m_origin = { -1, -1 };
    std::shared_ptr<const color_table> m_colors;
    display_lines       m_displayed;
    bool                m_border_dirty = false;
    coord               m_relative_cursor = { -1, -1 };

    cstring             m_accumulator;
    bool                m_coalesce_output = false;
};

} // namespace tib
