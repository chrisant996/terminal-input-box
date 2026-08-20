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

namespace tib {

//------------------------------------------------------------------------------

int32_t accept_line(editor_context& ctx, int32_t key, const char* name)
{
    ctx.set_done();
    return 0;
}

//------------------------------------------------------------------------------

int32_t begin_of_line(editor_context& ctx, int32_t key, const char* name)
{
    ctx.begin_of_input();
    return 0;
}

int32_t end_of_line(editor_context& ctx, int32_t key, const char* name)
{
    ctx.end_of_input();
    return 0;
}

int32_t backward_char(editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_left();
    return 0;
}

int32_t forward_char(editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_right();
    return 0;
}

int32_t backward_word(editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_left(true/*word*/);
    return 0;
}

int32_t forward_word(editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_right(true/*word*/);
    return 0;
}

//------------------------------------------------------------------------------

int32_t del_char_left(editor_context& ctx, int32_t key, const char* name)
{
    ctx.backspace();
    return 0;
}

int32_t del_char_right(editor_context& ctx, int32_t key, const char* name)
{
    ctx.del();
    return 0;
}

int32_t del_word_left(editor_context& ctx, int32_t key, const char* name)
{
    ctx.backspace(true/*word*/);
    return 0;
}

int32_t del_word_right(editor_context& ctx, int32_t key, const char* name)
{
    ctx.del(true/*word*/);
    return 0;
}

//------------------------------------------------------------------------------

int32_t redo(editor_context& ctx, int32_t key, const char* name)
{
    ctx.redo();
    return 0;
}

int32_t undo(editor_context& ctx, int32_t key, const char* name)
{
    ctx.undo();
    return 0;
}

//------------------------------------------------------------------------------

int32_t select_all(editor_context& ctx, int32_t key, const char* name)
{
    ctx.set_selection(0, uint16_t(ctx.get_text().length()));
    return 0;
}

int32_t cua_begin_of_line(editor_context& ctx, int32_t key, const char* name)
{
    ctx.begin_of_input(true/*select*/);
    return 0;
}

int32_t cua_end_of_line(editor_context& ctx, int32_t key, const char* name)
{
    ctx.end_of_input(true/*select*/);
    return 0;
}

int32_t cua_backward_char(editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_left(false/*word*/, true/*select*/);
    return 0;
}

int32_t cua_forward_char(editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_right(false/*word*/, true/*select*/);
    return 0;
}

int32_t cua_backward_word(editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_left(true/*word*/, true/*select*/);
    return 0;
}

int32_t cua_forward_word(editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_right(true/*word*/, true/*select*/);
    return 0;
}

//------------------------------------------------------------------------------

int32_t cut(editor_context& ctx, int32_t key, const char* name)
{
    ctx.cut_to_clipboard();
    return 0;
}

int32_t copy(editor_context& ctx, int32_t key, const char* name)
{
    ctx.copy_to_clipboard();
    return 0;
}

int32_t paste(editor_context& ctx, int32_t key, const char* name)
{
    ctx.paste_from_clipboard();
    return 0;
}

//------------------------------------------------------------------------------

int32_t lorem_ipsum(editor_context& ctx, int32_t key, const char* name)
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

int32_t self_insert(editor_context& ctx, int32_t key, const char* name) noexcept
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

    t->add({ "\001", binding_target_func("select-all") });
    t->add({ "\003", binding_target_func("copy") });
    t->add({ "\010", binding_target_func("del-word-left") }); // VT sends 0x08 for Ctrl-Backspace.
    t->add({ "\r", binding_target_func("accept-line") });
    t->add({ "\026", binding_target_func("paste") });
    t->add({ "\030", binding_target_func("cut") });
    t->add({ "\031", binding_target_func("redo") });
    t->add({ "\032", binding_target_func("undo") });

    t->add({ "\177", binding_target_func("backspace") });     // VT sends 0x7F for Backspace.

    t->add({ "\033[H", binding_target_func("begin-of-line") });
    t->add({ "\033[F", binding_target_func("end-of-line") });
    t->add({ "\033[D", binding_target_func("backward-char") });
    t->add({ "\033[C", binding_target_func("forward-char") });
    t->add({ "\033[1;5D", binding_target_func("backward-word") });
    t->add({ "\033[1;5C", binding_target_func("forward-word") });

    t->add({ "\033[1;2H", binding_target_func("cua-begin-of-line") });
    t->add({ "\033[1;2F", binding_target_func("cua-end-of-line") });
    t->add({ "\033[1;2D", binding_target_func("cua-backward-char") });
    t->add({ "\033[1;2C", binding_target_func("cua-forward-char") });
    t->add({ "\033[1;6D", binding_target_func("cua-backward-word") });
    t->add({ "\033[1;6C", binding_target_func("cua-forward-word") });

    t->add({ "\033[3~", binding_target_func("del-char-right") });
    t->add({ "\033[3;5~", binding_target_func("del-word-right") });

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
    { "copy", copy },
    { "cua-backward-char", cua_backward_char },
    { "cua-backward-word", cua_backward_word },
    { "cua-begin-of-line", cua_begin_of_line },
    { "cua-end-of-line", cua_end_of_line },
    { "cua-forward-char", cua_forward_char },
    { "cua-forward-word", cua_forward_word },
    { "cut", cut },
    { "del-char-left", del_char_left },
    { "del-char-right", del_char_right },
    { "del-word-left", del_word_left },
    { "del-word-right", del_word_right },
    { "end-of-line", end_of_line },
    { "forward-char", forward_char },
    { "forward-word", forward_word },
    { "lorem-ipsum", lorem_ipsum },
    { "paste", paste },
    { "redo", redo },
    { "select-all", select_all },
    { "undo", undo },
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
