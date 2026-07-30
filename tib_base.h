// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et sw=4 cino={0s:

#pragma once

#include <stdint.h>

class cstring
{
public:
                        ~cstring();
                        cstring() = delete;
                        cstring(const char* s, uint16_t len=-1);
                        cstring(const cstring& s);
                        cstring(cstring&& s);
                        cstring& operator=(const cstring& s);
                        cstring& operator=(cstring&& s);

    void                set(const char* s, uint16_t len=-1);
    uint16_t            length() const { return m_len; }
    const char*         c_str() const { return m_text; }

private:
    uint16_t            m_len;
    const char*         m_text;
};
