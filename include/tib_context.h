// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"
#include "tib_bindings.h"
#include "tib_buffer.h"
#include "tib_colors.h"
#include "tib_display.h"
#include <vector>

struct grapheme_info;

namespace tib {

textpos_t pos_mover(const char* s, const size_t _len, textpos_t& pos, const bool forward, const bool word);

struct editor_callbacks : public std::enable_shared_from_this<editor_callbacks>
{
    virtual void        provide_faces(const input_buffer& buffer, cstring& faces) {}
};

struct undo_entry
{
                        undo_entry() = default;
                        ~undo_entry();
    void                link_at_tail(undo_entry*& head, undo_entry*& tail);
    void                unlink(undo_entry*& head, undo_entry*& tail);

    cstring             m_text;
    selection_state     m_sel_before;
    selection_state     m_sel_after;

    undo_entry*         m_prev = nullptr;
    undo_entry*         m_next = nullptr;
};

class editor_context : public input_buffer
{
public:
                        ~editor_context();
                        editor_context();

    void                set_max_length(uint32_t m) { m_max_length = static_cast<textpos_t>(min<uint32_t>(m, INT16_MAX)); }
    void                set_max_width(uint16_t m) { m_layout.max_width = static_cast<textpos_t>(min<uint16_t>(m, INT16_MAX)); }
    void                set_max_height(uint16_t m) { m_layout.max_height = static_cast<textpos_t>(min<uint16_t>(m, INT16_MAX)); }
    void                set_variable_height(bool v) { m_layout.variable_height = v; }
    void                set_border(const border_definition* border);
    void                set_horiz_scroll_markers(bool show) { m_style.horiz_scroll_markers = show; }
#if 0
    void                set_history(std::vector<StrW>* history);
#endif
    coord               get_origin() const { return m_display.get_origin(); }
    void                set_origin(int16_t x=-1, int16_t y=-1) { m_display.set_origin(x, y); }
    coord               get_relative_cursor() const { return m_display.get_relative_cursor(); }
    coord               get_extent() const { return m_display.get_extent(); }

    void                initialize_text(const char* text=nullptr, size_t len=c_auto_length);
    std::shared_ptr<const key_table_list> get_bindings() const;
    void                set_bindings(std::shared_ptr<const key_table_list> bindings);
    void                set_callbacks(editor_callbacks* callbacks);
    std::shared_ptr<const color_table> get_color_table() const;
    void                set_color_table(std::shared_ptr<const color_table> colors);
    void                set_face_defs(const face_definitions* face_defs);
    void                set_empty_face(char face);

#if 0
    int32_t             go(void* cookie=nullptr);
#endif
    int32_t             do_binding_target(const binding_target* target, int32_t c);
    void                display();
    void                erase_display();
    void                move_to_end_of_display();
    void                move_to_caret_position();

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
    void                inc_change_counter();

private:
    // NOTE:  Content and selection are contained in the base class.

    // Configuration.
    std::shared_ptr<const key_table_list> m_bindings;
    editor_callbacks*   m_callbacks = nullptr;      // Borrowed.
    layout_info         m_layout;   // REVIEW: does tib_context actually need access to this?
    style_info          m_style;    // REVIEW: does tib_context actually need access to this?
    uint32_t            m_max_length = INT16_MAX;

    // State.
    uint16_t            m_terminal_row = 0;
#if 0
    MouseHelper         m_mouse_helper;
#endif
    bool                m_can_drag = false;

    // Display.
    display_manager     m_display;
    std::vector<grapheme_info> m_tmp_graphemes;

    // Undo/redo queue.
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
};

} // namespace tib
