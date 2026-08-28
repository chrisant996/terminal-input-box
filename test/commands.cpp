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

TEST_CASE("Transpose chars command")
{
    SECTION("Swaps the grapheme at the caret with the preceding grapheme")
    {
        tib::editor_context context;
        initialize_context(context, "abcd", 2);
        const auto command = tib::editor_context::lookup_command("transpose-chars");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-chars", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("acbd"));
        REQUIRE(context.get_caret() == 3);
    }

    SECTION("Swaps two graphemes before the caret at end of input")
    {
        tib::editor_context context;
        initialize_context(context, "abcd", 4);
        const auto command = tib::editor_context::lookup_command("transpose-chars");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-chars", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("abdc"));
        REQUIRE(context.get_caret() == 4);
    }

    SECTION("Treats a combining sequence as one grapheme")
    {
        static const char text[] = "a" "e\xcc\x81" "b";
        tib::editor_context context;
        initialize_context(context, text, 4);
        const auto command = tib::editor_context::lookup_command("transpose-chars");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-chars", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("ab" "e\xcc\x81"));
        REQUIRE(context.get_caret() == 5);
    }

    SECTION("Does nothing without two graphemes")
    {
        tib::editor_context context;
        initialize_context(context, "a", 1);
        const auto command = tib::editor_context::lookup_command("transpose-chars");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-chars", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("a"));
        REQUIRE(context.get_caret() == 1);
    }
}

TEST_CASE("Transpose words command")
{
    SECTION("Swaps the previous word with a word beginning at the caret")
    {
        tib::editor_context context;
        initialize_context(context, "one two three", 4);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("two one three"));
        REQUIRE(context.get_caret() == 7);
    }

    SECTION("Swaps the words on either side of the caret")
    {
        tib::editor_context context;
        initialize_context(context, "one,  two three", 4);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("two,  one three"));
        REQUIRE(context.get_caret() == 9);
    }

    SECTION("Swaps the surrounding words at the end of a word")
    {
        tib::editor_context context;
        initialize_context(context, "one two three", 3);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("two one three"));
        REQUIRE(context.get_caret() == 7);
    }

    SECTION("Swaps the preceding word with the word containing the caret")
    {
        tib::editor_context context;
        initialize_context(context, "one,  two three", 7);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("two,  one three"));
        REQUIRE(context.get_caret() == 9);
    }

    SECTION("Does nothing at the beginning of the first word")
    {
        tib::editor_context context;
        initialize_context(context, "one,  two three", 0);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("one,  two three"));
        REQUIRE(context.get_caret() == 0);
    }

    SECTION("Swaps the two preceding words at end of input")
    {
        tib::editor_context context;
        initialize_context(context, "one two three", 13);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("one three two"));
        REQUIRE(context.get_caret() == 13);
    }

    SECTION("Uses the last word and its predecessor when no word follows")
    {
        tib::editor_context context;
        initialize_context(context, "one two three", 10);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("one three two"));
        REQUIRE(context.get_caret() == 13);
    }

    SECTION("Swaps the previous word with the last word beginning at the caret")
    {
        tib::editor_context context;
        initialize_context(context, "one two three", 8);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("one three two"));
        REQUIRE(context.get_caret() == 13);
    }

    SECTION("Does nothing without a word before leading separators")
    {
        tib::editor_context context;
        initialize_context(context, "  one two", 0);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("  one two"));
        REQUIRE(context.get_caret() == 0);
    }

    SECTION("Swap the preceding words after trailing separators")
    {
        tib::editor_context context;
        initialize_context(context, "one$two,,", 9);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("two$one,,"));
        REQUIRE(context.get_caret() == 7);
    }

    SECTION("Swaps the preceding words from within trailing separators")
    {
        tib::editor_context context;
        initialize_context(context, "one two  ", 8);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("two one  "));
        REQUIRE(context.get_caret() == 7);
    }

    SECTION("Treats Unicode graphemes as word characters")
    {
        static const char text[] = "a" "e\xcc\x81" " \xc2\xa2" "b";
        tib::editor_context context;
        initialize_context(context, text, 6);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("\xc2\xa2" "b a" "e\xcc\x81"));
        REQUIRE(context.get_caret() == 8);
    }

    SECTION("Does nothing without two words")
    {
        tib::editor_context context;
        initialize_context(context, "one", 3);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("one"));
        REQUIRE(context.get_caret() == 3);
    }

    SECTION("Does nothing without any words")
    {
        tib::editor_context context;
        initialize_context(context, "  , ", 2);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("  , "));
        REQUIRE(context.get_caret() == 2);
    }

    SECTION("Uses the caret end of a selection and clears the selection")
    {
        tib::editor_context context;
        initialize_context(context, "one two three", 5);
        context.set_selection(0, 5);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        REQUIRE(context.get_text() == tib::cstring("two one three"));
        REQUIRE(context.get_caret() == 7);
        REQUIRE(!context.get_selection_state().has_selection());
    }

    SECTION("Undo restores a transpose as one operation")
    {
        tib::editor_context context;
        initialize_context(context, "one two", 4);
        const auto command = tib::editor_context::lookup_command("transpose-words");
        REQUIRE(command != nullptr);
        REQUIRE(command(context, 0, "transpose-words", nullptr) == 0);
        context.undo();
        REQUIRE(context.get_text() == tib::cstring("one two"));
        REQUIRE(context.get_caret() == 4);
    }
}
