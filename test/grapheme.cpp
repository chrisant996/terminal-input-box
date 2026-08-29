// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "test.h"
#include "test_util.h"
#include "tib.h"
#include "wcwidth.h"

constexpr uint32_t c_display_line_comparison_passes = 100000;

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
        caret = backward_one_grapheme(text.c_str(), text.length(), caret, &width);
        REQUIRE(caret == positions[count]);
        REQUIRE(width == samples[count].width, [&](){
            tib::cstring_t<WCHAR> ws;
            to_utf16(samples[count].text, tib::c_auto_length, ws);
            tib::cstring_t<WCHAR> msg;
            msg.printf(L"index     %u\ngrapheme  '%s'\nexpected  %u\nwidth     %u",
                       count, ws.c_str(), samples[count].width, width);
            DWORD written;
            WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), msg.c_str(), DWORD(msg.length()), &written, nullptr);
        });
    }

    uint16_t width = 1;
    REQUIRE(backward_one_grapheme(text.c_str(), text.length(), caret, &width) == 0);
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

class display_test_buffer : public tib::input_buffer
{
public:
    void set_text(const char* text, tib::textpos_t caret, tib::textpos_t anchor=-1)
    {
        m_text.set(text);
        if (anchor < 0)
            m_selection.set_caret(caret);
        else
            m_selection.set_selection(anchor, caret);
        ++m_change_counter;
    }

    void set_selection(tib::textpos_t anchor, tib::textpos_t caret)
    {
        m_selection.set_selection(anchor, caret);
    }

    tib::selection_state& get_selection_state_out()
    {
        return m_selection;
    }
};

static tib::cstring s_display_output;

class display_test_fixture
{
public:
    display_test_fixture(uint16_t max_width=10, bool horiz_scroll_markers=false,
                         uint16_t max_height=1, bool variable_height=false)
    : m_output(s_display_output)
    {
        m_old_coalesce = tib::g_coalesce_output;
        tib::g_coalesce_output = true;

        m_layout.max_width = max_width;
        m_layout.max_height = max_height;
        m_layout.variable_height = variable_height;
        m_style.border = &m_border;
        m_style.horiz_scroll_markers = horiz_scroll_markers;
        m_display.init_layout(&m_layout);
        m_display.init_buffer(&m_buffer);
        m_display.init_style(&m_style);
        m_display.set_origin(1, 1);
    }

    ~display_test_fixture()
    {
        tib::g_coalesce_output = m_old_coalesce;
        s_display_output.clear();
    }

    void display_initial(const char* text, tib::textpos_t caret, tib::textpos_t anchor=-1)
    {
        m_buffer.set_text(text, caret, anchor);
        REQUIRE(m_display.display() == false);
        s_display_output.clear();
    }

    display_test_buffer m_buffer;
    tib::display_manager m_display;

private:
    test_output_stream  m_output;
    tib::layout_info m_layout;
    tib::border_definition m_border;
    tib::style_info m_style;
    bool m_old_coalesce = false;
};

TEST_CASE("Display differential updates")
{
    SECTION("Redraws changed left text")
    {
        display_test_fixture fixture;
        fixture.m_display.set_left_text("old: ", 5);
        fixture.display_initial("abc", 0);

        fixture.m_display.set_left_text("new: ", 5);
        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(strstr(s_display_output.c_str(), "new: ") != nullptr);
    }

    SECTION("Reuses a line only when all displayed text matches")
    {
        display_test_fixture fixture;
        fixture.m_display.set_right_text("xyz", 3);
        fixture.display_initial("abc", 0);

        // Moving the caret forces a rebuild with an unchanged line.  Reusing
        // the displayed line must avoid outputting either part of that line.
        fixture.m_buffer.set_selection(1, 1);
        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(strpbrk(s_display_output.c_str(), "abc") == nullptr);
        REQUIRE(strpbrk(s_display_output.c_str(), "xyz") == nullptr);

        // Right text is part of the first displayed line, so changing it
        // must prevent reuse even though the input text is unchanged.
        s_display_output.clear();
        fixture.m_display.set_right_text("xyz", 3);
        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(strstr(s_display_output.c_str(), "xyz") != nullptr);
    }

    SECTION("Skips matching leading and trailing text")
    {
        display_test_fixture fixture;
        fixture.display_initial("abcde", 5);

        fixture.m_buffer.set_text("abXde", 5);
        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(strstr(s_display_output.c_str(), "X") != nullptr);
        REQUIRE(strstr(s_display_output.c_str(), "ab") == nullptr);
        REQUIRE(strstr(s_display_output.c_str(), "de") == nullptr);
    }

    SECTION("Compares faces along with text")
    {
        display_test_fixture fixture;
        fixture.display_initial("abc", 0);

        fixture.m_buffer.set_selection(1, 2);
        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(strstr(s_display_output.c_str(), "b") != nullptr);
        REQUIRE(strstr(s_display_output.c_str(), "a") == nullptr);
        REQUIRE(strstr(s_display_output.c_str(), "c") == nullptr);
    }

    SECTION("Does not split matching grapheme prefixes")
    {
        display_test_fixture fixture;
        fixture.display_initial("a\xcc\x81x", 4);

        fixture.m_buffer.set_text("a\xcc\x88x", 4);
        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(strstr(s_display_output.c_str(), "a\xcc\x88") != nullptr);
        REQUIRE(strstr(s_display_output.c_str(), "x") == nullptr);
    }

    SECTION("Does not split matching grapheme suffixes")
    {
        display_test_fixture fixture;
        fixture.display_initial("xa\xcc\x81", 4);

        fixture.m_buffer.set_text("xb\xcc\x81", 4);
        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(strstr(s_display_output.c_str(), "b\xcc\x81") != nullptr);
        REQUIRE(strstr(s_display_output.c_str(), "x") == nullptr);
    }
}

TEST_CASE("Display left text")
{
    SECTION("Participates in wrapping")
    {
        display_test_fixture fixture(5, false, 3, true);
        fixture.m_display.set_left_text("> ", 2);
        fixture.m_buffer.set_text("abcd", 4);

        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(strstr(s_display_output.c_str(), "> ") != nullptr);
        const tib::coord expected = { 1, 1 };
        REQUIRE(fixture.m_display.get_relative_cursor() == expected);
    }

    SECTION("Is omitted unless an input column remains")
    {
        display_test_fixture fixture(5, false, 3, true);
        fixture.m_display.set_left_text("12345", 5);
        fixture.m_buffer.set_text("abcd", 4);

        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(strstr(s_display_output.c_str(), "12345") == nullptr);
        const tib::coord expected = { 4, 0 };
        REQUIRE(fixture.m_display.get_relative_cursor() == expected);
    }

    SECTION("Is omitted when horizontally scrolled")
    {
        display_test_fixture fixture(8, true);
        fixture.m_display.set_left_text("1234567", 7);
        fixture.m_buffer.set_text("abcdefghijkl", 12);

        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(fixture.m_display.get_left() > 0);
        REQUIRE(strstr(s_display_output.c_str(), "1234567") == nullptr);
    }

    SECTION("Is displayed before horizontal scrolling starts")
    {
        display_test_fixture fixture(8, true);
        fixture.m_display.set_left_text("> ", 2);
        fixture.m_buffer.set_text("abc", 3);

        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(fixture.m_display.get_left() == 0);
        REQUIRE(strstr(s_display_output.c_str(), "> ") != nullptr);
    }

    SECTION("Is not displayed when the first logical line is scrolled away")
    {
        display_test_fixture fixture(5, false, 2);
        fixture.m_display.set_left_text("> ", 2);
        fixture.m_buffer.set_text("abcdefghijkl", 12);

        REQUIRE(fixture.m_display.display() == false);
        REQUIRE(fixture.m_display.get_top() > 0);
        REQUIRE(strstr(s_display_output.c_str(), "> ") == nullptr);
    }
}

TEST_CASE("Display vertical caret movement")
{
    display_test_fixture fixture(5, false, 3, true);
    fixture.display_initial("abc\ndef\nghi", 1, 0);

    const int32_t cursor_column = fixture.m_display.get_relative_cursor().x;
    REQUIRE(fixture.m_display.move_caret_vertically(
                1, cursor_column, fixture.m_buffer.get_selection_state_out(), true/*select*/));
    REQUIRE(fixture.m_buffer.get_selection_state().get_anchor() == 0);
    REQUIRE(fixture.m_buffer.get_selection_state().get_caret() == 5);

    REQUIRE(fixture.m_display.display() == false);
    REQUIRE(fixture.m_display.move_caret_vertically(
                -1, cursor_column, fixture.m_buffer.get_selection_state_out()));
    REQUIRE(fixture.m_buffer.get_selection_state().get_anchor() == 1);
    REQUIRE(fixture.m_buffer.get_selection_state().get_caret() == 1);
}

TEST_CASE("Display horizontal scrolling")
{
    display_test_fixture fixture(40, true);
    tib::cstring text;
    text.set("x\xe2\x9c\x94\xef\xb8\x8fy"); // x + U+2714 U+FE0F + y.
    for (uint32_t i = 0; i < 35; ++i)
        text.append("x");

    fixture.display_initial(text.c_str(), tib::textpos_t(text.length()));
    REQUIRE(fixture.m_display.get_left() == 1);
    REQUIRE(fixture.m_display.get_relative_cursor().x == 37);

    text.append("x");
    fixture.display_initial(text.c_str(), tib::textpos_t(text.length()));
    REQUIRE(fixture.m_display.get_left() == 1);
    REQUIRE(fixture.m_display.get_relative_cursor().x == 38);

    text.append("x");
    fixture.display_initial(text.c_str(), tib::textpos_t(text.length()));
    REQUIRE(fixture.m_display.get_left() == 7);
    REQUIRE(fixture.m_display.get_relative_cursor().x == 38);
}

TEST_CASE("Display multiline wrapping")
{
    display_test_fixture fixture(40, false, 2);
    tib::cstring text;
    for (uint32_t i = 0; i < 39; ++i)
        text.append("x");
    text.append("\xe2\x9c\x94\xef\xb8\x8f"); // U+2714 U+FE0F.

    fixture.display_initial(text.c_str(), 39);
    REQUIRE(fixture.m_display.get_relative_cursor().x == 0);
    REQUIRE(fixture.m_display.get_relative_cursor().y == 1);

    fixture.display_initial(text.c_str(), 42);
    REQUIRE(fixture.m_display.get_relative_cursor().x == 0);
    REQUIRE(fixture.m_display.get_relative_cursor().y == 1);

    fixture.display_initial(text.c_str(), tib::textpos_t(text.length()));
    REQUIRE(fixture.m_display.get_relative_cursor().x == 2);
    REQUIRE(fixture.m_display.get_relative_cursor().y == 1);
}

TEST_CASE("Display variable height scrolling")
{
    display_test_fixture fixture(10, false, 3, true);
    tib::cstring text;
    text.append("xxxxxxxxxx", 10);
    text.append("xxxxxxxxxx", 10);
    text.append("xxxxxxxxxx", 10);
    text.append("xxxxxxxxxx", 10);

    fixture.display_initial(text.c_str(), tib::textpos_t(text.length()));
    REQUIRE(fixture.m_display.get_top() == 2);

    text.set_length(31);
    fixture.display_initial(text.c_str(), tib::textpos_t(text.length()));
    REQUIRE(fixture.m_display.get_top() == 1);

    text.set_length(15);
    fixture.display_initial(text.c_str(), tib::textpos_t(text.length()));
    REQUIRE(fixture.m_display.get_top() == 0);
}

static void make_matching_display_line_data(tib::cstring& text, tib::cstring& matching_text,
                                            tib::cstring& faces, tib::cstring& matching_faces)
{
    text.set("The quick brown fox jumps over the lazy dog. "
             "The quick brown fox jumps over the lazy dog.");
    matching_text = text;
    faces.append_spaces(text.length());
    matching_faces = faces;
}

PERF_CASE("PERF, compare matching display line with memcmp")
{
    tib::cstring text;
    tib::cstring matching_text;
    tib::cstring faces;
    tib::cstring matching_faces;
    make_matching_display_line_data(text, matching_text, faces, matching_faces);

    uint32_t matches = 0;
    for (uint32_t pass = 0; pass < c_display_line_comparison_passes; ++pass)
    {
        if (text == matching_text && faces == matching_faces)
            ++matches;
    }

    REQUIRE(matches == c_display_line_comparison_passes);
}

PERF_CASE("PERF, compare matching display line by grapheme")
{
    tib::cstring text;
    tib::cstring matching_text;
    tib::cstring faces;
    tib::cstring matching_faces;
    make_matching_display_line_data(text, matching_text, faces, matching_faces);

    uint32_t matches = 0;
    for (uint32_t pass = 0; pass < c_display_line_comparison_passes; ++pass)
    {
        size_t pos = 0;
        size_t matching_pos = 0;
        while (pos < text.length() && matching_pos < matching_text.length())
        {
            const size_t next = forward_one_grapheme(text.c_str(), text.length(), uint32_t(pos));
            const size_t matching_next = forward_one_grapheme(matching_text.c_str(), matching_text.length(), uint32_t(matching_pos));
            const size_t length = next - pos;
            const size_t matching_length = matching_next - matching_pos;
            if (length != matching_length ||
                memcmp(text.c_str() + pos, matching_text.c_str() + matching_pos, length) != 0 ||
                memcmp(faces.c_str() + pos, matching_faces.c_str() + matching_pos, length) != 0)
                break;

            pos = next;
            matching_pos = matching_next;
        }

        if (pos == text.length() && matching_pos == matching_text.length())
            ++matches;
    }

    REQUIRE(matches == c_display_line_comparison_passes);
}
