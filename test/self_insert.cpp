// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "test.h"
#include "test_util.h"
#include "tib.h"

class self_insert_tester : public tib::editor_context
{
    typedef tib::editor_context base;

public:
    int32_t             dispatch(const tib::cstring& sequence, int32_t key, const tib::binding_target* binding, const tib::binding_params* params) noexcept override;
    uint32_t            get_dispatch_count() const noexcept { return m_dispatch_count; }

private:
    uint32_t            m_dispatch_count = 0;
};

int32_t self_insert_tester::dispatch(const tib::cstring& sequence, int32_t key, const tib::binding_target* binding, const tib::binding_params* params) noexcept
{
    ++m_dispatch_count;
    return base::dispatch(sequence, key, binding, params);
}

static void dispatch_macro(const char* text, std::shared_ptr<self_insert_tester>& input, tib::binding_resolver& resolver)
{
    tib::cstring macro(text);
    macro.append("\r");
    REQUIRE(tib::term_push_macro_text(macro.c_str(), macro.length()));

    for (;;)
    {
        const int32_t c = tib::term_in();
        REQUIRE(c >= 0);
        if (c == '\r')
            break;

        auto resolved = resolver.step(uint8_t(c));
        REQUIRE(resolved.outcome == tib::dispatch_outcome::self_insert);
        REQUIRE(resolved.dispatch());
    }
}

static std::shared_ptr<tib::key_table_list> make_quoted_insert_key_table(bool numeric_argument=false)
{
    auto tables = tib::make_default_key_table(numeric_argument);
    REQUIRE(!tables->empty());
    REQUIRE(tables->back()->add("\021", tib::binding_target_func("quoted-insert")));
    return tables;
}

static void invoke_quoted_insert(tib::binding_resolver& resolver)
{
    auto resolved = resolver.step('\021');
    REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
    REQUIRE(resolved.binding_target->is_func_name("quoted-insert"));
    REQUIRE(resolved.dispatch());
}

TEST_CASE("Quoted insert")
{
    auto input = std::make_shared<self_insert_tester>();
    input->initialize();
    input->set_bindings(make_quoted_insert_key_table());

    tib::binding_resolver resolver;
    resolver.add_target(input);

    SECTION("Inserts any valid input byte literally")
    {
        const uint8_t bytes[] = { 0x01, '\r', 0x1b, 0x7f, 0x80, 0xf9 };
        for (const uint8_t c : bytes)
        {
            auto byte_input = std::make_shared<self_insert_tester>();
            byte_input->initialize();
            byte_input->set_bindings(make_quoted_insert_key_table());

            tib::binding_resolver byte_resolver;
            byte_resolver.add_target(byte_input);
            invoke_quoted_insert(byte_resolver);

            auto resolved = byte_resolver.step(c);
            REQUIRE(resolved.outcome == tib::dispatch_outcome::quoted_insert);
            REQUIRE(resolved.sequence.length() == 1);
            REQUIRE(uint8_t(resolved.sequence.c_str()[0]) == c);
            REQUIRE(resolved.dispatch());

            REQUIRE(byte_input->get_text().length() == 1);
            REQUIRE(uint8_t(byte_input->get_text().c_str()[0]) == c);
            REQUIRE(!strcmp(byte_input->get_last_command(), "self-insert"));
        }
    }

    SECTION("Applies to exactly one byte")
    {
        invoke_quoted_insert(resolver);

        auto resolved = resolver.step('\r');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::quoted_insert);
        REQUIRE(resolved.dispatch());
        REQUIRE(!input->done());
        REQUIRE(input->get_text() == tib::cstring("\r", 1));

        resolved = resolver.step('\r');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
        REQUIRE(resolved.binding_target->is_func_name("accept-line"));
        REQUIRE(resolved.dispatch());
        REQUIRE(input->done());
    }

    SECTION("Positive numeric argument repeats the next byte")
    {
        input->set_numeric_argument(5);
        invoke_quoted_insert(resolver);
        REQUIRE(!input->has_numeric_argument());

        auto resolved = resolver.step('x');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::quoted_insert);
        REQUIRE(resolved.dispatch());
        REQUIRE(input->get_text() == "xxxxx");

        resolved = resolver.step('\r');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
        REQUIRE(resolved.binding_target->is_func_name("accept-line"));
    }

    SECTION("Negative numeric argument quotes the next N bytes")
    {
        input->set_numeric_argument(-3);
        invoke_quoted_insert(resolver);
        REQUIRE(!input->has_numeric_argument());

        const uint8_t bytes[] = { '\r', 0x1b, 'x' };
        for (const uint8_t c : bytes)
        {
            auto resolved = resolver.step(c);
            REQUIRE(resolved.outcome == tib::dispatch_outcome::quoted_insert);
            REQUIRE(resolved.dispatch());
        }
        REQUIRE(input->get_text() == tib::cstring("\r\x1bx", 3));

        auto resolved = resolver.step('\r');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
        REQUIRE(resolved.binding_target->is_func_name("accept-line"));
    }

    SECTION("Proper negative digit argument quotes exactly N bytes")
    {
        tib::editor_quirks quirks;
        quirks.bash_digit_argument = false;
        input->set_quirks(quirks);
        input->set_bindings(make_quoted_insert_key_table(true/*numeric_argument*/));

        auto resolved = resolver.step('\x1b');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::more);
        resolved = resolver.step('-');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
        REQUIRE(resolved.binding_target->is_func_name("digit-argument"));
        REQUIRE(resolved.dispatch());
        REQUIRE(input->get_numeric_argument() == -1);

        resolved = resolver.step('\x1b');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::more);
        resolved = resolver.step('2');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
        REQUIRE(resolved.binding_target->is_func_name("digit-argument"));
        REQUIRE(resolved.dispatch());
        REQUIRE(input->get_numeric_argument() == -2);

        invoke_quoted_insert(resolver);
        for (int32_t n = 0; n < 2; ++n)
        {
            resolved = resolver.step('\x7f');
            REQUIRE(resolved.outcome == tib::dispatch_outcome::quoted_insert);
            REQUIRE(resolved.dispatch());
        }
        REQUIRE(input->get_text() == tib::cstring("\x7f\x7f", 2));

        resolved = resolver.step('\x7f');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
        REQUIRE(resolved.binding_target->is_func_name("del-char-left"));
        REQUIRE(resolved.dispatch());
        REQUIRE(input->get_text() == tib::cstring("\x7f", 1));
    }

    SECTION("Bash digit argument includes the implicit one")
    {
        tib::editor_quirks quirks;
        quirks.bash_digit_argument = true;
        input->set_quirks(quirks);
        input->set_bindings(make_quoted_insert_key_table(true/*numeric_argument*/));

        auto resolved = resolver.step('\x1b');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::more);
        resolved = resolver.step('-');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
        REQUIRE(resolved.dispatch());

        resolved = resolver.step('\x1b');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::more);
        resolved = resolver.step('2');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
        REQUIRE(resolved.dispatch());
        REQUIRE(input->get_numeric_argument() == -12);
    }

    SECTION("Discards a pending binding prefix")
    {
        auto resolved = resolver.step('\x1b');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::more);

        invoke_quoted_insert(resolver);
        resolved = resolver.step('x');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::quoted_insert);
        REQUIRE(resolved.dispatch());
        REQUIRE(input->get_text() == "x");
    }

    SECTION("Clearing targets cancels the pending mode")
    {
        invoke_quoted_insert(resolver);
        resolver.clear_targets();
        resolver.add_target(input);

        auto resolved = resolver.step('\r');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
        REQUIRE(resolved.binding_target->is_func_name("accept-line"));
    }

    SECTION("Returns quoted input to the initiating target")
    {
        auto other = std::make_shared<self_insert_tester>();
        other->initialize();

        auto other_table = std::make_shared<tib::key_table>(false);
        REQUIRE(other_table->add("x", tib::binding_target_func("accept-line")));
        auto other_tables = std::make_shared<tib::key_table_list>();
        other_tables->emplace_back(std::move(other_table));
        other->set_bindings(std::move(other_tables));
        resolver.add_target(other);

        invoke_quoted_insert(resolver);
        auto resolved = resolver.step('x');
        REQUIRE(resolved.outcome == tib::dispatch_outcome::quoted_insert);
        REQUIRE(resolved.dispatcher_target.lock() == input);
        REQUIRE(input->get_text().empty());
        REQUIRE(other->get_text().empty());
        REQUIRE(!other->done());
        REQUIRE(resolved.dispatch());
        REQUIRE(input->get_text() == "x");
        REQUIRE(other->get_text().empty());
        REQUIRE(!other->done());
    }
}

TEST_CASE("Quoted insert does not read ahead")
{
    REQUIRE(!tib::g_optimize_self_insert);
    const bool optimize_self_insert = tib::g_optimize_self_insert;
    MAKE_CLEANUP([optimize_self_insert]() { tib::g_optimize_self_insert = optimize_self_insert; });
    tib::g_optimize_self_insert = true;

    test_input_stream stream("ab");

    auto input = std::make_shared<self_insert_tester>();
    input->initialize();
    input->set_bindings(make_quoted_insert_key_table());

    tib::binding_resolver resolver;
    resolver.add_target(input);
    invoke_quoted_insert(resolver);

    auto resolved = resolver.step(uint8_t(tib::term_in()));
    REQUIRE(resolved.outcome == tib::dispatch_outcome::quoted_insert);
    REQUIRE(resolved.dispatch());
    REQUIRE(input->get_dispatch_count() == 2);
    REQUIRE(!stream.empty());
    REQUIRE(input->get_text() == "a");

    resolved = resolver.step(uint8_t(tib::term_in()));
    REQUIRE(resolved.outcome == tib::dispatch_outcome::self_insert);
    REQUIRE(resolved.dispatch());
    REQUIRE(input->get_dispatch_count() == 3);
    REQUIRE(stream.empty());
    REQUIRE(input->get_text() == "ab");

    input->undo();
    REQUIRE(input->get_text() == "a");
    input->undo();
    REQUIRE(input->get_text().empty());
}

TEST_CASE("Self insert optimization")
{
    REQUIRE(!tib::g_optimize_self_insert);
    const bool optimize_self_insert = tib::g_optimize_self_insert;
    MAKE_CLEANUP([optimize_self_insert]() { tib::g_optimize_self_insert = optimize_self_insert; });

    auto input = std::make_shared<self_insert_tester>();
    input->initialize();
    input->set_bindings(tib::make_default_key_table());

    tib::binding_resolver resolver;
    resolver.add_target(input);

    const char utf8[] = "\xf0\x9f\x9a\xa8\xf0\x9f\x98\x8eHello";

    SECTION("Disabled")
    {
        dispatch_macro(utf8, input, resolver);
        REQUIRE(input->get_dispatch_count() == sizeof(utf8) - 1);
        REQUIRE(input->get_text() == tib::cstring(utf8));
    }

    SECTION("Enabled")
    {
        tib::g_optimize_self_insert = true;
        dispatch_macro(utf8, input, resolver);
        REQUIRE(input->get_dispatch_count() == 1);
        REQUIRE(input->get_text() == tib::cstring(utf8));
    }
}

TEST_CASE("Optimized self insert undo grouping")
{
    REQUIRE(!tib::g_optimize_self_insert);
    const bool optimize_self_insert = tib::g_optimize_self_insert;
    MAKE_CLEANUP([optimize_self_insert]() { tib::g_optimize_self_insert = optimize_self_insert; });
    tib::g_optimize_self_insert = true;

    auto input = std::make_shared<self_insert_tester>();
    input->initialize();
    input->set_bindings(tib::make_default_key_table());

    tib::binding_resolver resolver;
    resolver.add_target(input);

    const char utf8[] = "\xf0\x9f\x9a\xa8\xf0\x9f\x98\x8eHello";
    dispatch_macro(utf8, input, resolver);
    REQUIRE(input->get_dispatch_count() == 1);
    REQUIRE(input->get_text() == tib::cstring(utf8));

    input->undo();
    REQUIRE(input->get_text().empty());

    input->redo();
    REQUIRE(input->get_text() == tib::cstring(utf8));
}
