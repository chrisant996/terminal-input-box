// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#define NOMINMAX
#define VC_EXTRALEAN

#include <cstdint>
#include <vector>

namespace tib {

constexpr size_t c_auto_length = size_t(-1);

#undef min
template <class A> A min(A a, A b) { return (a < b) ? a : b; }

#undef max
template <class A> A max(A a, A b) { return (a > b) ? a : b; }

#undef clamp
template <class A> A clamp(A v, A m, A M) { return min(max(v, m), M); }

size_t resolve_auto_length(size_t len, const char* s);

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
    bool                append(const T* s, size_t len=c_auto_length);
    bool                reserve(size_t len);
    void                clear();
    void                free();

    size_t              length() const { return m_len; }
    const T*            c_str() const { return m_text ? m_text : ""; }

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
    free(m_text);
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
bool cstring_t<T>::reserve(size_t len)
{
    ++len;
    if (len >= m_capacity)
    {
        char* tmp = static_cast<T*>(realloc(m_text, len));
        if (!tmp)
            return false;
        m_text = tmp;
        m_text[m_len] = 0;
        m_capacity = len;
    }
    return true;
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

}
