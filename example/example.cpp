// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include <stdio.h>

#include "maybe_windows.h"
#include "tib.h"
#include "tib_host.h"
#include <wcwidth.h>
#include <assert.h>

static tib_host::auto_terminal_init s_auto_terminal_init;

static bool s_done = false;

static const char* const c_bar_text_color = "0;38;2;180;140;33";
static const char* const c_border_text_color = "0;38;2;33;33;33";

static const tib::border_definition c_padding_border =
{
    nullptr,    "▄",    nullptr,
    "██",               "██",
    nullptr,    "▀",    nullptr,

    0,          1,      0,
    2,                  2,
    0,          1,      0,
};

struct bar_padding_border_definition : public tib::border_definition
{
    bar_padding_border_definition()
    {
        make_bar("▗▄", custom_top_left, top_left, top_left_width);
        make_bar("▐█", custom_left, left, left_width);
        make_bar("▝▀", custom_bottom_left, bottom_left, bottom_left_width);

        top_right = "▖";        top_right_width = 1;
        right = "█▌";           right_width = 2;
        bottom_right = "▘";     bottom_right_width = 1;

        top = "▄";              top_width = 1;
        bottom = "▀";           bottom_width = 1;
    };

protected:
    static void make_bar(const char* in, tib::cstring& out, const char*& dst, int8_t& dst_width)
    {
        wcwidth_iter iter(in);
        iter.next();
        out.printf("\x1b[%sm", c_bar_text_color);
        out.append(iter.character_pointer(), iter.character_length());
        iter.next();
        out.printf("\x1b[%sm", c_border_text_color);
        out.append(iter.character_pointer(), iter.character_length());
        dst = out.c_str();
        dst_width = 2;
    }

private:
    tib::cstring custom_top_left;
    tib::cstring custom_left;
    tib::cstring custom_bottom_left;
};

static const bar_padding_border_definition c_bar_padding_border;

static int32_t backspace(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.backspace();
    return 0;
}

static int32_t del_word_left(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.backspace(true/*word*/);
    return 0;
}

static int32_t accept_line(tib::editor_context& ctx, int32_t key, const char* name)
{
    s_done = true;
    return 0;
}

static int32_t begin_of_line(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.begin_of_input();
    return 0;
}

static int32_t end_of_line(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.end_of_input();
    return 0;
}

static int32_t backward_char(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_left();
    return 0;
}

static int32_t forward_char(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_right();
    return 0;
}

static int32_t backward_word(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_left(true/*word*/);
    return 0;
}

static int32_t forward_word(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_right(true/*word*/);
    return 0;
}

static int32_t del_char_right(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.del();
    return 0;
}

static int32_t del_word_right(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.del(true/*word*/);
    return 0;
}

static int32_t redo(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.redo();
    return 0;
}

static int32_t undo(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.undo();
    return 0;
}

static int32_t select_all(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.set_selection(0, uint16_t(ctx.get_text().length()));
    return 0;
}

static int32_t cua_begin_of_line(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.begin_of_input(true/*select*/);
    return 0;
}

static int32_t cua_end_of_line(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.end_of_input(true/*select*/);
    return 0;
}

static int32_t cua_backward_char(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_left(false/*word*/, true/*select*/);
    return 0;
}

static int32_t cua_forward_char(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_right(false/*word*/, true/*select*/);
    return 0;
}

static int32_t cua_backward_word(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_left(true/*word*/, true/*select*/);
    return 0;
}

static int32_t cua_forward_word(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_right(true/*word*/, true/*select*/);
    return 0;
}

static int32_t cut(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.cut_to_clipboard();
    return 0;
}

static int32_t copy(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.copy_to_clipboard();
    return 0;
}

static int32_t paste(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.paste_from_clipboard();
    return 0;
}

static int32_t lorem_ipsum(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.insert_text(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua.  Ut enim "
        "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
        "aliquip ex ea commodo consequat.  Duis aute irure dolor in "
        "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
        "pariatur.  Excepteur sint occaecat cupidatat non proident, sunt in "
        "culpa qui officia deserunt mollit anim id est laborum.");
    return 0;
}

std::shared_ptr<tib::key_table_list> make_basic_key_table()
{
    auto t = std::make_shared<tib::key_table>();

    t->add({ "\177", tib::binding_target(backspace) });     // VT sends 0x7F for Backspace.
    t->add({ "\001", tib::binding_target(select_all) });
    t->add({ "\003", tib::binding_target(copy) });
    t->add({ "\010", tib::binding_target(del_word_left) }); // VT sends 0x08 for Ctrl-Backspace.
    t->add({ "\r", tib::binding_target(accept_line) });
    t->add({ "\022", tib::binding_target(lorem_ipsum) });
    t->add({ "\024", tib::binding_target("Macro Text") });
    t->add({ "\026", tib::binding_target(paste) });
    t->add({ "\030", tib::binding_target(cut) });
    t->add({ "\031", tib::binding_target(redo) });
    t->add({ "\032", tib::binding_target(undo) });
    t->add({ "\033[H", tib::binding_target(begin_of_line) });
    t->add({ "\033[F", tib::binding_target(end_of_line) });
    t->add({ "\033[D", tib::binding_target(backward_char) });
    t->add({ "\033[C", tib::binding_target(forward_char) });
    t->add({ "\033[1;5D", tib::binding_target(backward_word) });
    t->add({ "\033[1;5C", tib::binding_target(forward_word) });
    t->add({ "\033[1;2H", tib::binding_target(cua_begin_of_line) });
    t->add({ "\033[1;2F", tib::binding_target(cua_end_of_line) });
    t->add({ "\033[1;2D", tib::binding_target(cua_backward_char) });
    t->add({ "\033[1;2C", tib::binding_target(cua_forward_char) });
    t->add({ "\033[1;6D", tib::binding_target(cua_backward_word) });
    t->add({ "\033[1;6C", tib::binding_target(cua_forward_word) });
    t->add({ "\033[3~", tib::binding_target(del_char_right) });
    t->add({ "\033[3;5~", tib::binding_target(del_word_right) });

    auto tables = std::make_shared<tib::key_table_list>();
    tables->emplace_back(std::move(t));
    return tables;
}

class custom_input_box : public tib::input_box, protected tib::editor_callbacks
{
public:
                        ~custom_input_box() = default;
                        custom_input_box();

protected:
    void                provide_faces(const tib::input_buffer& buffer, tib::cstring& faces);

private:
    tib::cstring        m_prev_text;
    tib::cstring        m_prev_faces;
    char                m_curr_face;
};

custom_input_box::custom_input_box()
{
    m_curr_face = 'a';
    set_callbacks(this);
}

void custom_input_box::provide_faces(const tib::input_buffer& buffer, tib::cstring& faces)
{
#if 0
    const char* old_ptr = m_prev_text.c_str();
    uint32_t old_len = uint32_t(m_prev_text.length());
    const char* new_ptr = buffer.get_text().c_str();
    uint32_t new_len = uint32_t(buffer.get_text().length());
    const char* old_begin_ptr = old_ptr;
    const char* new_begin_ptr = new_ptr;

    wcwidth_iter iter_old(old_ptr, old_len);
    wcwidth_iter iter_new(new_ptr, new_len);
    while (iter_old.next() && iter_new.next())
    {
        old_begin_ptr = iter_old.character_pointer();
        new_begin_ptr = iter_new.character_pointer();
        if (iter_old.character_length() != iter_new.character_length())
            break;
        if (memcmp(iter_old.character_pointer(), iter_new.character_pointer(), iter_new.character_length()))
            break;
    }

    uint32_t old_end_pos = old_len;
    uint32_t new_end_pos = new_len;
    while (true)
    {
        uint32_t old_end_candidate = backward_one_grapheme(old_ptr, old_len, old_end_pos);
        uint32_t new_end_candidate = backward_one_grapheme(new_ptr, new_len, new_end_pos);
        if (old_end_pos - old_end_candidate != new_end_pos - new_end_candidate)
            break;
        if (memcmp(old_ptr + old_end_candidate, new_ptr + new_end_candidate, new_end_pos - new_end_candidate))
            break;
        old_end_pos = old_end_candidate;
        new_end_pos = new_end_candidate;
        if (old_end_pos == old_begin_ptr - old_ptr || new_end_pos == new_begin_ptr - new_ptr)
            break;
    }

    const char* new_end_ptr = new_ptr + new_end_pos;
    for (ptrdiff_t i = 0; i < new_begin_ptr - new_ptr; ++i)
        faces.set_at(i, m_prev_faces.c_str()[i]);
    for (ptrdiff_t i = new_begin_ptr - new_ptr; i < new_end_ptr - new_ptr; ++i)
        faces.set_at(i, m_curr_face);
    for (ptrdiff_t i = new_end_ptr - new_ptr; i < new_len; ++i)
        faces.set_at(i, m_prev_faces.c_str()[i - new_len + old_len]);

    if (new_end_ptr > new_begin_ptr)
    {
        ++m_curr_face;
        if (m_curr_face > 'g')
            m_curr_face = 'a';
    }

    m_prev_text = buffer.get_text();
    m_prev_faces = faces;
#endif
}

int main(int argc, const char** argv)
{
    --argc, ++argv;

    {
        tib::cstring v;
        if (tib::getenv("TIB_NO_COALESCE_OUTPUT", v) && !v.empty())
            tib::g_coalesce_output = !(atoi(v.c_str()) > 0);
    }

    tib_host::set_crt_locale_utf8();
    tib_host::set_console_vt_input();
    reset_wcwidths();

    const tib::border_definition* border = nullptr;
    // border = &tib::c_light_border;
    // border = &c_padding_border;
    border = &c_bar_padding_border;

    std::shared_ptr<tib::color_table> colors = std::make_shared<tib::color_table>();
    colors->set_color(tib::color_element::base, "0;48;2;33;33;33");
    colors->set_color(tib::color_element::border, "0;38;2;33;33;33");

    tib::face_definitions face_defs;
    face_defs.emplace(tib::FACE_DEFAULT, colors->get_color(tib::color_element::base));
    face_defs.emplace(tib::FACE_SELECTION, "0;7");
    face_defs.emplace(tib::FACE_SCROLLER, "0;48;2;33;33;33;36");
    if (border == &c_padding_border)
        face_defs.emplace(tib::FACE_EMPTY, colors->get_color(tib::color_element::border));
    face_defs.emplace('a', "0;48;2;33;33;33;38;2;255;0;0");
    face_defs.emplace('b', "0;48;2;33;33;33;38;2;255;128;0");
    face_defs.emplace('c', "0;48;2;33;33;33;38;2;255;255;0");
    face_defs.emplace('d', "0;48;2;33;33;33;38;2;0;255;0");
    face_defs.emplace('e', "0;48;2;33;33;33;38;2;0;160;255");
    face_defs.emplace('f', "0;48;2;33;33;33;38;2;0;96;255");
    face_defs.emplace('g', "0;48;2;33;33;33;38;2;128;0;255");

    custom_input_box tib;
    tib.set_bindings(make_basic_key_table());
    tib.set_border(border);
    if (border == &c_padding_border)
        tib.set_empty_face(tib::FACE_DEFAULT);
    tib.set_color_table(colors);
    tib.set_face_defs(&face_defs);
    tib.set_max_height(3);
    tib.set_variable_height(true);
    tib.set_max_width(40);

#ifdef _WIN32
    // CONSOLE_SCREEN_BUFFER_INFO csbi;
    // GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    // tib.set_origin(csbi.dwCursorPosition.X + 1);
#else
    // TODO:  Query the terminal for the current position.
#endif

    tib.initialize_text("hello world");
    tib.set_selection(0, uint16_t(tib.get_text().length()));

    tib::dispatcher dispatcher;
    dispatcher.init(tib.get_bindings());

    tib::cstring tmp;
    tmp.set("\n\033[A");
    tib::term_out(tmp.c_str(), tmp.length());

    tib::cstring sequence;
    tib::cstring show_sequence;
    double last_clock = tib::clock();

    while (!s_done)
    {
        tib.display();

        if (!show_sequence.empty())
        {
            const tib::coord origin = tib.get_origin();
            const tib::coord cursor = tib.get_relative_cursor();
            const tib::coord extent = tib.get_extent();

            tmp.set("\033[s");
            if (cursor.y < extent.y - 1)
                tmp.printf("\x1b[%uB", (extent.y - 1) - cursor.y);
            else if (cursor.y > extent.y - 1)
                tmp.printf("\x1b[%uA", cursor.y - (extent.y - 1));
            tmp.append_color(colors->get_color(tib::color_element::border));
            tmp.append_color("7;46");
            tmp.append("\x1b[4G keys:  ");
            tmp.append(show_sequence.c_str(), show_sequence.length());
            tmp.append_color(colors->get_color(tib::color_element::border));
            for (size_t n = 25 - show_sequence.length(); n--;)
                tmp.append(border ? border->bottom : " ");
            tmp.append("\033[u");
            tib::term_out(tmp.c_str(), tmp.length());
            tib.set_border(border);
        }

        const int32_t c = tib::term_in();

        const double now = tib::clock();
        if (now - last_clock >= 0.1)
            sequence.clear();
        last_clock = now;
        if (c > 0x20 && c < 0x7f)
            sequence.printf("%c ", char(c));
        else
            sequence.printf("0x%02.2x ", c);
        while (sequence.length() > 25)
        {
            const char* p = sequence.c_str();
            while (*p && *p != ' ')
                ++p;
            while (*p && *p == ' ')
                ++p;
            tmp.set(p);
            sequence = std::move(tmp);
        }
        show_sequence.set(sequence.c_str(), sequence.length());

        switch (dispatcher.step(c))
        {
        case tib::dispatch_outcome::miss:
            if (!(c & 0xffffff00))
            {
                // Insert the char into the input_box.
// TODO: optimize for typeahead insertion (with undo group).
                tib.insert_char(char(c));
                if (sequence.length() == 1/*c*/ + 1/*space*/)
                    sequence.clear();
            }
            break;
        case tib::dispatch_outcome::more:
            break;
        case tib::dispatch_outcome::match:
            {
                const auto target = dispatcher.get_target();
                assert(target);
                tib.do_binding_target(target, c);
                sequence.clear();
            }
            break;
        }
    }

    tib.erase_display();

    tib::term_out("\r\n", 2);

    return 0;
}
