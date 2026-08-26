// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_input.h"
#include "tib_output.h"
#include "tib_termcap.h"
#include <memory>
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
static mouse_input_mode s_mouse_input_mode = mouse_input_mode::none;
static bool s_mouse_sgr_encoding = true;

static int32_t s_term_began = 0;
static coord s_last_term_size { -1, -1 };
static HANDLE s_hin = 0;
static HANDLE s_hout = 0;
static DWORD s_prev_input_mode = 0;
static DWORD s_prev_output_mode = 0;
static DWORD s_prev_mouse_button_state = 0;

#ifdef _WIN32
#ifdef DEBUG
static const DWORD c_idMainThread = GetCurrentThreadId();
#endif
#endif

class pushed_input
{
    // The data size was originally 16, but it needs to be able to hold at
    // least as many mouse click/release events as can be generated from a
    // single MOUSE_EVENT_RECORD.
    //
    // One event could be `CSI < 255 ; 9999 ; 9999 M` so call that 17 bytes,
    // and round up to 20.
    //
    // One MOUSE_EVENT_RECORD could generate one event per button state
    // change, and it supports 3 buttons plus potentially wheel and hwheel, so
    // call that 5 events per record.
    //
    // So call it 20 * 5 = 100, then round up to a power of two = 128 bytes.
    enum : uint16_t { c_data_size = 128 };

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
    uint8_t             m_data[c_data_size];
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
    return c;
}

int32_t pushed_input::read()
{
    assert(!empty());
    const char c = m_data[m_head];
    ++m_head;
    --m_count;
    m_head %= std::size(m_data);
    return c;
}

void term_begin()
{
#ifdef _WIN32
#ifdef DEBUG
    assert(c_idMainThread == GetCurrentThreadId());
#endif
#endif

    assert(s_term_began >= 0);
    if (s_term_began < 0)
        s_term_began = 0;

    if (!s_term_began)
    {
        s_last_term_size = get_terminal_size();

        HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
        if (GetConsoleMode(hin, &s_prev_input_mode))
        {
            s_hin = hin;
            s_prev_mouse_button_state = 0;
            if (GetKeyState(VK_LBUTTON) < 0)
                s_prev_mouse_button_state |= FROM_LEFT_1ST_BUTTON_PRESSED;
            if (GetKeyState(VK_MBUTTON) < 0)
                s_prev_mouse_button_state |= FROM_LEFT_2ND_BUTTON_PRESSED;
            if (GetKeyState(VK_RBUTTON) < 0)
                s_prev_mouse_button_state |= RIGHTMOST_BUTTON_PRESSED;
        }
        else
        {
            assert(!s_hin);
            s_hin = 0;
        }

        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleMode(hout, &s_prev_output_mode))
        {
            s_hout = hout;
        }
        else
        {
            assert(!s_hout);
            s_hout = 0;
        }
    }

    ++s_term_began;
}

void term_end()
{
#ifdef _WIN32
#ifdef DEBUG
    assert(c_idMainThread == GetCurrentThreadId());
#endif
#endif

    assert(s_term_began > 0);
    if (s_term_began <= 0)
        return;

    if (s_term_began == 1)
    {
        if (s_hout)
        {
            term_out(c_show_cursor);
            // FUTURE: cursor shape.
            term_out("\x1b[m");

            if (s_mouse_input_mode != mouse_input_mode::none)
                enable_mouse_input(mouse_input_mode::none, false);
        }

        if (s_hin)
            SetConsoleMode(s_hin, s_prev_input_mode);
        if (s_hout)
            SetConsoleMode(s_hout, s_prev_output_mode);

        s_hin = 0;
        s_hout = 0;
    }

    --s_term_began;
}

void term_sigint()
{
    if (s_term_began)
    {
        s_term_began = 1;
        term_end();
    }
}

class auto_term_end
{
public:
    ~auto_term_end() { term_sigint(); }
};
static auto_term_end s_auto_term_end;

#ifdef _WIN32
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

static bool append_one_mouse_sequence(uint8_t flags, uint16_t x, uint16_t y, bool release, cstring& out)
{
    if (s_mouse_sgr_encoding)
    {
        // SGR Encoding.
        const char op = release ? 'm' : 'M';
        out.printf("\x1b[<%u;%u;%u%c", flags, x, y, op);
        return true;
    }
    else
    {
        // DEFAULT Encoding.
        if (release)
            flags = uint8_t((flags & 0x1c) | 3); // Generic release.
        const uint8_t Cb = uint8_t(0x20 + flags);
        const uint8_t Cx = (x < 1 || x > 223) ? 0 : uint8_t(x + 0x20);
        const uint8_t Cy = (y < 1 || y > 223) ? 0 : uint8_t(y + 0x20);
        out.append("\x1b[M");
        out.append(reinterpret_cast<const char*>(&Cb), 1);
        out.append(reinterpret_cast<const char*>(&Cx), 1);
        out.append(reinterpret_cast<const char*>(&Cy), 1);
        return true;
    }

    return false;
}

static bool generate_mouse_sequences(const MOUSE_EVENT_RECORD& record, cstring& out)
{
    // The implementation here supports these protocols:  VT200, DRAG, ANY.
    // It does not support SGR Pixels Encoding.
    //
    // https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Mouse-Tracking

    out.clear();

    // Remember the button state, to differentiate press vs release.
    const auto prv = s_prev_mouse_button_state;
    s_prev_mouse_button_state = record.dwButtonState;

    // Xterm generates a separate sequence per button event.  Attempt to
    // emulate that reasonably well.
    const auto btn = record.dwButtonState;
    const bool left_held = !!(btn & FROM_LEFT_1ST_BUTTON_PRESSED);
    const bool middle_held = !!(btn & FROM_LEFT_2ND_BUTTON_PRESSED);
    const bool right_held = !!(btn & RIGHTMOST_BUTTON_PRESSED);
    const bool left_button_change = left_held != !!(prv & FROM_LEFT_1ST_BUTTON_PRESSED);
    const bool middle_button_change = middle_held != !!(prv & FROM_LEFT_2ND_BUTTON_PRESSED);
    const bool right_button_change = right_held != !!(prv & RIGHTMOST_BUTTON_PRESSED);
    const bool wheel = !!(record.dwEventFlags & MOUSE_WHEELED) && (s_mouse_input_mode >= mouse_input_mode::VT200);
    const bool hwheel = !!(record.dwEventFlags & MOUSE_HWHEELED) && (s_mouse_input_mode >= mouse_input_mode::VT200);

    constexpr DWORD ALT_PRESSED = LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED;
    constexpr DWORD CTRL_PRESSED = LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED;

    uint8_t flags = 0;
    if (record.dwControlKeyState & SHIFT_PRESSED)  flags |= 4;      // MB+S
    if (record.dwControlKeyState & ALT_PRESSED)    flags |= 8;      // MB+A (also MB4)
    if (record.dwControlKeyState & CTRL_PRESSED)   flags |= 16;     // MB+C

    const uint16_t x = record.dwMousePosition.X + 1;
    const uint16_t y = record.dwMousePosition.Y + 1;

    bool ret = false;

    // Left click/release.
    if (left_button_change)
        ret |= append_one_mouse_sequence(flags + 0, x, y, !left_held, out); // MB1

    // Middle click/release.
    if (middle_button_change)
        ret |= append_one_mouse_sequence(flags + 1, x, y, !middle_held, out); // MB2

    // Right click/release.
    if (right_button_change)
        ret |= append_one_mouse_sequence(flags + 2, x, y, !right_held, out); // MB3

    // Mouse wheel.
    if (wheel)
    {
        const auto direction = int16_t(0 - int16_t(HIWORD(record.dwButtonState))) / 120;
        ret |= append_one_mouse_sequence(flags + ((direction < 0) ? 64 : 65), x, y, false, out); // MB4/MB5
    }

    // Mouse horizontal wheel.
    if (hwheel)
    {
        const auto direction = int16_t(int16_t(HIWORD(record.dwButtonState)) / 120);
        ret |= append_one_mouse_sequence(flags + ((direction < 0) ? 66 : 67), x, y, false, out); // MB6/MB7
    }

    // Drag.
    if (!ret && (record.dwEventFlags & MOUSE_MOVED))
    {
assert(!left_held);
assert(!middle_held);
assert(!right_held);
        if ((s_mouse_input_mode == mouse_input_mode::ANY) ||
            (s_mouse_input_mode == mouse_input_mode::DRAG && (left_held || middle_held || right_held)))
        {
            uint8_t drag_flags = (flags & 0x1c) | 32;               // Motion
            if (left_held)         drag_flags += 0;                 // MB1
            else if (middle_held)  drag_flags += 1;                 // MB2
            else if (right_held)   drag_flags += 2;                 // MB3
            else                   drag_flags += 3;                 // None
            ret |= append_one_mouse_sequence(drag_flags, x, y, false, out);
        }
    }

    return ret;
}
#endif // _WIN32

int32_t term_in()
{
#ifdef _WIN32
#ifdef DEBUG
    assert(c_idMainThread == GetCurrentThreadId());
#endif
#endif

    assert(s_term_began);
    if (!s_term_began || !s_hin)
        return c_input_terminal_eof;

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

    const coord term_size = get_terminal_size();
    if (s_last_term_size != term_size)
    {
        s_last_term_size = term_size;
        return uint8_t(c_input_terminal_resize);
    }

    if (hook_term_in)
    {
        const int32_t c = hook_term_in();
        assert(c < 0 || !(c & 0xffffff00));
        return c;
    }

#ifdef _WIN32
    static cstring s_tmp_utf8;

again:
    DWORD num_read;
#define USE_READCONSOLEINPUT
#ifdef USE_READCONSOLEINPUT
    // FUTURE: add a mode that conditionally applies/removes
    // ENABLE_MOUSE_INPUT like Clink does?
    INPUT_RECORD record;
    if (!ReadConsoleInputW(s_hin, &record, 1, &num_read) || 1 != num_read)
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
            if (generate_mouse_sequences(record.Event.MouseEvent, seq))
            {
                for (size_t i = 0; i < seq.length(); ++i)
                    s_pushed.push(seq.c_str()[i]);
                return s_pushed.read();
            }
        }
        goto again;
    case WINDOW_BUFFER_SIZE_EVENT:
        {
            const coord term_size = get_terminal_size();
            if (s_last_term_size != term_size)
            {
                s_last_term_size = term_size;
                return c_input_terminal_resize;
            }
        }
        goto again;
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
#else
    // TODO-LINUX: Use fgetc?
    // TODO-LINUX: What to do upon EOF?
#endif
}

int32_t term_in_peek()
{
#ifdef _WIN32
#ifdef DEBUG
    assert(c_idMainThread == GetCurrentThreadId());
#endif
#endif

    assert(s_term_began);
    if (!s_term_began || !s_hin)
        return c_input_terminal_eof;

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
    assert(c_idMainThread == GetCurrentThreadId());
#endif
#endif

    assert(s_term_began);
    if (!s_term_began || !s_hin)
        return false;

#ifdef _WIN32
#ifdef DEBUG
    DWORD mode;
    assert(GetConsoleMode(s_hin, &mode));
#endif
#endif

    if (!s_pushed.empty())
        return true;
    if (s_macro_playback)
        return true;

    const coord term_size = get_terminal_size();
    if (s_last_term_size != term_size)
    {
        s_last_term_size = term_size;
        s_pushed.push(c_input_terminal_resize);
        return true;
    }

    if (hook_term_in_avail)
        return hook_term_in_avail(_timeout);

#ifdef _WIN32
    bool ret = !s_pushed.empty();
    bool sleep_on_error = false;
    const DWORD stop = GetTickCount() + _timeout;
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

        const DWORD waited = WaitForSingleObject(s_hin, timeout);
        if (waited == WAIT_TIMEOUT)
            break;

        DWORD count;
        INPUT_RECORD record;
        if (!ReadConsoleInputW(s_hin, &record, 1, &count) || 1 != count)
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
                if (generate_mouse_sequences(record.Event.MouseEvent, seq))
                {
                    for (size_t i = 0; i < seq.length(); ++i)
                        s_pushed.push(seq.c_str()[i]);
                    ret = true;
                }
            }
            break;

        case WINDOW_BUFFER_SIZE_EVENT:
#ifdef USE_READCONSOLEINPUT
            if (s_pushed.push(c_input_terminal_resize))
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
#ifdef _WIN32
#ifdef DEBUG
    assert(c_idMainThread == GetCurrentThreadId());
#endif
#endif

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

void enable_mouse_input(mouse_input_mode mode, bool sgr_encoding)
{
#ifdef _WIN32
#ifdef DEBUG
    assert(c_idMainThread == GetCurrentThreadId());
#endif
#endif

    assert(s_term_began);
    if (!s_term_began || !s_hin || !s_hout)
        return;

    // https://tintin.mudhalla.net/info/xterm/
    //
    // XTERM MOUSE TRACKING
    //
    // Code             Effect  Note
    // ----             ------  ----
    // CSI ? 1000 h     MTM     Enable Mouse Tracking Mode (VT200 Protocol; press, release, wheel)
    // CSI ? 1001 h     HMTM    Set Highlight Mouse Tracking Mode
    // CSI ? 1002 h     BMMTM   Set Button Motion Mouse Tracking Mode (DRAG Protocol; press, release, wheel, drag while button down)
    // CSI ? 1003 h             (ANY Protocol; all mouse events)
    // CSI ? 1004 h     WFTM    Enable Window Focus Tracking Mode
    // CSI ? 1006 h     DMTM    Set Decimal Mouse Tracking Mode (SGR Encoding)
    // CSI ? 1016 h             (SGR Pixels Encoding)
    //
    // NOTE:  The Win32 implementation in generate_mouse_sequences() supports
    // these protocols:  VT200, DRAG, ANY.  It supports both DEFAULT and SGR
    // encodings for each protocol.  It does not support SGR Pixels encoding
    // or Window Focus Tracking Mode.

#ifdef _WIN32
    DWORD old_console_mode;
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(hin, &old_console_mode) &&
        !(old_console_mode & ENABLE_VIRTUAL_TERMINAL_INPUT))
    {
        DWORD new_console_mode = old_console_mode;
        new_console_mode &= ~(ENABLE_MOUSE_INPUT|ENABLE_QUICK_EDIT_MODE);
        if (mode != mouse_input_mode::none)
            new_console_mode |= ENABLE_MOUSE_INPUT;
        else
            new_console_mode |= (s_prev_input_mode & ENABLE_QUICK_EDIT_MODE);
        if (old_console_mode != new_console_mode)
            SetConsoleMode(hin, new_console_mode);
    }
    else
#endif
    {
        switch (mode)
        {
        case mouse_input_mode::none:    term_out("\x1b[?1000l\x1b[?1002l\x1b[?1003l"); break;
        case mouse_input_mode::VT200:   term_out("\x1b[?1000h"); break;
        case mouse_input_mode::DRAG:    term_out("\x1b[?1002h"); break;
        case mouse_input_mode::ANY:     term_out("\x1b[?1003h"); break;
        default:                        assert(false); break;
        }

        term_out(sgr_encoding ? "\x1b[?1006h" : "\x1b[?1006l");
    }

    s_mouse_input_mode = mode;
    s_mouse_sgr_encoding = sgr_encoding;
}

} // namespace tib
