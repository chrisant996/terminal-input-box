// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "test.h"
#include "test_util.h"
#include "tib.h"

TEST_CASE("Undo grouping")
{
    tib::editor_context context;
    context.initialize();

    SECTION("Merge insert into previous group")
    {
        const char utf8[] =
            "\xc2\xa2"          // U+00A2, 2 bytes.
            "\xe2\x82\xac"      // U+20AC, 3 bytes.
            "\xf0\x90\x8d\x88"; // U+10348, 4 bytes.
        for (const char c : utf8)
        {
            if (c)
                context.insert_char(c);
        }
        REQUIRE(context.get_text() == tib::cstring(utf8));

        context.undo();
        REQUIRE(context.get_text() == tib::cstring("\xc2\xa2\xe2\x82\xac"));

        context.undo();
        REQUIRE(context.get_text() == tib::cstring("\xc2\xa2"));

        context.undo();
        REQUIRE(context.get_text().empty());

        context.redo();
        REQUIRE(context.get_text() == tib::cstring("\xc2\xa2"));

        context.redo();
        REQUIRE(context.get_text() == tib::cstring("\xc2\xa2\xe2\x82\xac"));

        context.redo();
        REQUIRE(context.get_text() == tib::cstring(utf8));
    }

    SECTION("Continuation byte without previous group")
    {
        context.insert_char(char(0x80));
        context.undo();
        REQUIRE(context.get_text().empty());
    }

    SECTION("ASCII bytes remain separate groups")
    {
        context.insert_char('a');
        context.insert_char('b');
        context.undo();
        REQUIRE(context.get_text() == tib::cstring("a"));
    }
}
