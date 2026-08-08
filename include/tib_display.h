// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"
#include "tib_output.h"
#include "tib_context.h"
#include <vector>

namespace tib {

struct display_line
{
    uint16_t            m_left;
    uint16_t            m_right;
    const char*         m_faces;
    const char*         m_text;
    size_t              m_length;
};

struct display_lines
{
    void                clear();

    cstring             m_faces;
    cstring             m_text;
    uint16_t            m_pos = 0;
    uint32_t            m_change_counter = 0;

    std::vector<display_line> m_lines;
    coord               m_cursor = { -1, -1 };
};

class display_manager
{
public:
                        ~display_manager() = default;
                        display_manager() = default;

    void                init(layout_info* layout);
    bool                set(const cstring& text, const cstring& faces, uint16_t pos, uint32_t change_counter);
    bool                display();

private:
    layout_info*        m_layout = nullptr;
    display_lines       m_old;
    display_lines       m_new;
    // TODO: split text and faces into display_lines.
};

} // namespace tib
