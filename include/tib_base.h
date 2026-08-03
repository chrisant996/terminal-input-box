// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#define NOMINMAX
#define VC_EXTRALEAN

#include <cstdint>
#include <vector>
#include <assert.h>

namespace tib {

constexpr size_t c_auto_length = size_t(-1);

#undef min
template <class A> A min(A a, A b) { return (a < b) ? a : b; }

#undef max
template <class A> A max(A a, A b) { return (a > b) ? a : b; }

#undef clamp
template <class A> A clamp(A v, A m, A M) { return min(max(v, m), M); }

size_t resolve_auto_length(size_t len, const char* s);
#ifdef _WIN32
size_t resolve_auto_length(size_t len, const WCHAR* s);
#endif

// A counted string that can optionally contain embedded NUL characters.
template<class T>
class cstring_t
{
public:
                        ~cstring_t() { ::free(m_text); }
                        cstring_t() = default;
                        cstring_t(const T* s, size_t len=c_auto_length) { raw_set(s, len); }
                        cstring_t(const cstring_t<T>& s) { raw_set(s.m_text, s.m_len); }
                        cstring_t(cstring_t<T>&& s) { *this = std::move(s); }
    cstring_t<T>&       operator=(const cstring_t<T>& s);
    cstring_t<T>&       operator=(cstring_t<T>&& s);
    bool                operator==(const cstring_t& s) const;

    bool                set(const T* s, size_t len=c_auto_length);
    bool                set(const cstring_t<T>& s);
    bool                append(const T* s, size_t len=c_auto_length);
    bool                append_color(const T* sgr_params);
    bool                printf(const T* format, ...);
    bool                printfv(const T* format, va_list args);
    T*                  reserve(size_t len);
    void                set_length(size_t len);
    void                clear();
    void                free();

    bool                empty() const { return !m_len; }
    size_t              length() const { return m_len; }
    size_t              capacity() const { return m_capacity; }
    const T*            c_str() const;

private:
    bool                raw_set(const T* s, size_t len);
    size_t              m_capacity = 0;
    size_t              m_len = 0;
    T*                  m_text = nullptr;
};

template<class T>
cstring_t<T>& cstring_t<T>::operator=(const cstring_t<T>& s)
{
    set(s.m_text, s.m_len);
    return *this;
}

template<class T>
cstring_t<T>& cstring_t<T>::operator=(cstring_t<T>&& s)
{
    ::free(m_text);
    m_len = s.m_len;
    m_text = s.m_text;
    s.m_len = 0;
    s.m_text = nullptr;
    return *this;
}

template<class T>
bool cstring_t<T>::operator==(const cstring_t<T>& s) const
{
    return (m_len == s.m_len) && (memcmp(m_text, s.m_text, m_len) == 0);
}

template<class T>
bool cstring_t<T>::set(const T* s, size_t len)
{
    return raw_set(s, len);
}

template<class T>
bool cstring_t<T>::set(const cstring_t<T>& s)
{
    return raw_set(s.c_str(), s.length());
}

template<class T>
bool cstring_t<T>::append(const T* s, size_t len)
{
    len = resolve_auto_length(len, s);
    const size_t needed = m_len + len + 1;
    if (needed >= m_capacity)
    {
        size_t grow = max<size_t>(64, m_capacity * 2);
        if (needed >= grow)
            grow = needed + (needed / 2);
        if (!reserve(grow))
            return false;
    }
    memcpy(m_text + m_len, s, len);
    m_len += len;
    m_text[m_len] = 0;
    return true;
}

template<class T>
bool cstring_t<T>::append_color(const T* sgr_params)
{
    const size_t orig_len = length();

    if (!append("\x1b[", 2))
    {
nope:
        set_length(orig_len);
        return false;
    }

    if (sgr_params)
        (void)append(sgr_params);   // Intentionally ignore failure.
    if (!append("m"))
        goto nope;

    return true;
}

inline size_t str_len(const char* s) { return strlen(s); }
int __vsnprintf(char* buffer, size_t len, const char* format, va_list args);

#ifdef _WIN32
inline size_t str_len(const WCHAR* s) { return wcslen(s); }
int __vsnprintf(WCHAR* buffer, size_t len, const WCHAR* format, va_list args);
#endif

template<class T>
bool cstring_t<T>::printfv(const T* format, va_list args)
{
    const size_t len = length();
    size_t cap = m_capacity - len;

    int res = -1;
    if (cap > 1)
    {
        errno = 0;
        res = __vsnprintf(m_text + len, cap, format, args);
        if (errno)
        {
            m_text[len] = 0;
            return false;
        }
    }

    if (res < 0)
    {
        cap = std::max<size_t>(cap * 2, 100);
        while (res < 0)
        {
            // Subtract 1 from the max character count to ensure room for null
            // terminator; see MSDN for idiosyncracy of the snprintf family of
            // functions.
            if (!reserve(len + cap))
                return false;
            errno = 0;
            res = __vsnprintf(m_text + len, cap, format, args);
            if (errno)
            {
                m_text[len] = 0;
                return false;
            }
            cap *= 2;
        }
    }

    m_len += res;
    assert(m_len == len + str_len(m_text + len));
    assert(m_len < m_capacity);
    return true;
}

template<class T>
bool cstring_t<T>::printf(const T* format, ...)
{
    va_list args;
    va_start(args, format);
    const bool ret = printfv(format, args);
    va_end(args);
    return ret;
}

template<class T>
T* cstring_t<T>::reserve(size_t len)
{
    ++len;
    if (len >= m_capacity)
    {
        T* tmp = static_cast<T*>(realloc(m_text, len * sizeof(T)));
        if (!tmp)
            return nullptr;
        m_text = tmp;
        m_text[m_len] = 0;
        m_capacity = len;
    }
    return m_text;
}

template<class T>
void cstring_t<T>::set_length(size_t len)
{
    if (!len)
    {
        clear();
    }
    else
    {
        // assert(len <= m_len);
        assert(len < m_capacity);
        m_len = len;
        m_text[m_len] = 0;
    }
}

template<class T>
void cstring_t<T>::clear()
{
    m_len = 0;
    if (m_text && m_capacity)
        m_text[m_len] = 0;
}

template<class T>
void cstring_t<T>::free()
{
    ::free(m_text);
    m_capacity = 0;
    m_len = 0;
    m_text = nullptr;
}

template<class T>
bool cstring_t<T>::raw_set(const T* s, size_t len)
{
    len = resolve_auto_length(len, s);
    if (!reserve(len))
        return false;

    memcpy(m_text, s, len);
    m_len = len;
    m_text[m_len] = 0;
    return true;
}

typedef cstring_t<char> cstring;

} // namespace tib
