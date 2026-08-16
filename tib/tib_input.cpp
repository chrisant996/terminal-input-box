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

struct macro_playback
{
    cstring             m_text;
    size_t              m_index = 0;
    macro_playback*     m_next = nullptr;
};

static macro_playback* s_macro_playback = nullptr;

int32_t term_in()
{
    // TODO:  Assert/enforce single threaded.
    // TODO: Pushed input.

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
        return hook_term_in();

#ifdef _WIN32
    static cstring s_pending_utf8;
    static size_t s_pending_head = 0;

    if (s_pending_utf8.length())
    {
        if (s_pending_head < s_pending_utf8.length())
        {
            const char c = s_pending_utf8.c_str()[++s_pending_head];
            return uint8_t(c);
        }
        s_pending_utf8.clear();
        s_pending_head = 0;
    }

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
                s_pending_utf8.set("\xbf\xbd");
                s_pending_head = 0;
                return uint8_t(0xef);
            }
            ++available;
        }
        if (!to_utf8(tmp, available, s_pending_utf8) || s_pending_utf8.empty())
            return -1;
        const char c = s_pending_utf8.c_str()[0];
        if (s_pending_utf8.length() > 1)
        {
            s_pending_head = 1;
        }
        else
        {
            s_pending_utf8.clear();
            s_pending_head = 0;
        }
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
