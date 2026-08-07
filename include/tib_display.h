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
    std::vector<uint8_t> m_faces;
    std::vector<char>   m_text;
};

struct display_lines
{
    std::vector<display_line> m_lines;
    coord               m_cursor;
};

class display_manager
{
public:
                        ~display_manager() = default;
                        display_manager() = default;

    void                init(layout_info* layout);
    bool                set(const cstring& text, const uint8_t* faces, uint32_t change_counter);
    bool                display();

private:
    layout_info*        m_layout = nullptr;
    display_lines       m_old;
    display_lines       m_new;
    // TODO: change counter.
    // TODO: split text and faces into display_lines.
};

} // namespace tib
