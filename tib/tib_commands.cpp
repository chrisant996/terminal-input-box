// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_bindings.h"
#include "tib_context.h"
#include <algorithm>
#include <assert.h>
#include <chrono>

namespace tib {

static const char c_cursor_column_operation_var_name[] = "cursor_column_operation";

static bool is_in_string_list(const char* s, const char* const* list)
{
    while (*list)
    {
        if (!strcmp(s, *list))
            return true;
        ++list;
    }
    return false;
}

static int16_t cursor_column_continuation(editor_context& ctx, const char* command_name, const char* operation, const char* const* alt_command_names=nullptr)
{
    static const char c_continuation_var_name[] = "cursor_column_continuation";

    const char* const have_operation = ctx.get_named_value(c_cursor_column_operation_var_name);
    const bool continuing = ((!operation || (operation && have_operation && !strcmp(have_operation, operation))) &&
                             (!strcmp(ctx.get_last_command(), command_name) ||
                              (alt_command_names && is_in_string_list(ctx.get_last_command(), alt_command_names))));

    int32_t cursor_column;
    if (continuing)
    {
        cursor_column = ctx.get_named_value_int(c_continuation_var_name);
    }
    else
    {
        cursor_column = ctx.get_relative_cursor().x;
        ctx.set_named_value_int(c_continuation_var_name, cursor_column);
    }

    ctx.set_named_value(c_cursor_column_operation_var_name, operation);

    return int16_t(cursor_column);
}

//------------------------------------------------------------------------------

int32_t accept_line(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.set_done();
    return 0;
}

int32_t quoted_insert(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    return c_dispatch_request_quoted_insert;
}

//------------------------------------------------------------------------------

static const char* const c_screen_line_commands[] =
{
    "cua-screen-line-down",
    "cua-screen-line-up",
    "screen-line-down",
    "screen-line-up",
    nullptr
};

int32_t begin_of_line(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.begin_of_input();
    return 0;
}

int32_t end_of_line(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.end_of_input();
    return 0;
}

int32_t backward_char(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_left();
    return 0;
}

int32_t forward_char(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_right();
    return 0;
}

int32_t backward_word(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_left(true/*word*/);
    return 0;
}

int32_t forward_word(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_right(true/*word*/);
    return 0;
}

int32_t screen_line_down(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_caret_vertically(1, cursor_column_continuation(ctx, "screen-line-down", nullptr, c_screen_line_commands));
    return 0;
}

int32_t screen_line_up(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_caret_vertically(-1, cursor_column_continuation(ctx, "screen-line-up", nullptr, c_screen_line_commands));
    return 0;
}

//------------------------------------------------------------------------------

int32_t del_char_left(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.backspace();
    return 0;
}

int32_t del_char_right(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.del();
    return 0;
}

int32_t del_word_left(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.backspace(true/*word*/);
    return 0;
}

int32_t del_word_right(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.del(true/*word*/);
    return 0;
}

//------------------------------------------------------------------------------

int32_t redo(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.redo();
    return 0;
}

int32_t undo(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.undo();
    return 0;
}

int32_t undo_all(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.undo_all();
    return 0;
}

//------------------------------------------------------------------------------

int32_t clear_selection(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.clear_selection();
    return 0;
}

int32_t select_all(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.set_selection(0, uint16_t(ctx.get_text().length()));
    return 0;
}

int32_t select_word(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.select_word();
    return 0;
}

int32_t cua_begin_of_line(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.begin_of_input(true/*select*/);
    return 0;
}

int32_t cua_end_of_line(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.end_of_input(true/*select*/);
    return 0;
}

int32_t cua_backward_char(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_left(false/*word*/, true/*select*/);
    return 0;
}

int32_t cua_forward_char(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_right(false/*word*/, true/*select*/);
    return 0;
}

int32_t cua_backward_word(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_left(true/*word*/, true/*select*/);
    return 0;
}

int32_t cua_forward_word(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_right(true/*word*/, true/*select*/);
    return 0;
}

int32_t cua_screen_line_down(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_caret_vertically(1, cursor_column_continuation(ctx, "screen-line-down", nullptr, c_screen_line_commands), true/*select*/);
    return 0;
}

int32_t cua_screen_line_up(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.move_caret_vertically(-1, cursor_column_continuation(ctx, "screen-line-up", nullptr, c_screen_line_commands), true/*select*/);
    return 0;
}

//------------------------------------------------------------------------------

int32_t cut(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.cut_to_clipboard();
    return 0;
}

int32_t copy(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.copy_to_clipboard();
    return 0;
}

int32_t paste(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.paste_from_clipboard();
    return 0;
}

//------------------------------------------------------------------------------

int32_t transpose_chars(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.transpose();
    return 0;
}

int32_t transpose_words(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.transpose(true/*word*/);
    return 0;
}

//------------------------------------------------------------------------------

static bool range_equals(const char* input, size_t len, const cstring& transformed)
{
    return len == transformed.length() && !memcmp(input, transformed.c_str(), len);
}

static int32_t transform_case(editor_context& ctx, transform_mode mode, bool toggle) noexcept
{
    const auto& selection = ctx.get_selection_state();
    const textpos_t anchor = selection.get_anchor();
    const textpos_t caret = selection.get_caret();

    textpos_t begin;
    textpos_t end;
    if (selection.has_selection())
    {
        begin = selection.get_sel_begin();
        end = selection.get_sel_end();
    }
    else
    {
        ctx.move_right(true/*word*/);
        end = ctx.get_caret();
        ctx.move_left(true/*word*/);
        begin = max(ctx.get_caret(), caret);
        ctx.set_selection(anchor, caret);
    }

    if (begin >= end)
        return 0;

    const char* const input = ctx.get_text().c_str() + begin;
    const size_t len = size_t(end - begin);
    cstring transformed;

    if (toggle)
    {
        if (!str_transform(input, len, transformed, transform_mode::upper))
            return -1;

        if (range_equals(input, len, transformed))
        {
            if (!str_transform(input, len, transformed, transform_mode::title))
                return -1;
        }
        else
        {
            cstring title;
            if (!str_transform(input, len, title, transform_mode::title))
                return -1;
            if (range_equals(input, len, title) &&
                !str_transform(input, len, transformed, transform_mode::lower))
                return -1;
        }
    }
    else if (!str_transform(input, len, transformed, mode))
    {
        return -1;
    }

    ctx.begin_undo_group();

    ctx.set_selection(begin, end);
    ctx.insert_text(transformed.c_str(), transformed.length());

    if (anchor != caret)
    {
        textpos_t new_anchor = begin;
        textpos_t new_caret = begin + uint32_t(transformed.length());
        if (anchor > caret)
        {
            new_anchor = new_caret;
            new_caret = begin;
        }
        ctx.set_selection(new_anchor, new_caret);
    }
    else if (toggle)
    {
        // Restore the caret to allow repeated toggling.
        ctx.set_caret(caret);
        if (caret > 0)
        {
            // Move left and right to snap to a grapheme boundary in case the
            // transformation alter where grapheme boundaries lie.
            ctx.move_left();
            ctx.move_right();
        }
    }

    ctx.end_undo_group();
    return 0;
}

int32_t upper_case(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    return transform_case(ctx, transform_mode::upper, false/*toggle*/);
}

int32_t lower_case(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    return transform_case(ctx, transform_mode::lower, false/*toggle*/);
}

int32_t capitalize(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    return transform_case(ctx, transform_mode::title, false/*toggle*/);
}

int32_t toggle_case(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    return transform_case(ctx, transform_mode::upper, true/*toggle*/);
}

//------------------------------------------------------------------------------

static uint32_t get_scroll_lines(const editor_context& ctx)
{
    uint32_t scroll_lines = 3;

#ifdef _WIN32
    SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &scroll_lines, 0);
    if (scroll_lines == UINT_MAX)
    {
        // Page scrolling.
        scroll_lines = uint32_t(max(ctx.get_inner_extent().y - 1, 1));
        return scroll_lines;
    }
#endif

    // Constrain to one less than the number of visible rows.
    scroll_lines = uint32_t(max(min<int32_t>(ctx.get_inner_extent().y - 1, scroll_lines), 1));
    return scroll_lines;
}

static uint32_t get_scroll_chars(const editor_context& ctx)
{
    uint32_t scroll_chars = 3;

#ifdef _WIN32
    SystemParametersInfoW(SPI_GETWHEELSCROLLCHARS, 0, &scroll_chars, 0);
    if (scroll_chars == UINT_MAX)
    {
        scroll_chars = uint32_t(max(ctx.get_inner_extent().x - 1, 1));
        return scroll_chars;
    }
#endif

    // Constrain to half the number of visible columns.
    scroll_chars = uint32_t(max(min<int32_t>(ctx.get_inner_extent().x / 2, scroll_chars), 1));
    return scroll_chars;
}

int32_t mouse_input(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    if (!params || params->size() < 3)
    {
        ctx.clear_named_value(c_cursor_column_operation_var_name);
        return -1;
    }

    struct mouse_click_state
    {
        std::chrono::steady_clock::time_point time;
        const void* context = nullptr;
        uint32_t button = UINT32_MAX;
        uint32_t x = 0;
        uint32_t y = 0;
    };
    static mouse_click_state s_last_click;

    struct mouse_drag_state
    {
        const void* context = nullptr;
        textpos_t begin = 0;
        textpos_t end = 0;
        bool word = false;
    };
    static mouse_drag_state s_drag;

    const uint32_t button = uint32_t(strtoul((*params)[0].c_str(), nullptr, 10));
    const uint32_t base_button = button & ~uint32_t(4 | 8 | 16);
    const uint32_t x = uint32_t(strtoul((*params)[1].c_str(), nullptr, 10));
    const uint32_t y = uint32_t(strtoul((*params)[2].c_str(), nullptr, 10));

    bool double_click = false;
    if (key == 'M')
    {
        const auto now = std::chrono::steady_clock::now();
#ifdef _WIN32
        const auto double_click_time = std::chrono::milliseconds(GetDoubleClickTime());
#else
        constexpr auto double_click_time = std::chrono::milliseconds(500);
#endif
        double_click = (base_button == 0 && s_last_click.context == &ctx &&
                        s_last_click.button == base_button &&
                        s_last_click.x == x && s_last_click.y == y &&
                        now - s_last_click.time <= double_click_time);
        s_last_click.time = now;
        s_last_click.context = &ctx;
        s_last_click.button = base_button;
        s_last_click.x = x;
        s_last_click.y = y;
    }

    switch (base_button)
    {
    case 64:
    case 65:
        // Mouse WHEEL.
        {
            const int16_t cursor_column = cursor_column_continuation(ctx, name, "wheel");
            const uint32_t scroll_lines = get_scroll_lines(ctx);
            if (scroll_lines)
                ctx.move_caret_vertically((base_button == 64 ? -1 : 1) * int32_t(scroll_lines), cursor_column);
            return 0;
        }

    case 66:
    case 67:
        // Mouse HWHEEL.
        {
            const int16_t cursor_column = cursor_column_continuation(ctx, name, "hwheel");
            const uint32_t scroll_chars = get_scroll_chars(ctx);
            if (scroll_chars)
                ctx.scroll_horizontally((base_button == 66 ? -1 : 1) * int32_t(scroll_chars), cursor_column);
            return 0;
        }

    case 0:
        // Left click.  Clicking within the inner extents sets the caret;
        // clicking outside is ignored (and returns -1 so something else can
        // opt to provide a fallback handler).
        if (key == 'M')
        {
            if (!ctx.set_caret_from_screen(x, y))
            {
                s_drag.context = nullptr;
                ctx.clear_named_value(c_cursor_column_operation_var_name);
                return -1;
            }
            if (double_click)
            {
                ctx.select_word();
                ctx.set_named_value(c_cursor_column_operation_var_name, "double_click");
            }
            else
            {
                ctx.clear_named_value(c_cursor_column_operation_var_name);
            }
            const auto& selection = ctx.get_selection_state();
            s_drag.context = &ctx;
            s_drag.begin = selection.get_sel_begin();
            s_drag.end = selection.get_sel_end();
            s_drag.word = double_click;
            return 0;
        }
        break;

    case 32:
        if (key == 'M' && s_drag.context == &ctx)
        {
            if (!ctx.set_caret_from_screen(x, y, get_scroll_chars(ctx), s_drag.word))
                return -1;

            const textpos_t pos = ctx.get_caret();
            if (!s_drag.word)
            {
                ctx.set_selection(s_drag.begin, pos);
                ctx.suppress_auto_horizontal_scroll();
            }
            else
            {
                ctx.select_word();
                const auto& selection = ctx.get_selection_state();
                if (pos < s_drag.begin)
                    ctx.set_selection(s_drag.end, selection.get_sel_begin());
                else
                    ctx.set_selection(s_drag.begin, selection.get_sel_end());
            }
            return 0;
        }
        break;

    case 2:
        // Right click.  Copies and clears any selection to the clipboard,
        // otherwise it pastes from the clipboard.
        if (key == 'M')
        {
            ctx.clear_named_value(c_cursor_column_operation_var_name);
            if (ctx.get_selection_state().has_selection())
            {
                ctx.copy_to_clipboard();
                ctx.set_selection(ctx.get_caret(), ctx.get_caret());
            }
            else
            {
                ctx.paste_from_clipboard();
            }
            return 0;
        }
        break;
    }

    if (key == 'm' && s_drag.context == &ctx)
        s_drag.context = nullptr;
    ctx.clear_named_value(c_cursor_column_operation_var_name);
    return -1;
}

//------------------------------------------------------------------------------

int32_t redisplay(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    ctx.invalidate();
    ctx.invalidate_border();
    ctx.display();
    return 0;
}

//------------------------------------------------------------------------------

int32_t lorem_ipsum(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
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

//------------------------------------------------------------------------------

int32_t self_insert(editor_context& ctx, int32_t key, const char* name, const binding_params* params) noexcept
{
    if (key < 0)
        return -1;

    if (key <= 0xff)
    {
        ctx.insert_char(char(key));
        return 0;
    }

    // Interpret key as UTF32 and convert it to UTF8.
    char utf8[4];
    size_t length;
    if (key <= 0x7ff)
    {
        utf8[0] = char(0xc0 | (key >> 6));
        utf8[1] = char(0x80 | (key & 0x3f));
        length = 2;
    }
    else if (key <= 0xffff)
    {
        if (key >= 0xd800 && key <= 0xdfff)
            return -1;
        utf8[0] = char(0xe0 | (key >> 12));
        utf8[1] = char(0x80 | ((key >> 6) & 0x3f));
        utf8[2] = char(0x80 | (key & 0x3f));
        length = 3;
    }
    else if (key <= 0x10ffff)
    {
        utf8[0] = char(0xf0 | (key >> 18));
        utf8[1] = char(0x80 | ((key >> 12) & 0x3f));
        utf8[2] = char(0x80 | ((key >> 6) & 0x3f));
        utf8[3] = char(0x80 | (key & 0x3f));
        length = 4;
    }
    else
        return -1;

    // Insert the converted UTF8 characters.
    ctx.begin_undo_group();
    for (size_t i = 0; i < length; ++i)
        ctx.insert_char(utf8[i]);
    ctx.end_undo_group();

    return 0;
}

//------------------------------------------------------------------------------

std::shared_ptr<key_table_list> make_default_key_table()
{
    auto t = std::make_shared<key_table>(true/*can_self_insert*/);

    t->add({ "\001", binding_target_func("select-all") });      // Ctrl-A
    t->add({ "\003", binding_target_func("copy") });            // Ctrl-C
    t->add({ "\010", binding_target_func("del-word-left") });   // VT sends 0x08 for Ctrl-Backspace.
    t->add({ "\r", binding_target_func("accept-line") });       // Ctrl-M / Enter
    t->add({ "\024", binding_target_func("transpose-chars") }); // Ctrl-T
    t->add({ "\026", binding_target_func("paste") });           // Ctrl-V
    t->add({ "\030", binding_target_func("cut") });             // Ctrl-X
    t->add({ "\031", binding_target_func("redo") });            // Ctrl-Y
    t->add({ "\032", binding_target_func("undo") });            // Ctrl-Z

    t->add({ "\033c", binding_target_func("capitalize") });         // Alt-C (c)
    t->add({ "\033l", binding_target_func("lower-case") });         // Alt-L (l)
    t->add({ "\033t", binding_target_func("transpose-words") });    // Alt-T (t)
    t->add({ "\033u", binding_target_func("upper-case") });         // Alt-U (u)

    t->add({ "\033\024", binding_target_func("toggle-case") });     // Alt-Ctrl-T

    t->add({ "\177", binding_target_func("del-char-left") });       // VT sends 0x7F for Backspace.

    t->add({ "\033[H", binding_target_func("begin-of-line") });     // Home
    t->add({ "\033[F", binding_target_func("end-of-line") });       // End
    t->add({ "\033[D", binding_target_func("backward-char") });     // Left
    t->add({ "\033[C", binding_target_func("forward-char") });      // Right
    t->add({ "\033[1;5D", binding_target_func("backward-word") });  // Ctrl-Left
    t->add({ "\033[1;5C", binding_target_func("forward-word") });   // Ctrl-Right
    t->add({ "\033[B", binding_target_func("screen-line-down") });  // Down
    t->add({ "\033[A", binding_target_func("screen-line-up") });    // Up

    t->add({ "\033[1;2H", binding_target_func("cua-begin-of-line") });      // Shift-Home
    t->add({ "\033[1;2F", binding_target_func("cua-end-of-line") });        // Shift-End
    t->add({ "\033[1;2D", binding_target_func("cua-backward-char") });      // Shift-Left
    t->add({ "\033[1;2C", binding_target_func("cua-forward-char") });       // Shift-Right
    t->add({ "\033[1;6D", binding_target_func("cua-backward-word") });      // Shift-Ctrl-Left
    t->add({ "\033[1;6C", binding_target_func("cua-forward-word") });       // Shift-Ctrl-Right
    t->add({ "\033[1;2B", binding_target_func("cua-screen-line-down") });   // Shift-Down
    t->add({ "\033[1;2A", binding_target_func("cua-screen-line-up") });     // Shift-Up

    t->add({ "\033[3~", binding_target_func("del-char-right") });       // Del
    t->add({ "\033[3;5~", binding_target_func("del-word-right") });     // Ctrl-Del

    t->add({ "\033[<%#;%#;%#M", binding_target_func("mouse-input"), true/*pattern*/ }); // Mouse press
    t->add({ "\033[<%#;%#;%#m", binding_target_func("mouse-input"), true/*pattern*/ }); // Mouse release

    auto tables = std::make_shared<key_table_list>();
    tables->emplace_back(std::move(t));
    return tables;
}

//------------------------------------------------------------------------------

static const editor_command c_commands[] =
{
    { "accept-line", accept_line },
    { "backward-char", backward_char },
    { "backward-word", backward_word },
    { "begin-of-line", begin_of_line },
    { "capitalize", capitalize },
    { "clear-selection", clear_selection },
    { "copy", copy },
    { "cua-backward-char", cua_backward_char },
    { "cua-backward-word", cua_backward_word },
    { "cua-begin-of-line", cua_begin_of_line },
    { "cua-end-of-line", cua_end_of_line },
    { "cua-forward-char", cua_forward_char },
    { "cua-forward-word", cua_forward_word },
    { "cua-screen-line-down", cua_screen_line_down },
    { "cua-screen-line-up", cua_screen_line_up },
    { "cut", cut },
    { "del-char-left", del_char_left },
    { "del-char-right", del_char_right },
    { "del-word-left", del_word_left },
    { "del-word-right", del_word_right },
    { "end-of-line", end_of_line },
    { "forward-char", forward_char },
    { "forward-word", forward_word },
    { "lorem-ipsum", lorem_ipsum },
    { "lower-case", lower_case },
    { "mouse-input", mouse_input },
    { "paste", paste },
    { "quoted-insert", quoted_insert },
    { "redisplay", redisplay },
    { "redo", redo },
    { "screen-line-down", screen_line_down },
    { "screen-line-up", screen_line_up },
    { "select-all", select_all },
    { "select-word", select_word },
    { "toggle-case", toggle_case },
    { "transpose-chars", transpose_chars },
    { "transpose-words", transpose_words },
    { "undo", undo },
    { "undo-all", undo_all },
    { "upper-case", upper_case },
};

void editor_context::ensure_commands()
{
    if (!s_commands.empty())
        return;

    for (const auto& command : c_commands)
        s_commands.emplace_back(command);

    s_unsorted_commands = true;
}

void editor_context::register_command(const char* name, editor_command_func_t func)
{
    assert(name);
    if (!name)
        return;

    const auto found = std::lower_bound(s_commands.begin(), s_commands.end(), name, [](const editor_command& candidate, const char* name) {
        const int comparison = strcmp(candidate.name, name);
        return comparison < 0;
    });

    if (found != s_commands.end() && strcmp(found->name, name) == 0)
    {
        if (func)
            found->func = func;
        else
            s_commands.erase(found);
    }
    else
    {
        s_command_names.emplace_back(name);

        editor_command command;
        command.name = s_command_names.back().c_str();
        command.func = func;

        s_commands.insert(found, std::move(command));
        s_unsorted_commands = true;
    }
}

const std::vector<editor_command>& editor_context::get_registered_commands()
{
    ensure_commands_sorted();
    return s_commands;
}

void editor_context::ensure_commands_sorted()
{
    if (!s_unsorted_commands)
        return;

    std::sort(s_commands.begin(), s_commands.end(), [](const editor_command& a, const editor_command& b) {
        const int comparison = strcmp(a.name, b.name);
        return comparison < 0;
    });

    s_unsorted_commands = false;
}

editor_command_func_t editor_context::lookup_command(const char* name)
{
    if (!name)
        return nullptr;

    ensure_commands_sorted();

    const auto found = std::lower_bound(s_commands.begin(), s_commands.end(), name, [](const editor_command& candidate, const char* name) {
        const int comparison = strcmp(candidate.name, name);
        return comparison < 0;
    });

    if (found == s_commands.end() || strcmp(found->name, name) != 0)
        return nullptr;

    return found->func;
}

std::vector<editor_command> editor_context::s_commands;
std::vector<cstring> editor_context::s_command_names;
bool editor_context::s_unsorted_commands = false;

//------------------------------------------------------------------------------

}
