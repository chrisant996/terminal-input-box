// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_input.h"
#include <conio.h>
#include <assert.h>

namespace tib {

int32_t (*hook_term_in)() = nullptr;
bool (*hook_term_in_avail)() = nullptr;

struct macro_playback
{
    cstring             m_text;
    size_t              m_index = 0;
    macro_playback*     m_next = nullptr;
};

static macro_playback* s_macro_playback = nullptr;

#ifdef _WIN32
#ifdef DEBUG
static const DWORD c_idMainThread = GetCurrentThreadId();
#endif
#endif

struct pushed_input
{
    bool                empty() const { return !count; }
    bool                push(uint8_t c);
    uint8_t             peek() const;
    uint8_t             read();

    uint8_t             data[16];
    uint16_t            head = 0;
    uint16_t            count = 0;
};

static pushed_input s_pushed;

bool pushed_input::push(uint8_t c)
{
    assert(count < std::size(data));
    if (count >= std::size(data))
        return false;

    data[(head + count) % std::size(data)] = c;
    ++count;
    return true;
}

uint8_t pushed_input::peek() const
{
    assert(!empty());
    return data[head];
}

uint8_t pushed_input::read()
{
    assert(!empty());
    const char c = data[head];
    ++head;
    --count;
    head %= std::size(data);
    return c;
}

int32_t term_in()
{
#ifdef _WIN32
#ifdef DEBUG
    assert(c_idMainThread == GetCurrentThreadId());
#endif
#endif

    if (!s_pushed.empty())
        return s_pushed.read();

    if (s_macro_playback)
    {
        assert(s_macro_playback->m_index < s_macro_playback->m_text.length());
        const char c = s_macro_playback->m_text.c_str()[s_macro_playback->m_index++];
        if (s_macro_playback->m_index >= s_macro_playback->m_text.length())
        {
            macro_playback* d = s_macro_playback;
            s_macro_playback = s_macro_playback->m_next;
            delete d;
        }
        return uint8_t(c);
    }

    // TODO: Differentiate between EOF versus other failures.

    if (hook_term_in)
    {
        const int32_t c = hook_term_in();
        assert(c < 0 || !(c & 0xffffff00));
        return c;
    }

#ifdef _WIN32
    static cstring s_tmp_utf8;

    // TODO: Cache for performance; maybe have an init_terminal() function?
    DWORD mode;
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(h, &mode))
    {
        WCHAR tmp[2];
        size_t available = 0;
        DWORD num_read;
        if (!ReadConsoleW(h, tmp, 1, &num_read, nullptr) || 1 != num_read)
            return -1;
        ++available;
        if (IS_HIGH_SURROGATE(tmp[0]))
        {
            assert(1 == available);
            if (!ReadConsoleW(h, tmp + available, 1, &num_read, nullptr) || 1 != num_read)
            {
                // Return U+FFFD, the invalid character codepoint.
                s_pushed.push(0xbf);
                s_pushed.push(0xbd);
                return uint8_t(0xef);
            }
            ++available;
        }
        if (!to_utf8(tmp, available, s_tmp_utf8) || s_tmp_utf8.empty())
            return -1;
        const char c = s_tmp_utf8.c_str()[0];
        for (size_t i = 1; i < s_tmp_utf8.length(); ++i)
            s_pushed.push(s_tmp_utf8.c_str()[i]);
        s_tmp_utf8.clear();
        return uint8_t(c);
    }
    else
    {
        char c;
        DWORD num_read;
        if (!ReadFile(h, &c, 1, &num_read, nullptr) || !num_read)
            return -1;
        return uint8_t(c);
    }
#else
    // TODO-LINUX: Use fgetc?
    // TODO-LINUX: What to do upon EOF?
#endif
}

int32_t term_in_peek()
{
    if (!s_pushed.empty())
        return s_pushed.peek();

    if (s_macro_playback)
    {
        assert(s_macro_playback->m_index < s_macro_playback->m_text.length());
        const char c = s_macro_playback->m_text.c_str()[s_macro_playback->m_index];
        return uint8_t(c);
    }

    if (!term_in_avail())
        return -1;

    const int32_t c = term_in();
    if (c < 0)
        return c;
    assert(!(c & 0xffffff00));

    s_pushed.push(uint8_t(c));
    return c;
}

bool term_in_avail()
{
    if (!s_pushed.empty())
        return true;
    if (s_macro_playback)
        return true;

    if (hook_term_in_avail)
        return hook_term_in_avail();

#ifdef _WIN32
    // TODO: check for pending terminal input.
    // TODO: surrogate pairs could be complicated...
#else
    // TODO-LINUX: Alternative Linux implementation.
#endif
    return false;
}

bool term_push_macro_text(const char* text, size_t len)
{
    macro_playback* m = new macro_playback;
    if (!m)
        return false;

    if (!m->m_text.set(text, len))
    {
        delete m;
        return false;
    }

    m->m_next = s_macro_playback;
    s_macro_playback = m;
    return true;
}

} // namespace tib
