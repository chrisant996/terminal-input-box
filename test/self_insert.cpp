// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "test.h"
#include "tib.h"

class self_insert_tester : public tib::editor_context
{
    typedef tib::editor_context base;

public:
    int32_t             dispatch(const tib::cstring& sequence, int32_t key, const tib::binding_target* binding) noexcept override;
    uint32_t            get_dispatch_count() const noexcept { return m_dispatch_count; }

private:
    uint32_t            m_dispatch_count = 0;
};

int32_t self_insert_tester::dispatch(const tib::cstring& sequence, int32_t key, const tib::binding_target* binding) noexcept
{
    ++m_dispatch_count;
    return base::dispatch(sequence, key, binding);
}

static void dispatch_macro(const char* text, std::shared_ptr<self_insert_tester>& input, tib::binding_resolver& resolver)
{
    tib::cstring macro(text);
    macro.append("\r");
    REQUIRE(tib::term_push_macro_text(macro.c_str(), macro.length()));

    for (;;)
    {
        const int32_t c = tib::term_in();
        if (c == '\r')
            break;

        auto resolved = resolver.step(uint8_t(c));
        REQUIRE(resolved.outcome == tib::dispatch_outcome::self_insert);
        REQUIRE(resolved.dispatch());
    }
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
