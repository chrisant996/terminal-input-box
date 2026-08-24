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

// Some internal magic values, leveraging invalid UTF8 bytes.
constexpr char c_input_terminal_resize_magic_char = char(c_input_terminal_resize);
static_assert(uint8_t(c_input_terminal_resize_magic_char) == 0xfe);

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

static int32_t translate_special(char c)
{
    // Handle special internal values.
    switch (c)
    {
    case c_input_terminal_resize_magic_char:
        return c_input_terminal_resize;
    }

    return uint8_t(c);
}

class pushed_input
{
public:
                        pushed_input() = default;
    bool                empty() const { return !m_count; }
    bool                has_capacity(size_t num) const { return std::size(m_data) - m_count >= num; }
    bool                push(uint8_t c);
#ifdef _WIN32
    int32_t             push_utf16(WCHAR c);
#endif
    bool                push_invalid();
    int32_t             peek() const;
    int32_t             read();

private:
    uint8_t             m_data[16];
    uint16_t            m_head = 0;
    uint16_t            m_count = 0;

#ifdef _WIN32
    WCHAR               m_high_surrogate = 0;
    cstring             m_tmp_utf8;
#endif
};

static pushed_input s_pushed;

bool pushed_input::push(uint8_t c)
{
    assert(m_count < std::size(m_data));
    if (m_count >= std::size(m_data))
        return false;

    if (m_high_surrogate && !push_invalid())
        return false;

    m_data[(m_head + m_count) % std::size(m_data)] = c;
    ++m_count;
    return true;
}

#ifdef _WIN32
int32_t pushed_input::push_utf16(WCHAR c)
{
    // If c is a high surrogate then cache it for later.
    if (IS_HIGH_SURROGATE(c))
    {
        const int32_t pushed = m_high_surrogate ? push_invalid() : -1;
        m_high_surrogate = c; // After push_invalid() because that clears it.
        return pushed;
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
    m_high_surrogate = 0;
    return push(0xef) && push(0xbf) && push(0xbd);
}

int32_t pushed_input::peek() const
{
    assert(!empty());
    const char c = m_data[m_head];
    return translate_special(c);
}

int32_t pushed_input::read()
{
    assert(!empty());
    const char c = m_data[m_head];
    ++m_head;
    --m_count;
    m_head %= std::size(m_data);
    return translate_special(c);
}

static bool is_invalid_keyevent(KEY_EVENT_RECORD& record)
{
    // Only respond to key down events.
    if (!record.bKeyDown)
    {
        // WARNING:  Some times conhost can send through ALT codes, with the
        // resulting Unicode code point in the Alt key-up event.  I don't know
        // whether that also happens when ENABLE_VIRTUAL_TERMINAL_PROCESSING
        // is present, but I think that would be a console bug, and not
        // something for this to attempt to mitigate.
        return true;
    }

    // Ignore unaccompanied Alt/Ctrl/Shift/Windows key presses.  These can
    // happen even when ENABLE_VIRTUAL_TERMINAL_PROCESSING is present.
    switch (record.wVirtualKeyCode)
    {
    case VK_MENU:
    case VK_CONTROL:
    case VK_SHIFT:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    }

    return false;
}

static bool generate_mouse_sequence(const MOUSE_EVENT_RECORD& record, cstring& out)
{
    // TODO: generate xterm mouse sequences; might need additional state about
    // the previous mouse input status.
    return false;
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
again:
        DWORD num_read;
#define USE_READCONSOLEINPUT
#ifdef USE_READCONSOLEINPUT
        // FUTURE: add a mode that conditionally applies/removes
        // ENABLE_MOUSE_INPUT like Clink does?
        INPUT_RECORD record;
        if (!ReadConsoleInputW(h, &record, 1, &num_read) || 1 != num_read)
            return -1;
        switch (record.EventType)
        {
        case KEY_EVENT:
            if (is_invalid_keyevent(record.Event.KeyEvent))
                goto again;
            break;
        case MOUSE_EVENT:
            {
                cstring seq;
                if (generate_mouse_sequence(record.Event.MouseEvent, seq))
                {
                    for (const char* s = seq.c_str(); *s; ++s)
                        s_pushed.push(*s);
                    return true;
                }
            }
            goto again;
        case WINDOW_BUFFER_SIZE_EVENT:
            return c_input_terminal_resize;
        default:
            assert(false);
        case MENU_EVENT:
        case FOCUS_EVENT:
            goto again;
        }
        assert(record.EventType == KEY_EVENT);
#define tmp_input record.Event.KeyEvent.uChar.UnicodeChar
#else
        WCHAR tmp;
        if (!ReadConsoleW(h, &tmp, 1, &num_read, nullptr) || 1 != num_read)
            return -1;
#define tmp_input tmp
#endif
        const int32_t pushed = s_pushed.push_utf16(tmp_input);
        if (pushed < 0)
            goto again;
        return s_pushed.read();
#undef tmp_input
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

    // term_in_avail() can queue multiple UTF8 bytes for one UTF16 input
    // character.  Return the head in place; reading and pushing it back would
    // rotate the queued bytes.
    if (!s_pushed.empty())
        return s_pushed.peek();

    const int32_t c = term_in();
    if (c < 0)
        return c;
    assert(!(c & 0xffffff00));

    s_pushed.push(uint8_t(c));
    return c;
}

bool term_in_avail(const DWORD _timeout)
{
#ifdef _WIN32
#ifdef DEBUG
    DWORD mode;
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    assert(GetConsoleMode(h, &mode));
#endif
#endif

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
        if (!ReadConsoleInputW(hin, &record, 1, &count) || 1 != count)
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
            if (!is_invalid_keyevent(record.Event.KeyEvent))
            {
                const int32_t pushed = s_pushed.push_utf16(record.Event.KeyEvent.uChar.UnicodeChar);
                if (pushed < 0)
                    continue;
                ret = (pushed > 0);
            }
            break;

        case MOUSE_EVENT:
            {
                cstring seq;
                if (generate_mouse_sequence(record.Event.MouseEvent, seq))
                {
                    for (const char* s = seq.c_str(); *s; ++s)
                        s_pushed.push(*s);
                    ret = true;
                }
            }
            break;

        case WINDOW_BUFFER_SIZE_EVENT:
#ifdef USE_READCONSOLEINPUT
            if (s_pushed.push(int8_t(c_input_terminal_resize)))
                ret = true;
#else
            // REVIEW: can't really do anything with this unless term_in()
            // also uses ReadConsoleInputW().
#endif
            break;
        }

        if (!timeout)
            break;
    }
#else
    // TODO-LINUX: Alternative Linux implementation.
    ret = false;
#endif
    return ret;
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

void enable_mouse_input(bool enable)
{
#ifdef _WIN32
    DWORD old_mode;
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(hin, &old_mode))
    {
        DWORD new_mode = old_mode;
        if (enable)
            new_mode |= ENABLE_MOUSE_INPUT;
        else
            new_mode &= ~ENABLE_MOUSE_INPUT;
        SetConsoleMode(hin, new_mode);
    }
#else
    // TODO-LINUX: alternative implementation using escape codes to enable mouse input.
#endif
}

} // namespace tib
