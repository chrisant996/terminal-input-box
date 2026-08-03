// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"
#include "tib_colors.h"
#include <vector>

namespace tib {

typedef int32_t textpos_t;

#ifdef _WIN32
typedef COORD coord;
#else
typedef struct _coord {
    int16_t X;
    int16_t Y;
} coord;
#endif

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

protected:
    cstring             m_text;
    selection_state     m_selection;
};

struct undo_entry
{
                    undo_entry() = default;
                    ~undo_entry();
    void            link_at_tail(undo_entry*& head, undo_entry*& tail);
    void            unlink(undo_entry*& head, undo_entry*& tail);

    cstring         m_text;
    selection_state m_sel_before;
    selection_state m_sel_after;

    undo_entry*     m_prev = nullptr;
    undo_entry*     m_next = nullptr;
};

class editor_context : public input_buffer
{
public:
                        ~editor_context();
                        editor_context();

    void                set_max_width(uint32_t m) { m_max_width = static_cast<textpos_t>(min<uint32_t>(m, INT16_MAX)); }
    void                set_max_length(uint32_t m) { m_max_length = static_cast<textpos_t>(min<uint32_t>(m, INT16_MAX)); }
#if 0
    void                Set_Callback(std::optional<std::function<int32(const InputRecord&, const ReadInputBuffer&, void*)>> input_callback);
    void                set_history(std::vector<StrW>* history);
#endif
    void                set_origin(coord coord) { m_origin = coord; }
    void                set_horiz_scroll_markers(bool show) { m_horiz_scroll_markers = show; }

    void                initialize_text(const char* text=nullptr, size_t len=c_auto_length);
    std::shared_ptr<const key_table_list> get_bindings() const;
    void                set_bindings(std::shared_ptr<const key_table_list> bindings);
    std::shared_ptr<const color_table> get_color_table() const;
    void                set_color_table(std::shared_ptr<const color_table> colors);

#if 0
    int32_t             go(void* cookie=nullptr);
#endif
    int32_t             do_binding_target(const binding_target* target, int32_t c);

    void                begin_of_input(bool select=false);
    void                end_of_input(bool select=false);
    void                move_left(bool word=false, bool select=false);
    void                move_right(bool word=false, bool select=false);
    void                backspace(bool word=false);
    void                del(bool word=false);

    void                set_selection(textpos_t anchor, textpos_t caret);
    void                select_word();

#if _WIN32
    void                copy_to_clipboard();
    void                cut_to_clipboard();
    void                paste_from_clipboard();
#endif

#if 0
    void                replace_from_history(const cstring& text, bool keep_undo);
#endif
    void                insert_char(char c);
    void                insert_text(const char* text, size_t len=c_auto_length);
    void                remove_text(textpos_t begin, textpos_t end);
    bool                elide_selected_text();

    void                clear_undo() { init_undo(); }
    void                begin_undo_group();
    void                end_undo_group();
    void                undo();
    void                redo();

    void                transfer_text(cstring& out);

#ifdef DEBUG
    void                dump_undo_stack();
#endif

private:
    void                ensure_left();
    void                print_visible();
    void                init_undo();
    void                clear_undo_internal();
    void                unlink_endo_entry(undo_entry* p);

private:
    // NOTE:  Content and selection are contained in the base class.

    // Configuration.
    std::shared_ptr<const key_table_list> m_bindings;
    std::shared_ptr<const color_table> m_colors;
    uint32_t            m_max_width = 0;
    uint32_t            m_max_length = 0;
    coord               m_origin = { -1, -1 };
    bool                m_horiz_scroll_markers = true;

    // State.
    uint32_t            m_change_counter = 0;
    uint16_t            m_terminal_row = 0;
    textpos_t           m_left = 0;
#if 0
    MouseHelper         m_mouse_helper;
#endif
    bool                m_can_drag = false;

    // undo/redo queue.
    undo_entry*         m_undo_head = nullptr;
    undo_entry*         m_undo_tail = nullptr;
    undo_entry*         m_undo_current = nullptr;
    int16_t             m_grouping = 0;  // >0 means an undo group is in progress.
    bool                m_defer_init_undo = false;

    // History.
#if 0
    std::vector<cstring>* m_history = nullptr;
    size_t              m_history_index = 0;
    cstring             m_curr_input_history;
#endif

    // Callback.
#if 0
    std::optional<std::function<int32(const InputRecord&, const ReadInputBuffer&, void*)>> m_callback;
#endif
};

} // namespace tib
