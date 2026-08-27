# Terminal Input Box

Provides a library for managing terminal input inside a box-like area.

Project title:      Terminal Input Box
Repository name:    terminal-input-box
Colloquial name:    `tib`

Do not use word-wrapping in this document; it's unnecessary because the human reads the document in a GUI that applies word-wrapping at render time to match the width of the GUI window.

## Goals

- [x] Customizable key bindings.
    - [x] Allow "overlaying" a set of key bindings.
- Bindable commands.
    - Basic set of bindable commands for input editing.
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

## Internal encoding

- Use UTF8 internally.
- Clink and List-Redux ultimately want a limit expressed in WCHAR, but that conflicts with using UTF8 internally.
- `wchar_t` in gcc/Linux is UTF32, but `wchar_t` in MSVC is UTF16.

- [ ] OPEN ISSUE:  Templated so the host can choose `char` (UTF8) or Windows `wchar_t` (UTF16), or just always use UTF8 and leave any UTF16 support to be post-processing by the caller?

## Data Structures

- Use a flat sorted line of key binding sequence strings.  I'm not a fan of using a trie for managing key bindings.  The performance benefits are negligible and require complex traversal implementations to successfully use the trie.  _[YES:  perf testing shows a trie is massive overkill.]_
  - _[Also, this was crucial for supporting mouse input bindings.]_
- Each tib has a separate class instance associated with it.  The host can choose which tib is active, and will call appropriate input and dispatch functions to provide an input loop.  Or optionally call a generic encapsulated input loop implementation.
- Minimal "repaints" are important.  Maintain a cache of what's been displayed, and when updating the display skip displaying any line that's the same as what's already displayed.  And a minimal sub-line to display, by clipping leading/trailing graphemes that match what's already displayed.  IMPORTANT:  Clip based on grapheme boundaries, NOT on byte or character or encoding boundaries!

# NOTES

- The flat list of key bindings is already a problem.  I overlooked the fact that it doesn't always have the full sequence to match at first.  So, that forces the design back to a trie.  But I still want a flat list as the central source of truth -- the trie can be re-generated on demand after changes to the key table.  _[NO:  overlay tables complicate the tries anyway, and empirical testing on an Alienware m16 R2 Intel Core Ultra 9 185H 2.30 GHz resolves 26,000 key sequences in ~165ms **without** a trie, so a trie is overkill.]_

- I'm considering starting from the List-Redux el cheapo input routine as a starting point, and refactor that into a class with an input driver and so on.
