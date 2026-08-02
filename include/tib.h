// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include "tib_base.h"
#include "tib_bindings.h"
#include "tib_context.h"
#include "tib_dispatch.h"
#include "tib_input.h"
#include "tib_output.h"

namespace tib {

class input_box
{
public:
                        ~input_box() = default;
                        input_box() = default;

private:
	const key_table*    m_bindings = nullptr;
};

}
