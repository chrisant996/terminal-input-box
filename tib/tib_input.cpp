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

hook_term_in_func_t hook_term_in = nullptr;
hook_term_in_avail_func_t hook_term_in_avail = nullptr;

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

class pushed_input
{
public:
                        pushed_input() = default;
    bool                empty() const { return !count; }
    bool                has_capacity(size_t num) const { return std::size(data) - count >= num; }
    bool                push(uint8_t c);
#ifdef _WIN32
    bool                push_utf16(WCHAR c);
#endif
    bool                push_invalid();
    uint8_t             peek() const;
    uint8_t             read();

private:
    uint8_t             data[16];
    uint16_t            head = 0;
    uint16_t            count = 0;

#ifdef _WIN32
    WCHAR               m_high_surrogate = 0;
    cstring             m_tmp_utf8;
#endif
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

#ifdef _WIN32
bool pushed_input::push_utf16(WCHAR c)
{
    // If c is a high surrogate then cache it for later.
    if (IS_HIGH_SURROGATE(c))
    {
        const WCHAR ls = m_high_surrogate;
        m_high_surrogate = c;
        if (ls)
        {
            // If a high surrogate
            return push_invalid();
        }
        return true;
    }

    // If a high surrogate is cached then complete it.
    if (m_high_surrogate)
    {
        if (!IS_LOW_SURROGATE(c))
        {
            if (!push_invalid())
                return false;

convert_c:
            if (!to_utf8(&c, 1, m_tmp_utf8))
                return false;

push_utf8:
            if (!has_capacity(m_tmp_utf8.length()))
                return false;
            for (size_t i = 0; i < m_tmp_utf8.length(); ++i)
                push(m_tmp_utf8.c_str()[i]);
            return true;
        }

        WCHAR convert[2];
        convert[0] = m_high_surrogate;
        convert[1] = c;
        m_high_surrogate = 0;
        if (!to_utf8(convert, 2, m_tmp_utf8))
            return false;
        goto push_utf8;
    }

    goto convert_c;
}
#endif

bool pushed_input::push_invalid()
{
    return push(0xef) && push(0xbf) && push(0xbd);
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

    if (hook_term_in)
    {
        const int32_t c = hook_term_in();
        assert(c < 0 || !(c & 0xffffff00));
        return c;
    }

#ifdef _WIN32
    static cstring s_tmp_utf8;

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
                if (!s_pushed.push_invalid())
                    return -1;
                return s_pushed.read();
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

bool term_in_avail(const DWORD _timeout)
{
    if (!s_pushed.empty())
        return true;
    if (s_macro_playback)
        return true;

    if (hook_term_in_avail)
        return hook_term_in_avail(_timeout);

#ifdef _WIN32
    bool ret = !s_pushed.empty();
    bool sleep_on_error = false;
    const DWORD stop = GetTickCount() + _timeout;
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    while (!ret)
    {
        DWORD timeout = stop - GetTickCount();
        if (timeout > _timeout)
            timeout = 0;

        if (sleep_on_error)
        {
            sleep_on_error = false;
            const DWORD sleep = (timeout > 1000) ? timeout - 1000 : timeout;
            Sleep(sleep);
            timeout -= sleep;
        }

        const DWORD waited = WaitForSingleObject(hin, timeout);
        if (waited == WAIT_TIMEOUT)
            break;

        DWORD count;
        INPUT_RECORD record;
        if (!ReadConsoleInputW(hin, &record, 1, &count))
        {
            // Handle's probably invalid if ReadConsoleInput() failed.
            sleep_on_error = true;
            continue;
        }

        switch (record.EventType)
        {
        case KEY_EVENT:
            // Because of ENABLE_VIRTUAL_TERMINAL_PROCESSING there is very
            // little to do here.
            ret = s_pushed.push_utf16(record.Event.KeyEvent.uChar.UnicodeChar);
            break;

        case MOUSE_EVENT:
            // REVIEW: should not happen with ENABLE_MOUSE_INPUT missing, and
            // using ReadConsoleW claims to not return them (mouse support is
            // not implemented by ENABLE_VIRTUAL_TERMINAL_PROCESSING?).
            assert(false);
            break;

        case WINDOW_BUFFER_SIZE_EVENT:
            // REVIEW: can't really do anything with this unless term_in()
            // also uses ReadConsoleInputW().
            break;
        }

        if (!timeout)
            break;
    }
    return ret;
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
