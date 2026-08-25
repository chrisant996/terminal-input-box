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
"  --show-keys       Show input key sequences.\n"
;

static tib_host::auto_terminal_init s_auto_terminal_init;

#pragma region Example customizations.
static bool s_use_rainbow_faces = false;
static bool s_show_keys = false;

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
#pragma endregion // Example customizations.

std::shared_ptr<tib::key_table_list> make_key_tables()
{
    auto t = std::make_shared<tib::key_table>();
    t->add({ "\022", tib::binding_target_func("lorem-ipsum") });
    t->add({ "\024", tib::binding_target_macro("Macro Text") });

    auto tables = tib::make_default_key_table();
    tables->emplace_back(t);
    return tables;
}

class custom_input_box : public tib::input_box, protected tib::editor_callbacks
{
public:
                        ~custom_input_box() = default;
                        custom_input_box();

protected:
                        // Methods on the tib::editor_callbacks interface.
    void                provide_faces(const tib::input_buffer& buffer, tib::cstring& faces);
};

custom_input_box::custom_input_box()
{
    tib::ensure_term_caps();
    set_callbacks(this);
}

#pragma region Example customizations.
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
            if (s[i] >= 0 && s[i] < ' ')
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

static void display_key_sequence(tib::editor_context& ctx, const tib::cstring& show_sequence, const tib::coord* old_extent=nullptr)
{
    const tib::coord origin = ctx.get_origin();
    const tib::coord cursor = ctx.get_relative_cursor();
    const tib::coord extent = ctx.get_extent();
    const int32_t vert = extent.y - cursor.y;

    if (tib::g_show_hide_cursor)
        tib::term_out(tib::c_hide_cursor);
    ctx.move_to_end_of_display();

    tib::cstring tmp;
    if (show_sequence.length())
    {
        tmp.append_color("36");
        tmp.append(tib::term_col(4));
        tmp.append("keys:  ");
        tmp.append(show_sequence.c_str(), show_sequence.length());
        tmp.append_color("");
    }
    tmp.append(tib::term_erase_to_eol());

    if (old_extent && old_extent->y > extent.y)
    {
        const uint32_t delta = old_extent->y - extent.y;
        for (uint32_t n = delta; n--;)
            tmp.append("\n");
        tmp.append(tib::term_erase_to_eol());
        tmp.append(tib::term_move_up(delta));
    }

    tib::term_out(tmp.c_str(), tmp.length());

    ctx.move_to_caret_position();
    if (tib::g_show_hide_cursor)
        tib::term_out(tib::c_show_cursor);
}
#pragma endregion // Example customizations.

int main(int argc, const char** argv)
{
    --argc, ++argv;

    {
        tib::cstring v;
        if (tib::getenv("TIB_NO_COALESCE_OUTPUT", v) && !v.empty())
        {
            tib::g_coalesce_output = !(atoi(v.c_str()) > 0);
            tib::g_show_hide_cursor = !(atoi(v.c_str()) > 0);
        }
    }

    tib_host::set_crt_locale_utf8();
    tib_host::set_console_vt_input();
    reset_wcwidths();

    tib::term_begin();

    std::shared_ptr<custom_input_box> tib = std::make_shared<custom_input_box>();
    tib->set_bindings(make_key_tables());
    tib->set_max_width(40);

#pragma region Example customizations.
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

    const tib::border_definition* border = nullptr;
    tib::mouse_input_mode mode = tib::mouse_input_mode::none;
    bool sgr_encoding = true;
    bool set_mouse_input_mode = false;

    for (int i = 0; i < argc; ++i)
    {
        if (_stricmp(argv[i], "--single") == 0)
        {
            tib->set_max_height(1);
            tib->set_variable_height(false);
        }
        else if (_stricmp(argv[i], "--multiline") == 0)
        {
            tib->set_max_height(3);
        }
        else if (_stricmp(argv[i], "--fixed") == 0)
        {
            tib->set_variable_height(false);
        }
        else if (_stricmp(argv[i], "--variable") == 0)
        {
            tib->set_max_height(3);
            tib->set_variable_height(true);
        }
        else if (_stricmp(argv[i], "--full-width") == 0)
        {
            tib->set_max_width(INT16_MAX);
        }
        else if (_stricmp(argv[i], "--no-border") == 0)
        {
no_border:
            tib->set_empty_face(tib::FACE_EMPTY);
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
                    tib->set_empty_face(tib::FACE_DEFAULT);
                    colors->set_color(tib::color_element::base, c_norm_base);
                    border = &tib::c_light_border;
                }
                else if (_stricmp(argv[i], "padding") == 0)
                {
                    tib->set_empty_face(tib::FACE_DEFAULT);
                    colors->set_color(tib::color_element::base, c_padding_base);
                    border = &c_padding_border;
                }
                else if (_stricmp(argv[i], "bar-padding") == 0)
                {
                    tib->set_empty_face(tib::FACE_DEFAULT);
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
        else if (_stricmp(argv[i], "--show-keys") == 0)
        {
            s_show_keys = true;
        }
        else if (_stricmp(argv[i], "--mouse") == 0)
        {
            ++i;
            if (i < argc)
            {
                set_mouse_input_mode = true;
                if (_stricmp(argv[i], "none") == 0)
                    mode = tib::mouse_input_mode::none;
                else if (_stricmp(argv[i], "vt200") == 0)
                    mode = tib::mouse_input_mode::VT200;
                else if (_stricmp(argv[i], "drag") == 0)
                    mode = tib::mouse_input_mode::DRAG;
                else if (_stricmp(argv[i], "any") == 0)
                    mode = tib::mouse_input_mode::ANY;
                else
                {
                    fputs("Unrecognized mouse input mode.\n", stderr);
                    return 1;
                }
            }
            else
            {
                fputs("Missing mouse input mode.\n", stderr);
                return 1;
            }
        }
        else if (_stricmp(argv[i], "--enc") == 0)
        {
            ++i;
            if (i < argc)
            {
                set_mouse_input_mode = true;
                if (_stricmp(argv[i], "default") == 0)
                    sgr_encoding = false;
                else if (_stricmp(argv[i], "sgr") == 0)
                    sgr_encoding = true;
                else
                {
                    fputs("Unrecognized mouse input encoding.\n", stderr);
                    return 1;
                }
            }
            else
            {
                fputs("Missing mouse input encoding.\n", stderr);
                return 1;
            }
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

    if (set_mouse_input_mode)
    {
        tib::enable_mouse_input(mode, sgr_encoding);
        // tib::term_out("\x1b[?1000h");   // VT200 Protocol for mouse input.
        // tib::term_out("\x1b[?1000l");
        // tib::term_out("\x1b[?1006h");   // SGR Encoding for mouse input.
        // tib::term_out("\x1b[?1006l");
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
            tmp.printf("\x1b[38;2;%u;%u;%um", c_colors[i].r, c_colors[i].g, c_colors[i].b);
            static_assert(std::is_nothrow_move_constructible_v<tib::cstring>);
            static_assert(std::is_nothrow_move_assignable_v<tib::cstring>);
            rainbow_colors.emplace_back(std::move(tmp));
        }
        for (uint8_t i = 0; i < std::size(c_colors); ++i)
        {
            face_defs[FACE_RAINBOW + i] = rainbow_colors[i].c_str();
        }
    }

    tib->set_color_table(colors);
    tib->set_face_defs(&face_defs);
    tib->set_border(border);
#pragma endregion // Example customizations.

    tib->initialize("hello world");
    tib->set_selection(0, uint16_t(tib->get_text().length()));

#pragma region Show input sequence.
    tib::cstring tmp;
    tib::cstring sequence;
    tib::cstring show_sequence;
    tib::coord old_extent = tib->get_extent();
    double last_clock = tib::clock();

    auto show_sequence_after_display = [&]()
    {
        if (!s_show_keys)
            return;
        display_key_sequence(*tib, show_sequence, &old_extent);
    };

    auto update_sequence_before_step = [&](int32_t c)
    {
        if (!s_show_keys)
            return;
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
    };

    auto update_sequence_after_step = [&](tib::dispatch_outcome outcome)
    {
        if (!s_show_keys)
            return;
        switch (outcome)
        {
        case tib::dispatch_outcome::self_insert:
            if (sequence.length() == 1/*c*/ + 1/*space*/)
                sequence.clear();
            break;
        case tib::dispatch_outcome::match:
            sequence.clear();
            break;
        }

        old_extent = tib->get_extent();
    };
#pragma endregion // Show input sequence.

    tib::binding_resolver resolver;                         // Required.
    resolver.add_target(tib);                               // Required.

    while (!tib->done())                                    // Required.
    {
        tib->display();                                     // Required.

                /*Custom*/  show_sequence_after_display();

        const int32_t c = tib::term_in();                   // Required.
        if (c == tib::c_input_terminal_eof)
            break;

                /*Custom*/  update_sequence_before_step(c);

        auto resolved = resolver.step(c);                   // Required.
        resolved.dispatch();                                // Required.

                /*Custom*/  update_sequence_after_step(resolved.outcome);
    }

#pragma region Show input sequence.
    if (s_show_keys)
    {
        show_sequence.clear();
        display_key_sequence(*tib, show_sequence);
    }
#pragma endregion // Show input sequence.

    tib->erase_display();
    tib::term_out("\r\n", 2);

    return 0;
}
