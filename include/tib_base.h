// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et sw=4 cino={0s:

#pragma once

#include <cstdint>
#include <vector>

namespace tib {

#undef min
template <class A> A min(A a, A b) { return (a < b) ? a : b; }

#undef max
template <class A> A max(A a, A b) { return (a > b) ? a : b; }

#undef clamp
template <class A> A clamp(A v, A m, A M) { return min(max(v, m), M); }

// A counted string that can optionally contain embedded NUL characters.
//
// Annoying Note:  c_str() has to check for nullptr as part of supporting a
// cstring&& r-value constructor for the class.
class cstring
{
    enum : size_t { c_auto_length = size_t(-1) };

public:
                        ~cstring() { free(m_text); }
                        cstring() = delete;
                        cstring(const char* s, size_t len=c_auto_length) { raw_set(s, len); }
                        cstring(const cstring& s) { raw_set(s.m_text, s.m_len); }
                        cstring(cstring&& s);
    cstring&            operator=(const cstring& s);
    cstring&            operator=(cstring&& s);
    bool                operator==(const cstring& s) const;

    void                set(const char* s, size_t len=c_auto_length);
    size_t              length() const { return m_len; }
    const char*         c_str() const { return m_text ? m_text : ""; }

private:
    void                raw_set(const char* s, size_t len);
    size_t              m_len;
    char*               m_text;
};

}
