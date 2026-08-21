// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"

namespace tib {

struct selection_state
{
                    selection_state() : m_anchor(0), m_caret(0), m_dirty(false) { reset_word_anchor(); }
                    selection_state(textpos_t caret) : m_anchor(caret), m_caret(caret), m_dirty(false) { reset_word_anchor(); }
                    selection_state(textpos_t anchor, textpos_t caret) : m_anchor(anchor), m_caret(caret), m_dirty(false) { reset_word_anchor(); }

    void            set_caret(textpos_t caret) { set_selection(caret, caret); }
    void            set_selection(textpos_t anchor, textpos_t caret);
#if 0
    void            reset_word_anchor() { m_word_anchor_begin = m_anchor; m_word_anchor_end = m_caret; }
    void            reset_word_anchor(textpos_t caret) { m_word_anchor_begin = m_anchor; m_word_anchor_end = caret; }
#else
    void            reset_word_anchor() {}
#endif

    textpos_t       get_anchor() const { return m_anchor; }
    textpos_t       get_caret() const { return m_caret; }
    textpos_t       get_sel_begin() const { return min(m_anchor, m_caret); }
    textpos_t       get_sel_end() const { return max(m_anchor, m_caret); }
#if 0
    int             get_word_anchor_begin() const { return m_word_anchor_begin; }
    int             get_word_anchor_end() const { return m_word_anchor_end; }
#endif
    bool            has_selection() const { return m_anchor != m_caret; }

    bool            is_dirty() const { return m_dirty; }
    void            clear_dirty() { m_dirty = false; }

    textpos_t&      get_anchor_out() { return m_anchor; }
    textpos_t&      get_caret_out() { return m_caret; }

private:
    textpos_t       m_anchor;
    textpos_t       m_caret;
#if 0
    short           m_word_anchor_begin;
    short           m_word_anchor_end;
#endif
    bool            m_dirty;
};

class input_buffer
{
public:
                        ~input_buffer() = default;
                        input_buffer() = default;

    textpos_t           get_caret() const { return m_selection.get_caret(); }
    const selection_state& get_selection_state() const { return m_selection; }

    const cstring&      get_text() const { return m_text; }
    uint32_t            get_change_counter() const { return m_change_counter; }

protected:
    cstring             m_text;
    selection_state     m_selection;
    uint32_t            m_change_counter = 0;
};

} // namespace tib
