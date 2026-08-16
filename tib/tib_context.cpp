// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib.h"
#include "wcwidth.h"
#include <assert.h>

namespace tib {

undo_entry::~undo_entry()
{
    assert(!m_prev);
    assert(!m_next);
}

void undo_entry::link_at_tail(undo_entry*& head, undo_entry*& tail)
{
    assert(!m_prev);
    assert(!m_next);
    m_prev = tail;
    if (tail)
        tail->m_next = this;
    if (!head)
        head = this;
    tail = this;
}

void undo_entry::unlink(undo_entry*& head, undo_entry*& tail)
{
    if (m_prev)
        m_prev->m_next = m_next;
    else
        head = m_next;
    if (m_next)
        m_next->m_prev = m_prev;
    else
        tail = m_prev;
    m_prev = nullptr;
    m_next = nullptr;
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

static bool is_space(char c)
{
    // FUTURE:  Various other appropriate Unicode blank space codepoints?
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

textpos_t pos_mover(const char* s, const size_t _len, textpos_t& pos, const bool forward, const bool word)
{
    assert(pos >= 0);
    const textpos_t len = textpos_t(_len);
    assert(len >= 0);
    assert(pos <= len);

    // Try to make sure pos is at a valid codepoint boundary.
    // BUGBUG: this does not handle invalid UTF8 well.
    if (pos > 0 && pos < len)
    {
        const textpos_t prev = backward_one_grapheme(s, _len, pos);
        const textpos_t next = forward_one_grapheme(s, _len, prev);
        if (forward)
        {
            if (next > pos)
                pos = prev;
        }
        else
        {
            if (next > pos)
                pos = next;
        }
    }

    const textpos_t orig_pos = pos;

    if (forward)
    {
        if (pos < len)
        {
            if (!word)
            {
                pos = forward_one_grapheme(s, _len, pos);
            }
            else
            {
                while (pos < len)
                {
                    const textpos_t test_pos = forward_one_grapheme(s, _len, pos);
                    if ( ! (test_pos - pos == 1 && is_space(s[pos])))
                        break;
                    pos = test_pos;
                }
                while (pos < len)
                {
                    const textpos_t test_pos = forward_one_grapheme(s, _len, pos);
                    if (   (test_pos - pos == 1 && is_space(s[pos])))
                        break;
                    pos = test_pos;
                }
            }

            if (pos > len)
                pos = len;
        }
    }
    else
    {
        if (pos > 0)
        {
            if (!word)
            {
                pos = backward_one_grapheme(s, _len, pos);
            }
            else
            {
                while (pos > 0)
                {
                    const textpos_t test_pos = backward_one_grapheme(s, _len, pos);
                    if ( ! (pos - test_pos == 1 && is_space(s[test_pos])))
                        break;
                    pos = test_pos;
                }
                while (pos > 0)
                {
                    const textpos_t test_pos = backward_one_grapheme(s, _len, pos);
                    if (   (pos - test_pos == 1 && is_space(s[test_pos])))
                        break;
                    pos = test_pos;
                }
            }

            if (pos >= len)
                pos = 0;
        }
    }

    const textpos_t begin = min(pos, orig_pos);
    const textpos_t end = max(pos, orig_pos);
    const textpos_t moved = end - begin;
    return moved;
}

editor_context::~editor_context()
{
    clear_undo_internal();
}

editor_context::editor_context()
{
    init_undo();
    m_display.init_buffer(this);
    m_display.init_layout(&m_layout);
    m_display.init_style(&m_style);
}

void editor_context::set_border(const border_definition* border)
{
    if (border && !border->has_top() && !border->has_bottom() && !border->has_left() && !border->has_right())
        border = nullptr;
    if (m_style.border != border)
        m_display.invalidate_border();
    m_style.border = border;
}

#if 0
void editor_context::set_history(std::vector<cstring>* history)
{
    m_history = history;
    m_history_index = m_history ? m_history->size() : 0;
}
#endif

void editor_context::initialize_text(const char* text, size_t len)
{
    if (!text)
    {
        text = "";
        len = 0;
    }
    else
    {
        len = min<size_t>(INT16_MAX, resolve_auto_length(len, text));
    }

    clear_undo_internal();
    m_selection.set_caret(0);
    insert_text(text, len);
    m_selection.clear_dirty();
    m_left = 0;
    init_undo();

    assert(!m_selection.is_dirty());
    assert(m_selection.get_caret() == len);
    assert(m_selection.get_anchor() == len);
    assert(m_text.length() == len);
    assert(!m_defer_init_undo);

#if 0
    m_history_index = m_history ? m_history->size() : 0;
#endif
}

std::shared_ptr<const key_table_list> editor_context::get_bindings() const
{
    return m_bindings;
}

void editor_context::set_bindings(std::shared_ptr<const key_table_list> bindings)
{
    m_bindings = bindings;
}

void editor_context::set_callbacks(editor_callbacks* callbacks)
{
    m_callbacks = callbacks;
    m_display.init_callbacks(m_callbacks);
}

std::shared_ptr<const color_table> editor_context::get_color_table() const
{
    return m_display.get_color_table();
}

void editor_context::set_color_table(std::shared_ptr<const color_table> colors)
{
    m_display.set_color_table(colors);
}

void editor_context::set_face_defs(const face_definitions* face_defs)
{
    m_display.init_faces(face_defs);
}

void editor_context::set_empty_face(char face)
{
    m_style.empty_face = face;
    m_display.invalidate();
}

#if 0
int32_t editor_context::go(void* cookie)
{
    const HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    const HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hout, &csbi);

    if (unsigned(csbi.dwCursorPosition.X) + 8 >= unsigned(csbi.dwSize.X))
        return false;
    if (unsigned(csbi.dwCursorPosition.X) + m_layout.max_width >= unsigned(csbi.dwSize.X))
        m_layout.max_width = csbi.dwSize.X - csbi.dwCursorPosition.X;

    if (m_layout.origin.X < 0 || m_layout.origin.Y < 0)
        SetOrigin(csbi.dwCursorPosition);

    AutoMouseConsoleMode mouse(g_options.allow_mouse);
    m_mouse_helper.ClearClicks();
    m_can_drag = false;

#ifdef DEBUG
    StrW prev_text(m_s);
    uint32 prev_counter = m_change_counter;
#endif

    while (true)
    {
        ensure_left();
        print_visible();

#ifdef DEBUG
        // Verify any time m_s changes then m_change_counter also increases.
        if (!prev_text.Equal(m_s))
        {
            assert(int32(m_change_counter) - int32(prev_counter) > 0);
            prev_text.Set(m_s);
            prev_counter = m_change_counter;
        }
#endif

        const InputRecord input = SelectInput(INFINITE, &mouse);
        switch (input.type)
        {
        case InputType::None:
        case InputType::Error:
            continue;
        case InputType::Resize:
            return false;

        case InputType::Key:
        case InputType::Char:
        case InputType::Mouse:
            if (m_callback)
            {
                const int32 result = (*m_callback)(input, *this, cookie);
                // Negative means break out of the loop.
                if (result < 0)
                    return -1;
                // Positive means do not process (already handled).
                if (result > 0)
                    continue;
                // Zero means allow normal processing.
            }
            switch (HandleInput(input))
            {
            case Outcome::Cancelled:
                return false;
            case Outcome::Done:
                return true;
            case Outcome::DontResetHistoryIndex:
                break;
            case Outcome::ResetHistoryIndex:
                if (m_history && m_history_index < m_history->size())
                    m_history_index = m_history->size();
                break;
            default:
                assert(false);
                break;
            }
            break;

        default:
            assert(false);
            break;
        }
    }
}
#endif

int32_t editor_context::do_binding_target(const binding_target* target, int32_t c)
{
    assert(target);
    if (!target)
        return -1;

    // TODO: m_can_drag.
    // TODO: Whether/when to reset history index.

    switch (target->get_type())
    {
    case tib::binding_type::func:
        {
            const auto func = target->get_func();
            assert(func);
            return func(*this, uint8_t(c), target->get_text());
        }
        break;
    case tib::binding_type::macro:
        term_push_macro_text(target->get_text(), target->get_length());
        break;
    default:
        assert(false);
        break;
    }

    return -1;
}

void editor_context::display()
{
    // TODO: terminal size changes should invalidate.

    // REVIEW: optimize this away as much as possible?
    ensure_left();

    m_display.display();
}

void editor_context::erase_display()
{
    m_display.erase_display();
}

void editor_context::move_to_end_of_display()
{
    m_display.move_to_end_of_display();
}

void editor_context::move_to_caret_position()
{
    m_display.move_to_caret_position();
}

void editor_context::ensure_left()
{
    const coord max_size = m_display.get_effective_max_size(true/*omit_scroll_markers*/);
    if (max_size.y != 1)
    {
        m_left = 0;
        return;
    }

    m_left = min(m_left, m_selection.get_caret());

    // Auto-scroll horizontally forward.
    parse_graphemes(m_text.c_str() + m_left, m_selection.get_caret() - m_left, 0, m_tmp_graphemes);
    uint32_t width = 0;
    for (const auto& g : m_tmp_graphemes)
        width += g.width;
    for (auto g = m_tmp_graphemes.cbegin(); width >= uint32_t(max_size.x); ++g)
    {
        width -= g->width;
        m_left += g->length;
    }

    // Auto-scroll horizontally backward.
    assert(m_selection.get_caret() >= m_left);
    {
        textpos_t backup_left = back_up_by_amount(m_selection.get_caret(), m_text.c_str(), m_selection.get_caret(), 4);
        if (m_left > backup_left)
            m_left = backup_left;
    }
}

void editor_context::begin_of_input(bool select)
{
    if (!select)
        m_selection.set_caret(0);
    else if (!m_selection.has_selection())
        m_selection.set_selection(m_selection.get_caret(), 0);
    else
        m_selection.set_selection(m_selection.get_anchor(), 0);

    m_left = 0;

    if (!select)
        m_selection.reset_word_anchor();
}

void editor_context::end_of_input(bool select)
{
    if (!select)
        m_selection.set_caret(textpos_t(m_text.length()));
    else if (!m_selection.has_selection())
        m_selection.set_selection(m_selection.get_caret(), textpos_t(m_text.length()));
    else
        m_selection.set_selection(m_selection.get_anchor(), textpos_t(m_text.length()));

    if (!select)
        m_selection.reset_word_anchor();
}

void editor_context::move_left(bool word, bool select)
{
    if (!select && m_selection.has_selection())
    {
        m_selection.set_caret(m_selection.get_sel_begin());
    }
    else if (m_selection.get_caret() > 0)
    {
        textpos_t caret = m_selection.get_caret();
        textpos_t anchor = m_selection.get_anchor();
        pos_mover(m_text.c_str(), m_text.length(), caret, false/*forward*/, word);
        m_selection.set_selection(select ? anchor : caret, caret);
    }
    if (!select)
        m_selection.reset_word_anchor();
}

void editor_context::move_right(bool word, bool select)
{
    if (!select && m_selection.has_selection())
    {
        m_selection.set_caret(m_selection.get_sel_end());
    }
    else if (m_selection.get_caret() < m_text.length())
    {
        textpos_t caret = m_selection.get_caret();
        textpos_t anchor = m_selection.get_anchor();
        pos_mover(m_text.c_str(), m_text.length(), caret, true/*forward*/, word);
        m_selection.set_selection(select ? anchor : caret, caret);
    }
    if (!select)
        m_selection.reset_word_anchor();
}

void editor_context::backspace(bool word)
{
    m_selection.reset_word_anchor();
    if (!m_selection.has_selection() && m_selection.get_caret() <= 0)
        return;

    begin_undo_group();

    if (!elide_selected_text())
    {
#ifdef DEBUG
        const textpos_t old_pos = m_selection.get_caret();
#endif
        const textpos_t moved = pos_mover(m_text.c_str(), m_text.length(), m_selection.get_caret_out(), false/*forward*/, word);
#ifdef DEBUG
        assert(old_pos == m_selection.get_caret() + moved);
#endif
        remove_text(m_selection.get_caret(), m_selection.get_caret() + moved);
    }

    end_undo_group();
}

void editor_context::del(bool word)
{
    m_selection.reset_word_anchor();
    if (!m_selection.has_selection() && m_selection.get_caret() >= m_text.length())
        return;

    begin_undo_group();

    if (!elide_selected_text())
    {
        textpos_t del_pos = m_selection.get_caret();
        const textpos_t moved = pos_mover(m_text.c_str(), m_text.length(), del_pos, true/*forward*/, word);
        m_selection.set_caret(del_pos - moved);
        remove_text(m_selection.get_caret(), m_selection.get_caret() + moved);
    }

    end_undo_group();
}

void editor_context::set_selection(textpos_t anchor, textpos_t caret)
{
    m_selection.set_selection(anchor, caret);
}

void editor_context::select_word()
{
    const textpos_t orig_pos = m_selection.get_caret();

    // Look forward for a word.
    move_right(true/*word*/);
    textpos_t end = m_selection.get_caret();
    move_left(true/*word*/);
    const textpos_t high_mid = m_selection.get_caret();

    m_selection.set_caret(orig_pos);

    // Look backward for a word.
    move_left(true/*word*/);
    textpos_t begin = m_selection.get_caret();
    move_right(true/*word*/);
    const textpos_t low_mid = m_selection.get_caret();

    if (high_mid <= orig_pos)
    {
        begin = high_mid;
    }
    else if (low_mid > orig_pos)
    {
        end = low_mid;
    }
    else
    {
        // The position is between two words; select the text between.
        begin = low_mid;
        end = high_mid;
    }

    m_selection.set_selection(begin, end);
}

#ifdef _WIN32
void editor_context::copy_to_clipboard()
{
    if (!m_selection.has_selection())
        return;

    const textpos_t begin = m_selection.get_sel_begin();
    const textpos_t end = m_selection.get_sel_end();
    const textpos_t len = (end - begin);

    cstring_t<WCHAR> tmp;
    if (!to_utf16(m_text.c_str() + begin, len, tmp))
        return;

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT, (len + 1) * sizeof(WCHAR));
    if (mem == nullptr)
        return;

    WCHAR* data = (WCHAR*)GlobalLock(mem);
    memcpy(data, tmp.c_str(), tmp.length() * sizeof(WCHAR));
    data[tmp.length()] = 0;
    GlobalUnlock(mem);

    if (!OpenClipboard(0))
    {
        GlobalFree(mem);
        return;
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, mem);
    CloseClipboard();
}

void editor_context::cut_to_clipboard()
{
    begin_undo_group();
    copy_to_clipboard();
    elide_selected_text();
    end_undo_group();
}

void editor_context::paste_from_clipboard()
{
    if (!OpenClipboard(0))
        return;

    HANDLE mem = GetClipboardData(CF_UNICODETEXT);
    if (mem)
    {
        size_t len = size_t(GlobalSize(mem) / sizeof(WCHAR));
        LPCWSTR data = LPCWSTR(GlobalLock(mem));

        while (len && !data[len - 1])
            --len;

        cstring tmp;
        if (to_utf8(data, len, tmp))
            insert_text(tmp.c_str(), tmp.length());

        GlobalUnlock(mem);
    }

    CloseClipboard();
}
#endif

#if 0
void editor_context::replace_from_history(const cstring& s, bool keep_undo)
{
    inc_change_counter();

    m_text.set(s);
    m_selection.set_caret(m_text.Length());
    m_defer_init_undo = !keep_undo;

    coord max_size = get_effective_max_size();
    max_size.x = max<int16_t>(2, max_size.x);
    m_left = back_up_by_amount(get_caret(), m_text.c_str(), m_left, max_size.x - 1);
}
#endif

void editor_context::insert_char(char c)
{
    if (!c)
        return;

    insert_text(&c, 1);
}

void editor_context::insert_text(const char* s, size_t available)
{
    if (!available)
        return;

    begin_undo_group();

    m_selection.reset_word_anchor();

    elide_selected_text();

    if (available > size_t(m_max_length))
        available = m_max_length;

    textpos_t len = 0;
    wcwidth_iter iter(s, static_cast<textpos_t>(available));
    while (iter.next())
    {
        if (m_text.length() + iter.character_length() > m_max_length)
            break;
        len += iter.character_length();
    }

    inc_change_counter();

    if (m_selection.get_caret() == m_text.length())
    {
        m_text.append(s, len);
        m_selection.set_caret(textpos_t(m_text.length()));
    }
    else
    {
        cstring tmp;
        const int32_t insert_pos = m_selection.get_caret();
        tmp.set(m_text.c_str(), insert_pos);
        tmp.append(s, len);
        m_selection.set_caret(textpos_t(tmp.length()));
        tmp.append(m_text.c_str() + insert_pos, m_text.length() - insert_pos);
        m_text = std::move(tmp);
    }

    end_undo_group();
}

void editor_context::remove_text(textpos_t begin, textpos_t end)
{
    begin_undo_group();

    m_selection.reset_word_anchor();

    inc_change_counter();

    if (end == m_text.length())
    {
        m_text.set_length(begin);
    }
    else
    {
        cstring tmp;
        tmp.append(m_text.c_str(), begin);
        tmp.append(m_text.c_str() + end, m_text.length() - end);
        m_text = std::move(tmp);
    }

    m_selection.set_caret(begin);

    end_undo_group();
}

bool editor_context::elide_selected_text()
{
    if (!m_selection.has_selection())
        return false;

    const textpos_t begin = m_selection.get_sel_begin();
    const textpos_t end = m_selection.get_sel_end();
    remove_text(begin, end);
    return true;
}

void editor_context::clear_undo_internal()
{
    while (m_undo_head)
        unlink_endo_entry(m_undo_head);
    assert(!m_undo_head);
    assert(!m_undo_tail);
    m_undo_current = nullptr;
}

void editor_context::init_undo()
{
    clear_undo_internal();
    m_undo_head = m_undo_tail = new undo_entry;
    m_undo_tail->m_text = m_text;
    m_undo_tail->m_sel_before = m_selection;
    m_undo_tail->m_sel_after = m_selection;
    m_defer_init_undo = false;
}

void editor_context::unlink_endo_entry(undo_entry* p)
{
    p->unlink(m_undo_head, m_undo_tail);
}

void editor_context::inc_change_counter()
{
    ++m_change_counter;
    if (!m_change_counter)
        ++m_change_counter;
}

void editor_context::begin_undo_group()
{
    if (!m_undo_head)
        return;

    assert(m_grouping >= 0);
    if (!m_grouping)
    {
        if (m_defer_init_undo)
            init_undo();

        if (m_undo_current)
        {
            // Keep current, discard everything after current.
            m_undo_current = m_undo_current->m_next;
            while (m_undo_current)
            {
                undo_entry* del = m_undo_current;
                m_undo_current = m_undo_current->m_next;
                unlink_endo_entry(del);
                delete del;
            }
            assert(!m_undo_current);
        }

        undo_entry* p = new undo_entry;
        p->m_sel_before = m_selection;
        p->link_at_tail(m_undo_head, m_undo_tail);
        assert(p == m_undo_tail);
    }
    ++m_grouping;
}

void editor_context::end_undo_group()
{
    if (!m_undo_head)
        return;

    assert(m_grouping > 0);
    --m_grouping;
    if (!m_grouping)
    {
        m_undo_tail->m_text.set(m_text);
        m_undo_tail->m_sel_after = m_selection;
    }
}

void editor_context::undo()
{
    assert(!m_grouping);
    if (m_grouping)
        return;
    if (!m_undo_head)
        return;

    if (!m_undo_current)
        m_undo_current = m_undo_tail;
    undo_entry* p = m_undo_current->m_prev;
    if (!p)
        return;

    inc_change_counter();
    m_text.set(p->m_text);
    m_selection = m_undo_current->m_sel_before;
    m_undo_current = p;
}

void editor_context::redo()
{
    assert(!m_grouping);
    if (m_grouping)
        return;
    if (!m_undo_tail)
        return;

    if (!m_undo_current || m_undo_current == m_undo_tail)
        return;

    undo_entry* r = m_undo_current->m_next;
    assert(r);

    inc_change_counter();
    m_text.set(r->m_text);
    m_selection = r->m_sel_after;

    m_undo_current = r;
}

void editor_context::transfer_text(cstring& out)
{
    out = std::move(m_text);
    initialize_text();
}

#ifdef DEBUG
void editor_context::dump_undo_stack()
{
    puts("");
    for (undo_entry* p = m_undo_head; p; p = p->m_next)
    {
        cstring tag;
        if (p == m_undo_head) tag.append("H");
        if (p == m_undo_tail) tag.append("T");
        if (p == m_undo_current) tag.append("C");
        printf("%s\tcaret %u/%u, anchor %u/%u, text '%s'\n", tag.c_str(), p->m_sel_before.get_caret(), p->m_sel_after.get_caret(), p->m_sel_before.get_anchor(), p->m_sel_after.get_anchor(), p->m_text.c_str());
    }
    printf("----\n");
}
#endif

} // namespace tib
