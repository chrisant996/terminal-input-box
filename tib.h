// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et sw=4 cino={0s:

#pragma once

#include "tib_base.h"
#include "tib_bindings.h"

namespace tib {

class tib
{
public:
                        ~tib() = default;
                        tib() = default;

private:
	const key_table*    m_bindings = nullptr;
};

}
