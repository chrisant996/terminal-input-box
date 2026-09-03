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
