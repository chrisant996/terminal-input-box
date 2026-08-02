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

tib::key_table_list make_basic_key_table()
{
    auto t = std::make_shared<tib::key_table>();

    t->add({ "\b", tib::binding_target(backspace) });
    t->add({ "\r", tib::binding_target(accept_line) });

    tib::key_table_list tables;
    tables.emplace_back(std::move(t));
    return tables;
}

int main(int argc, const char** argv)
{
    --argc, ++argv;

    tib_host::set_crt_locale_utf8();
    tib_host::set_console_vt_input();

    tib::input_box tib;
    tib::dispatcher dispatcher;
    tib::editor_context ctx; // TODO:  Eventually this belongs inside tib::input_box.

    dispatcher.init(make_basic_key_table());

    while (!s_done)
    {
        char c = tib::term_in();

        switch (dispatcher.step(c))
        {
        case tib::dispatch_outcome::miss:
            // TODO:  Insert the text into the input_box.
            break;
        case tib::dispatch_outcome::more:
            break;
        case tib::dispatch_outcome::match:
            {
                const auto target = dispatcher.get_target();
                assert(target);

                // TODO:  Eventually tib::input_box needs a method to execute
                // a binding_target, which will keep tib::editor_context
                // encapsulated inside tib::input_box.
                switch (target->get_type())
                {
                case tib::binding_type::func:
                    {
                        const auto func = target->get_func();
                        assert(func);
                        func(ctx, uint8_t(c), target->get_text());
                    }
                    break;
                default:
                    assert(false);
                    break;
                }
            }
            break;
        }
    }

    return 0;
}
