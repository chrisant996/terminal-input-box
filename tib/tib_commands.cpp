// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_bindings.h"
#include "tib_context.h"
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

int32_t backspace(editor_context& ctx, int32_t key, const char* name)
{
    ctx.backspace();
    return 0;
}

int32_t del_word_left(editor_context& ctx, int32_t key, const char* name)
{
    ctx.backspace(true/*word*/);
    return 0;
}

int32_t del_char_right(editor_context& ctx, int32_t key, const char* name)
{
    ctx.del();
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

int32_t select_all(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.set_selection(0, uint16_t(ctx.get_text().length()));
    return 0;
}

int32_t cua_begin_of_line(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.begin_of_input(true/*select*/);
    return 0;
}

int32_t cua_end_of_line(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.end_of_input(true/*select*/);
    return 0;
}

int32_t cua_backward_char(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_left(false/*word*/, true/*select*/);
    return 0;
}

int32_t cua_forward_char(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_right(false/*word*/, true/*select*/);
    return 0;
}

int32_t cua_backward_word(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_left(true/*word*/, true/*select*/);
    return 0;
}

int32_t cua_forward_word(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.move_right(true/*word*/, true/*select*/);
    return 0;
}

//------------------------------------------------------------------------------

int32_t cut(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.cut_to_clipboard();
    return 0;
}

int32_t copy(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.copy_to_clipboard();
    return 0;
}

int32_t paste(tib::editor_context& ctx, int32_t key, const char* name)
{
    ctx.paste_from_clipboard();
    return 0;
}

//------------------------------------------------------------------------------

int32_t lorem_ipsum(tib::editor_context& ctx, int32_t key, const char* name)
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

std::shared_ptr<tib::key_table_list> make_basic_key_table()
{
    auto t = std::make_shared<tib::key_table>(true/*can_self_insert*/);

    t->add({ "\001", tib::binding_target(select_all) });
    t->add({ "\003", tib::binding_target(copy) });
    t->add({ "\010", tib::binding_target(del_word_left) }); // VT sends 0x08 for Ctrl-Backspace.
    t->add({ "\r", tib::binding_target(accept_line) });
    t->add({ "\026", tib::binding_target(paste) });
    t->add({ "\030", tib::binding_target(cut) });
    t->add({ "\031", tib::binding_target(redo) });
    t->add({ "\032", tib::binding_target(undo) });

    t->add({ "\177", tib::binding_target(backspace) });     // VT sends 0x7F for Backspace.

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

//------------------------------------------------------------------------------

}
