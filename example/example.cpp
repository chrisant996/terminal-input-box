// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include <stdio.h>

#include "maybe_windows.h"
#include "tib.h"
#include "tib_host.h"
#include <assert.h>

static tib_host::auto_terminal_init s_auto_terminal_init;

static bool s_done = false;

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

static int32_t eat_escape(tib::editor_context& ctx, int32_t key, const char* name)
{
    return 0;
}

std::shared_ptr<tib::key_table_list> make_basic_key_table()
{
    auto t = std::make_shared<tib::key_table>();

    t->add({ "\b", tib::binding_target(backspace) });
    t->add({ "\x7f", tib::binding_target(del_word_left) });
    t->add({ "\r", tib::binding_target(accept_line) });
    // TODO:  Whoa, the \xe0 and \xe0O do not seem like VT sequences...
    t->add({ "\xe0G", tib::binding_target(begin_of_line) });
    t->add({ "\xe0O", tib::binding_target(end_of_line) });
    t->add({ "\xe0K", tib::binding_target(backward_char) });
    t->add({ "\xe0M", tib::binding_target(forward_char) });
    t->add({ "\xe0s", tib::binding_target(backward_word) });
    t->add({ "\xe0t", tib::binding_target(forward_word) });
    t->add({ "\xe0S", tib::binding_target(del_char_right) });
    t->add({ "\xe0\x93", tib::binding_target(del_word_right) });
    t->add({ "\031", tib::binding_target(redo) });
    t->add({ "\032", tib::binding_target(undo) });
    t->add({ "\033", tib::binding_target(eat_escape) });

    auto tables = std::make_shared<tib::key_table_list>();
    tables->emplace_back(std::move(t));
    return tables;
}

int main(int argc, const char** argv)
{
    --argc, ++argv;

    tib_host::set_crt_locale_utf8();
    tib_host::set_console_vt_input();

    tib::input_box tib;
    tib.set_bindings(make_basic_key_table());

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    tib.set_origin(csbi.dwCursorPosition.X + 1);
#else
    // TODO:  Query the terminal for the current position.
#endif

    // TODO:  input_box should treat 0 max width as the current terminal width.
    tib.set_max_width(32);
    tib.initialize_text("hello world");

    tib::dispatcher dispatcher;
    dispatcher.init(tib.get_bindings());

    tib::cstring tmp;
    tmp.set("\n\033[A");
    tib::term_out(tmp.c_str(), tmp.length());

    tib::cstring sequence;

    while (!s_done)
    {
        tib.display();

        const int32_t c = tib::term_in();

        if (c > 0x20 && c < 0x7f)
            sequence.printf("%c ", char(c));
        else
            sequence.printf("0x%02.2x ", c);
        tmp.set("\033[s\n\033[90m");
        tmp.append(sequence.c_str(), sequence.length());
        tmp.append("\033[K\033[u");
        tib::term_out(tmp.c_str(), tmp.length());

        switch (dispatcher.step(c))
        {
        case tib::dispatch_outcome::miss:
            if (!(c & 0xffffff00))
            {
                // Insert the char into the input_box.
                tib.insert_char(char(c));
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

    tib::term_out("\r\n", 2);

    return 0;
}
