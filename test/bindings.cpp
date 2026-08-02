// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "test.h"
#include "tib.h"

TEST_CASE("Key bindings")
{
    SECTION("Main")
    {
        // TODO:  Implement some tests for key bindings.
        // - It's sufficient for the binding_target's to all use the same actual function pointer, and verification can rely only on the command name to differentiate between them.
        // - Create a key_table with several VT key sequences.
        // - Add it to a key_table_list.
        // - Verify stepping and resolving some bindings -- ones that match, ones that don't, ones that have a common prefix but later don't match.
        // - Create another key_table that overrides some key sequences in common with the first key_table.
        // - Add it to a key_table_list.
        // - Verify that sequences overridden by the second key_table match the second key_table, not the first one.
    }
}

