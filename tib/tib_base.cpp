// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et sw=4 cino={0s:

#include "pch.h"
#include "tib_base.h"
#include <assert.h>

namespace tib {

cstring::cstring(cstring&& s)
{
    m_len = 0;
    m_text = nullptr;   // UGLY.
    *this = std::move(s);
}

cstring& cstring::operator=(const cstring& s)
{
    assert(m_text);     // Don't use a cstring after std::move from it.

    set(s.m_text, s.m_len);
    return *this;
}

// Side effect:  swaps *this and s!
cstring& cstring::operator=(cstring&& s)
{
    assert(m_text);     // Don't use a cstring after std::move from it.

    size_t l = m_len;
    m_len = s.m_len;
    s.m_len = l;

    char* p = m_text;
    m_text = s.m_text;
    s.m_text = p;
    return *this;
}

bool cstring::operator==(const cstring& s) const
{
    assert(m_text);     // Don't use a cstring after std::move from it.
    assert(s.m_text);   // Don't use a cstring after std::move from it.
    return (m_len == s.m_len) && (memcmp(m_text, s.m_text, m_len) == 0);
}

void cstring::set(const char* s, size_t len)
{
    assert(m_text);     // Don't use a cstring after std::move from it.

    free(m_text);
    raw_set(s, len);
}

void cstring::raw_set(const char* s, size_t len)
{
    m_len = (len == c_auto_length) ? strlen(s) : len;
    m_text = static_cast<char*>(malloc(m_len + 1));

    memcpy(m_text, s, m_len);
    m_text[m_len] = 0;
}

}
