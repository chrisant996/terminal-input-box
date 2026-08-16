// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include <stdio.h>

#include "maybe_windows.h"
#include "tib.h"
#include "tib_host.h"
#include <wcwidth.h>
#include <assert.h>

static const char c_long_usage[] =
"Flags:\n"
"  --single          Single line input mode (default).\n"
"  --multiline       Multiple line input mode (max height 3 lines).\n"
"  --fixed           Fixed height input mode (default).\n"
"  --variable        Variable height input mode (implies --multiline).\n"
"  --full-width      Use the full terminal width (default is 40).\n"
"  --no-border       No border (default; same as '--border none').\n"
"  --border STYLE    Use border style:  light, padding, bar-padding, none.\n"
"  --rainbow         Apply rainbow colors to words.\n"
;

static tib_host::auto_terminal_init s_auto_terminal_init;

static bool s_done = false;

static bool s_use_rainbow_faces = false;

static const char* const c_bar_text_color = "0;38;2;180;140;33";
static const char* const c_border_text_color = "0;38;2;33;33;33";
constexpr char FACE_CTRL = '^';
constexpr char FACE_RAINBOW = '\x80';

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
        out.printf("\033[%sm", c_bar_text_color);
        out.append(iter.character_pointer(), iter.character_length());
        iter.next();
        out.printf("\033[%sm", c_border_text_color);
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

struct color_t
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

static const color_t c_colors[] =
{
    { 0xcc, 0x00, 0x00 },
    { 0xcc, 0x99, 0x00 },
    { 0xcc, 0xcc, 0x00 },
    { 0x00, 0xcc, 0x00 },
    { 0x00, 0xcc, 0xcc },
    { 0x00, 0x66, 0xcc },
    { 0xcc, 0x00, 0xcc },
};

class custom_input_box : public tib::input_box, protected tib::editor_callbacks
{
public:
                        ~custom_input_box() = default;
                        custom_input_box();

protected:
    void                provide_faces(const tib::input_buffer& buffer, tib::cstring& faces);
};

custom_input_box::custom_input_box()
{
    set_callbacks(this);
}

void custom_input_box::provide_faces(const tib::input_buffer& buffer, tib::cstring& faces)
{
    if (s_use_rainbow_faces)
    {
        uint8_t c = 0;
        bool space = true;
        const tib::cstring& text = buffer.get_text();
        const char* s = text.c_str();
        const size_t len = text.length();
        assert(len == faces.length());
        for (size_t i = 0; i < text.length(); ++i)
        {
            if (s[i] < ' ')
            {
                faces.set_at(i, FACE_CTRL);
            }
            else
            {
                if (!space && s[i] == ' ')
                    c = (c + 1) % std::size(c_colors);
                space = (s[i] == ' ');

                if (!space)
                    faces.set_at(i, FACE_RAINBOW + c);
            }
        }
    }
}

static void display_key_sequence(tib::editor_context& ctx, const tib::cstring& show_sequence)
{
    const tib::coord origin = ctx.get_origin();
    const tib::coord cursor = ctx.get_relative_cursor();
    const tib::coord extent = ctx.get_extent();
    const int32_t vert = extent.y - cursor.y;

    ctx.move_to_end_of_display();

    tib::cstring tmp;
    if (show_sequence.length())
    {
        tmp.append_color("36");
        tmp.append("\033[4Gkeys:  ");
        tmp.append(show_sequence.c_str(), show_sequence.length());
        tmp.append_color("");
    }
    tmp.append("\033[K");
    tib::term_out(tmp.c_str(), tmp.length());

    ctx.move_to_caret_position();
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

    const char c_norm_base[] = "0";
    const char c_padding_base[] = "0;48;2;33;33;33";

    std::shared_ptr<tib::color_table> colors = std::make_shared<tib::color_table>();
    colors->set_color(tib::color_element::base, c_norm_base);
    colors->set_color(tib::color_element::border, "0;38;2;33;33;33");

    tib::face_definitions face_defs;
    face_defs.emplace(tib::FACE_SELECTION, "0;7");
    face_defs.emplace(tib::FACE_SCROLLER, "0;7;36");
    face_defs.emplace(tib::FACE_EMPTY, c_norm_base);
    face_defs.emplace(FACE_CTRL, "0;36;44");

    custom_input_box tib;
    tib.set_bindings(make_basic_key_table());
    tib.set_max_width(40);

    const tib::border_definition* border = nullptr;
    for (int i = 0; i < argc; ++i)
    {
        if (_stricmp(argv[i], "--single") == 0)
        {
            tib.set_max_height(1);
            tib.set_variable_height(false);
        }
        else if (_stricmp(argv[i], "--multiline") == 0)
        {
            tib.set_max_height(3);
        }
        else if (_stricmp(argv[i], "--fixed") == 0)
        {
            tib.set_variable_height(false);
        }
        else if (_stricmp(argv[i], "--variable") == 0)
        {
            tib.set_max_height(3);
            tib.set_variable_height(true);
        }
        else if (_stricmp(argv[i], "--full-width") == 0)
        {
            tib.set_max_width(INT16_MAX);
        }
        else if (_stricmp(argv[i], "--no-border") == 0)
        {
no_border:
            tib.set_empty_face(tib::FACE_EMPTY);
            colors->set_color(tib::color_element::base, c_norm_base);
            border = nullptr;
        }
        else if (_stricmp(argv[i], "--border") == 0)
        {
            ++i;
            if (i < argc)
            {
                if (_stricmp(argv[i], "light") == 0)
                {
                    tib.set_empty_face(tib::FACE_DEFAULT);
                    colors->set_color(tib::color_element::base, c_norm_base);
                    border = &tib::c_light_border;
                }
                else if (_stricmp(argv[i], "padding") == 0)
                {
                    tib.set_empty_face(tib::FACE_DEFAULT);
                    colors->set_color(tib::color_element::base, c_padding_base);
                    border = &c_padding_border;
                }
                else if (_stricmp(argv[i], "bar-padding") == 0)
                {
                    tib.set_empty_face(tib::FACE_DEFAULT);
                    colors->set_color(tib::color_element::base, c_padding_base);
                    border = &c_bar_padding_border;
                }
                else if (_stricmp(argv[i], "none") == 0)
                {
                    goto no_border;
                }
                else
                {
                    fputs("Unrecognized border style.\n", stderr);
                    return 1;
                }
            }
            else
            {
                fputs("Missing border style.\n", stderr);
                return 1;
            }
        }
        else if (_stricmp(argv[i], "--rainbow") == 0)
        {
            s_use_rainbow_faces = true;
        }
        else if (_stricmp(argv[i], "-?") == 0 ||
                 _stricmp(argv[i], "--help") == 0)
        {
            fprintf(stdout, "%s", c_long_usage);
            return 0;
        }
        else
        {
            fprintf(stderr, "Unrecognized %s '%s'.\n", (argv[i][0] == '-') ? "flag" : "argument", argv[i]);
            return 1;
        }
    }

    face_defs.emplace(tib::FACE_DEFAULT, colors->get_color(tib::color_element::base));

    tib::cstring border_face_scroller;
    if (border)
    {
        border_face_scroller.set(colors->get_color(tib::color_element::base));
        border_face_scroller.append_color("36");
        face_defs[tib::FACE_SCROLLER] = border_face_scroller.c_str();
    }

    std::vector<tib::cstring> rainbow_colors;
    if (s_use_rainbow_faces)
    {
        for (uint8_t i = 0; i < std::size(c_colors); ++i)
        {
            tib::cstring tmp;
            tmp.set(colors->get_color(tib::color_element::base));
            tmp.printf("\033[38;2;%u;%u;%um", c_colors[i].r, c_colors[i].g, c_colors[i].b);
            static_assert(std::is_nothrow_move_constructible_v<tib::cstring>);
            static_assert(std::is_nothrow_move_assignable_v<tib::cstring>);
            rainbow_colors.emplace_back(std::move(tmp));
        }
        for (uint8_t i = 0; i < std::size(c_colors); ++i)
        {
            face_defs[FACE_RAINBOW + i] = rainbow_colors[i].c_str();
        }
    }

    tib.set_color_table(colors);
    tib.set_face_defs(&face_defs);
    tib.set_border(border);

#ifdef _WIN32
    // CONSOLE_SCREEN_BUFFER_INFO csbi;
    // GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    // tib.set_origin(csbi.dwCursorPosition.X + 1);
#else
    // TODO-LINUX: Query the terminal for the current position.
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
            display_key_sequence(tib, show_sequence);

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

        switch (dispatcher.step(c, &tib))
        {
        case tib::dispatch_outcome::self_insert:
            if (sequence.length() == 1/*c*/ + 1/*space*/)
                sequence.clear();
            break;
        case tib::dispatch_outcome::match:
            sequence.clear();
            break;
        }
    }

    show_sequence.clear();
    display_key_sequence(tib, show_sequence);
    tib.erase_display();

    tib::term_out("\r\n", 2);

    return 0;
}
