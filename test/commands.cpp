// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "maybe_windows.h"
#include "test.h"
#include "tib.h"

static void initialize_context(tib::editor_context& context, const char* text,
                               tib::textpos_t caret)
{
    context.initialize(text);
    context.set_caret(caret);
}

TEST_CASE("Select word command")
{
    SECTION("Selects the word containing the caret")
    {
        tib::editor_context context;
        initialize_context(context, "alpha beta", 2);
        const auto command = tib::editor_context::lookup_command("select-word");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "select-word", nullptr) == 0);
        const auto& selection = context.get_selection_state();
        REQUIRE(selection.get_sel_begin() == 0);
        REQUIRE(selection.get_sel_end() == 5);
    }

    SECTION("Selects a Unicode word without splitting graphemes")
    {
        static const char text[] = "x \xc2\xa2" "e\xcc\x81" "y z";
        tib::editor_context context;
        initialize_context(context, text, 5);
        const auto command = tib::editor_context::lookup_command("select-word");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "select-word", nullptr) == 0);
        const auto& selection = context.get_selection_state();
        REQUIRE(selection.get_sel_begin() == 2);
        REQUIRE(selection.get_sel_end() == 8);
    }

    SECTION("Selects the preceding word at end of input")
    {
        tib::editor_context context;
        initialize_context(context, "alpha beta", 10);
        const auto command = tib::editor_context::lookup_command("select-word");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "select-word", nullptr) == 0);
        const auto& selection = context.get_selection_state();
        REQUIRE(selection.get_sel_begin() == 6);
        REQUIRE(selection.get_sel_end() == 10);
    }
}
