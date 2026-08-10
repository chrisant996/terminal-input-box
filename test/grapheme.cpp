// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "test.h"
#include "tib.h"
#include "wcwidth.h"

struct grapheme_sample
{
    const char*         text;
    uint16_t            width;
};

static void check_back_one_grapheme(const grapheme_sample* samples, size_t count)
{
    tib::cstring text;
    std::vector<uint32_t> positions;
    for (size_t i = 0; i < count; ++i)
    {
        positions.emplace_back(uint32_t(text.length()));
        text.append(samples[i].text);
    }

    uint32_t caret = uint32_t(text.length());
    while (count)
    {
        --count;
        uint16_t width = 0;
        caret = back_one_grapheme(text.c_str(), text.length(), caret, width);
        REQUIRE(caret == positions[count]);
        REQUIRE(width == samples[count].width);
    }

    uint16_t width = 1;
    REQUIRE(back_one_grapheme(text.c_str(), text.length(), caret, width) == 0);
    REQUIRE(width == 0);
}

TEST_CASE("Back one grapheme")
{
    SECTION("ASCII and UTF8 encodings")
    {
        const grapheme_sample samples[] =
        {
            { "a", 1 },
            { "Z", 1 },
            { "\xc2\xa2", 1 },                 // U+00A2 CENT SIGN; 2-byte UTF8.
            { "\xe4\xb8\xad", 2 },             // U+4E2D CJK character; 3-byte UTF8.
            { "\xf0\x9f\x98\x80", 2 },         // U+1F600 GRINNING FACE; 4-byte UTF8.
        };
        check_back_one_grapheme(samples, std::size(samples));
    }

    SECTION("Combining marks")
    {
        const grapheme_sample samples[] =
        {
            { "a\xcc\x81", 1 },                 // a + U+0301 COMBINING ACUTE ACCENT.
            { "o\xcc\x82\xcc\x88", 1 },         // o + circumflex + diaeresis.
            { "u\xcc\x88\xcc\x84", 1 },         // u + diaeresis + macron.
        };
        check_back_one_grapheme(samples, std::size(samples));
    }

    SECTION("Emoji and variation selectors")
    {
        const grapheme_sample samples[] =
        {
            { "\xf0\x9f\x98\x80", 2 },                         // Grinning face.
            { "\xf0\x9f\x9a\x80", 2 },                         // Rocket.
            { "\xe2\x9d\xa4\xef\xb8\x8f", 2 },                 // Heart ending in U+FE0F.
            { "\xe2\x9c\x88\xef\xb8\x8f", 2 },                 // Airplane ending in U+FE0F.
            { "\xe2\x9d\xa4\xef\xb8\x8f\xe2\x80\x8d"
              "\xf0\x9f\x94\xa5", 2 },                         // Flaming heart; contains U+FE0F.
            { "\xf0\x9f\x91\xa8\xe2\x80\x8d"
              "\xf0\x9f\x92\xbb", 2 },                         // Man technologist.
            { "\xf0\x9f\x99\x86\xe2\x80\x8d"
              "\xe2\x99\x80\xef\xb8\x8f", 2 },                 // Woman gesturing OK.
            { "\xf0\x9f\x91\xa8\xe2\x80\x8d"
              "\xe2\x9a\x95\xef\xb8\x8f", 2 },                 // Man health worker.
        };
        check_back_one_grapheme(samples, std::size(samples));
    }

    SECTION("Flag sequences")
    {
        const grapheme_sample samples[] =
        {
            { "\xf0\x9f\x87\xba\xf0\x9f\x87\xb8", 2 },         // United States.
            { "\xf0\x9f\x87\xaf\xf0\x9f\x87\xb5", 2 },         // Japan.
            { "\xf0\x9f\x87\xa8\xf0\x9f\x87\xa6", 2 },         // Canada.
        };
        check_back_one_grapheme(samples, std::size(samples));
    }
}
