// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "tib_base.h"
#include "tib_bindings.h"
#include "tib_terminal.h"
#include "test_util.h"
#include <assert.h>

static tib::terminal_in* new_test_terminal_in(tib::pushed_input& pushed)
{
    return new test_terminal_in(pushed);
}

test_terminal_in* test_terminal_in::s_instance = nullptr;

test_terminal_in::test_terminal_in(tib::pushed_input& pushed)
{
    assert(!s_instance);
    s_instance = this;
}

test_terminal_in::~test_terminal_in()
{
    assert(s_instance == this);
    s_instance = nullptr;
}

int32_t test_terminal_in::read() noexcept
{
    if (empty())
        return -1;
    return uint8_t(m_input.c_str()[m_index++]);
}

bool test_terminal_in::avail(uint32_t timeout) noexcept
{
    return !empty();
}

void test_terminal_in::set_input(const char* input, size_t len)
{
    assert(empty());
    m_input.set(input, len);
    m_index = 0;
}

void test_terminal_in::clear() noexcept
{
    m_input.clear();
    m_index = 0;
}

test_terminal_in* test_terminal_in::get() noexcept
{
    return s_instance;
}

test_input_stream::test_input_stream(const char* input, size_t len)
{
    m_terminal = test_terminal_in::get();
    assert(m_terminal);
    m_terminal->set_input(input, len);
}

test_input_stream::~test_input_stream()
{
    assert(m_terminal == test_terminal_in::get());
    m_terminal->clear();
}

bool test_input_stream::empty() const noexcept
{
    return m_terminal->empty();
}

void install_test_terminal_in()
{
    assert(!tib::hook_new_terminal_in);
    tib::hook_new_terminal_in = new_test_terminal_in;
}

bool add_binding(tib::key_table& table, const char* sequence, const char* name)
{
    return table.add({ sequence, tib::binding_target_func(name) });
}
