// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "test.h"
#include "test_util.h"
#include "tib.h"

TEST_CASE("Terminal output interface")
{
    tib::cstring output;
    test_output_stream stream(output);

    tib::term_out("abc");
    tib::term_out("defghi", 3);
    tib::ding();

    REQUIRE(output == "abcdef");
    REQUIRE(stream.get_ding_count() == 1);
}

namespace {
tib::cstring s_shutdown_events;

class shutdown_test_input final : public tib::terminal_in
{
public:
    ~shutdown_test_input() override { s_shutdown_events.append("I"); }
    bool enable_mouse_input(tib::mouse_input_mode mode, bool) noexcept override
    {
        if (mode == tib::mouse_input_mode::none)
            s_shutdown_events.append("M");
        return true;
    }
    int32_t read() noexcept override { return -1; }
    bool avail(uint32_t) noexcept override { return false; }
};

class shutdown_test_output final : public tib::terminal_out
{
public:
    ~shutdown_test_output() override { s_shutdown_events.append("O"); }
    void write(const char* s, size_t len) noexcept override { s_shutdown_events.append(s, len); }
    void ding() noexcept override {}
};

tib::terminal_in* new_shutdown_test_input(tib::pushed_input&) { return new shutdown_test_input; }
tib::terminal_out* new_shutdown_test_output() { return new shutdown_test_output; }
}

TEST_CASE("Terminal shutdown runs before interface destruction and only on the final end")
{
    const auto input_hook = tib::hook_new_terminal_in;
    const auto output_hook = tib::hook_new_terminal_out;
    tib::term_end();
    tib::hook_new_terminal_in = new_shutdown_test_input;
    tib::hook_new_terminal_out = new_shutdown_test_output;
    s_shutdown_events.clear();
    tib::term_begin();
    const bool mouse_enabled = tib::enable_mouse_input(tib::mouse_input_mode::DRAG);
    tib::term_begin();
    tib::term_end();
    const bool deferred = s_shutdown_events.empty();
    tib::term_end();
    const bool ordered = s_shutdown_events == "\x1b[?25h\x1b[mMIO";
    tib::term_out("unexpected");
    const bool detached = s_shutdown_events == "\x1b[?25h\x1b[mMIO";
    tib::hook_new_terminal_in = input_hook;
    tib::hook_new_terminal_out = output_hook;
    tib::term_begin();
    REQUIRE(mouse_enabled);
    REQUIRE(deferred);
    REQUIRE(ordered);
    REQUIRE(detached);
}
