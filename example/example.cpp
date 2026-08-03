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
    puts("backspace");
    return 0;
}

static int32_t accept_line(tib::editor_context& ctx, int32_t key, const char* name)
{
    s_done = true;
    puts("accept_line");
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
    t->add({ "\r", tib::binding_target(accept_line) });
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

    tib::dispatcher dispatcher;
    dispatcher.init(tib.get_bindings());

    while (!s_done)
    {
        const int32_t c = tib::term_in();

        switch (dispatcher.step(c))
        {
        case tib::dispatch_outcome::miss:
            if (!(c & 0xffffff00))
            {
                // Insert the char into the input_box.
                tib.insert_char(char(c));
            }
            break;
        case tib::dispatch_outcome::more:
            break;
        case tib::dispatch_outcome::match:
            {
                const auto target = dispatcher.get_target();
                assert(target);
                tib.do_binding_target(target, c);
            }
            break;
        }
    }

    return 0;
}
