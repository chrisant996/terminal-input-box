// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#pragma once

#include "tib_base.h"
#include "tib_bindings.h"

static bool add_binding(tib::key_table& table, const char* sequence, const char* name)
{
    return table.add({ sequence, tib::binding_target_func(name) });
}

class dispatcher_tester : public tib::dispatcher_target
{
public:
                        ~dispatcher_tester() = default;
                        dispatcher_tester() = default;
    int32_t             dispatch(const tib::cstring &sequence, int32_t key, const tib::binding_target* binding, const tib::binding_params* params) noexcept { ++m_dispatch_count; return -1; }
    uint32_t            get_dispatch_count() const noexcept { return m_dispatch_count; }

private:
    uint32_t            m_dispatch_count = 0;
};
