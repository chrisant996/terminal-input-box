// Copyright (c) 2026 Christopher Antos
// Portions Copyright (c) 2016 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "tib_base.h"
#include "str_iter.h"

template <>
char32_t str_iter_impl<char>::next()
{
    if (!more())
        return 0;

    // TODO:  Detect invalid UTF8 correctly.

    int32_t ax = 0;
    int32_t encode_length = 0;
    do
    {
        const int32_t c = uint8_t(*m_ptr++);
        ax = (ax << 6) | (c & 0x7f);
        if (encode_length)
        {
            --encode_length;
        }
        else
        {
            if ((c & 0xc0) < 0xc0)
                return ax;

            if (encode_length = !!(c & 0x20))
                encode_length += !!(c & 0x10);

            ax &= (0x1f >> encode_length);
        }
    }
    while (more());

    return 0;
}

template <>
size_t str_iter_impl<char>::length() const
{
    return size_t((m_ptr <= m_end) ? m_end - m_ptr : strlen(m_ptr));
}

#ifdef _WIN32
template <>
char32_t str_iter_impl<WCHAR>::next()
{
    int32_t c;
    int32_t ax = 0;

    while (more() && (c = *m_ptr++))
    {
        // Decode surrogate pairs.
        if ((c & 0xfc00) == 0xd800)
        {
            if (!more() || (*m_ptr & 0xfc00) != 0xdc00)         // Invalid.
                return 0xfffd;
            ax = c << 10;
            continue;
        }
        else if ((c & 0xfc00) == 0xdc00)
        {
            if (ax < (1 << 10))                                 // Invalid.
                return 0xfffd;
            c = ax + c - 0x35fdc00;
            ax = 0;
        }
        else
        {
            if (ax)                                             // Invalid.
               return 0xfffd;
        }
        return c;
    }

    if (ax)                                                     // Invalid.
        return 0xfffd;
    return 0;
}
#endif

template <>
size_t str_iter_impl<wchar_t>::length() const
{
    return size_t((m_ptr <= m_end) ? m_end - m_ptr : wcslen(m_ptr));
}
