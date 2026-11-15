# Terminal Input Box

Provides a library for managing terminal input inside a box-like area.

Project title:      Terminal Input Box
Repository name:    terminal-input-box
Colloquial name:    `tib`

Do not use word-wrapping in this document; it's unnecessary because the human reads the document in a GUI that applies word-wrapping at render time to match the width of the GUI window.

## Goals

- [x] Customizable key bindings.
    - [x] Allow "overlaying" a set of key bindings.
- [x] Bindable commands.
    - [x] Basic set of bindable commands for input editing.
    - [x] Extensibility framework to allow registering custom bindable commands.
- [ ] History integration through a history provider interface.
    - [ ] Default to no history integration.
    - [ ] A built-in optional simple in-memory history provider can be used.
    - [ ] Or the host can provide its own custom history provider at runtime.
- [x] **(LATER?)** Completion integration through a completion provider interface.  This can be delegated to the host, for now.
    - [x] **(LATER?)** Default to no completion integration.  This can be delegated to the host, for now.
- [x] Input coloring.
    - [x] Optional interface for host to control input coloring.
    - [x] Default to no input coloring.
- Hookable events.
    - Enough hooks that a caller can manage optional contextual regions, such as a clickable toolbar, or a hint row, or etc.
    - [x] **(HOST)** Allow optional contextual regions outside the border.  This can be delegated to the host; the `binding_resolver` enables the host to handle mouse input that the `editor_context` doesn't.
    - [x] **(HOST)** Allow optional contextual regions inside the border?  This can be delegated to the host; the `binding_resolver` enables the host to handle mouse input that the `editor_context` doesn't.
- [x] Optional interface for host to provide input.
    - [x] Default to using built-in input driver for native VT input.
    - [x] Optional interface for the host to provide its own input driver.
    - [x] `term_out_hook` callback for host to handle output.
    - [x] `term_in_hook` callback for host to provide input.
- [x] Region.
    - [x] Let the caller specify a screen region, and whether the region can grow vertically as needed (and min and max heights).
    - [x] Optional border around the input box (imagine line drawing characters, or block graphics like Codex uses, or custom-defined borders).
    - [x] Support for irregular shape on the first line (e.g. prompt on the left and right-prompt on the right).
- Display.
    - [x] Custom border drawing.
    - [x] **(HOST)** Custom prompt drawing.  This can be delegated to the host; providing `set_left_text()` is sufficient for the host to handle the rest of the prompt display.
    - [x] **(HOST)** Custom prompt drawing, optionally within border.  This can be delegated to the host, via `border_definition` customization.
    - [x] Custom padding inside border.
    - [x] Relative cursor positioning, esp. for multi-line display.
- [x] Platform abstraction.
    - [x] Abstract platform-dependent functionality behind an interface, and only provide a built-in lowest-common-denominator implementation.
- Config options.
    - [x] Word wrapping.
    - [x] Multi-line.
    - [x] Horizontal scrolling.
    - [x] Vertical scrolling.
    - [x] Max width.
    - [x] Max height.
    - [x] Variable height (grow up to max height).
- [x] Max input size.
- [ ] Bracketed paste:  ON: `\x1b[?2004h`, OFF: `\x1b[?2004l`, BEGIN: `\x1b[200~`, END: `\x1b[201~`.

## The Host Must Always Control The Input Loop

Readline likes to go into modal dispatch loops.  That design choice creates problems for some hosts.  When tib wants a modal dispatch loop, it needs to find a stateful way to achieve the desired functional behaviors without actually using a modal dispatch loop.

`digit_argument` and `editor_context::apply_argument_bindings` show how to accomplish the effect of a modal dispatch loop without actually using a modal dispatch loop.

## Binding stack

Each noncopyable `dispatcher_target` is owned by a `shared_ptr`.  Each registration supplies one explicit `key_table_list` and one `binding_policy`.  `add_bindings(tables, policy)` retains the supplied list as-is; `add_bindings(table, policy)` wraps a single non-null table in a new list.  The supplier owns the list's contents and may add, remove, reorder, or replace tables without changing the registration or its policy.  Later tables within a list have priority for equal-length matches, and all tables within the list participate in longest-match resolution.  An empty list is allowed and retains its policy boundary.

Both overloads return a value-type `binding_handle` which the caller may discard without removing the bindings.  `handle.remove()` explicitly removes that registration without reordering the others and is harmless if already expired.  Registering the same table or list twice creates distinct registrations.  A handle is only a weak reference; it does not own the target or tables.  Default-constructed, moved-from, and removed handles evaluate to false; copies identify the same registration and observe removal through any copy.  `get_bindings()` exposes const group records for inspection.  Vector mutations can invalidate references and iterators to those records, while copied handles remain usable.

Each group's policy describes its relationship to lower registrations:

- `binding_policy::overlay` searches the list together with lower groups.  Longest-match resolution spans those groups, with later-group priority for equal-length matches, until a fallback or exclusive policy determines whether to continue.
- `binding_policy::fallback` searches below only after a miss.  A viable prefix, complete binding, or self-insert candidate prevents fallback.  On a miss the accumulated sequence is retried against the next group.
- `binding_policy::exclusive` never searches below, even on a miss.  This only excludes lower registrations on that target; it does not exclude other dispatcher targets.

The policy belongs to the registered list, so removing its first table does not remove a boundary or change which registration receives a fallback notification.  A boundary with no lower groups has nowhere to fall back.  Fallback does not remove registrations automatically.  Each crossed boundary is remembered by its handle while the input sequence is pending, and `on_binding_fallback(handle)` is called in search order when dispatch commits the result, including a final miss.  Probing never invokes callbacks.  Pending notifications retain neither targets nor lists.  Removed registrations are skipped, including removal by an earlier callback.  Re-registering the same list creates a new identity and cannot revive a notification for a removed registration.  Discarding a handle preserves notification; removing its registration cancels it.  An owner can end its mode by calling `handle.remove()`, or leave it active to keep allowing fallthrough.  `editor_context` handles its numeric-argument group itself and forwards host groups to `editor_callbacks::on_binding_fallback(ctx, handle)`.

Numeric-argument bindings use a fallback group; switching argument tables removes the old registration and adds the appropriate table through the public API.  Numeric argument mode does not support pushing other modes above it and then switching its tables beneath them.  Parameter requests must supply a nonempty vector of non-null shared key tables so the caller cannot forget to provide bindings to accept the parameter.  The editor builds one exclusive list containing the built-in parameter-editing table followed by the request's tables in vector order; later tables have priority.  The vector supplies additional tables rather than a separately registered group.  Hosts can append sub-mode groups with any policy while a request is active.  Ending the request removes only its own group through its handle.  Other registrations remain active regardless of when they were added, and their suppliers are responsible for removing them.  Outstanding copies of the request group's handle become expired.

To supply a group of overlaid tables, put them in one `key_table_list` and register the list with the desired policy.  Use the single-table overload when only one table is needed.  To supersede outer bindings, register the new list or table using `exclusive`.

## Internal encoding

- Use UTF8 internally.
- Clink and List-Redux ultimately want a limit expressed in WCHAR, but that conflicts with using UTF8 internally.
  - `wchar_t` in gcc/Linux is UTF32, but `wchar_t` in MSVC is UTF16.
  - Is it worth being templated so the host can choose `char` (UTF8) or Windows `wchar_t` (UTF16), or just always use UTF8 and leave any UTF16 support to be post-processing by the caller?  _[NO:  I don't want to maintain multiple access models or underlying algorithms; UTF8 is more compact and there is little enough encoding translation required that if a host really needs UTF16 then they can perform additional translations.]_

## Data Structures

- Use a flat sorted line of key binding sequence strings.  I'm not a fan of using a trie for managing key bindings.  The performance benefits are negligible and require complex traversal implementations to successfully use the trie.  _[YES:  perf testing shows a trie is massive overkill.]_
  - _[Also, this was crucial for supporting mouse input bindings.]_
- Each tib has a separate class instance associated with it.  The host can choose which tib is active, and will call appropriate input and dispatch functions to provide an input loop.  Or optionally call a generic encapsulated input loop implementation.
- Minimal "repaints" are important.  Maintain a cache of what's been displayed, and when updating the display skip displaying any line that's the same as what's already displayed.  And a minimal sub-line to display, by clipping leading/trailing graphemes that match what's already displayed.  IMPORTANT:  Clip based on grapheme boundaries, NOT on byte or character or encoding boundaries!

# NOTES

- The flat list of key bindings is already a problem.  I overlooked the fact that it doesn't always have the full sequence to match at first.  So, that forces the design back to a trie.  But I still want a flat list as the central source of truth -- the trie can be re-generated on demand after changes to the key table.  _[NO:  overlay tables complicate the tries anyway, and empirical testing on an Alienware m16 R2 Intel Core Ultra 9 185H 2.30 GHz resolves 26,000 key sequences in ~165ms **without** a trie, so a trie is overkill.]_

- I'm considering starting from the List-Redux el cheapo input routine as a starting point, and refactor that into a class with an input driver and so on.  _[That's what I did.]_
