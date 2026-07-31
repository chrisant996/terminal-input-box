# Terminal Input Box

Provides a library for managing terminal input inside a box-like area.

Project title:      Terminal Input Box
Repository name:    terminal-input-box
Colloquial name:    `tib`

## Goals

- Customizable key bindings.
    - Allow "overlaying" a set of key bindings.
- Bindable commands.
    - Basic set of bindable commands for input editing.
    - Extensibility framework to allow registering custom bindable commands.
- History integration through a history provider interface.
    - Default to no history integration.
    - A built-in optional simple in-memory history provider can be used.
    - Or the host can provide its own custom history provider at runtime.
- Completion integration through a completion provider interface.
    - Default to no completion integration.
- Input coloring.
    - Optional interface for host to control input coloring.
    - Default to no input coloring.
- Hookable events.
    - Enough hooks that a caller can manage optional contextual regions, such 
      as a clickable toolbar, or a hint row, or etc.
- Optional interface for host to provide input.
    - Default to using built-in input driver for native VT input.
    - Optional interface for the host to provide its own input driver.
- Region.
    - Let the caller specify a screen region, and whether the region can grow 
      vertically as needed (and min and max heights).
    - Optional border around the input box (imagine line drawing characters, 
      or block graphics like Codex uses, or custom-defined borders).
    - Support for irregular shape on the first line (e.g. prompt on the left 
      and right-prompt on the right).
- Platform abstraction.
    - Abstract platform-dependent functionality behind an interface, and only
      provide a built-in lowest-common-denominator implementation.
- Config options.
    - Word wrapping.
    - Horizontal scrolling.
    - Vertical scrolling.
- Max input size?

## Internal encoding

- Use UTF8 internally.
- Clink and List-Redux ultimately want a limit expressed in WCHAR, but that 
  conflicts with using UTF8 internally.
- `wchar_t` in gcc/Linux is UTF32, but `wchar_t` in MSVC is UTF16.

- [ ] OPEN:  Templated so the host can choose `char` (UTF8) or Windows 
  `wchar_t` (UTF16), or just always use UTF8 and leave any UTF16 support to be
  post-processing by the caller?

## Data Structures

- Use a flat sorted line of key binding sequence strings.  I'm not a fan of 
  using a trie for managing key bindings.  The performance benefits are 
  negligible and require complex traversal implementations to successfully use 
  the trie.
- Each tib has a separate class instance associated with it.  The host can 
  choose which tib is active, and will call appropriate input and dispatch 
  functions to provide an input loop.  Or optionally call a generic 
  encapsulated input loop implementation.
- Minimal "repaints" are important.  Maintain a cache of what's been 
  displayed, and when updating the display skip displaying any line that's the 
  same as what's already displayed.  And a minimal sub-line to display, by 
  clipping leading/trailing graphemes that match what's already displayed.
  IMPORTANT:  Clip based on grapheme boundaries, NOT on byte or character or
  encoding boundaries!

# NOTES

- The flat list of key bindings is already a problem.  I overlooked the fact
  that it doesn't always have the full sequence to match at first.  So, that
  forces the design back to a trie.  But I still want a flat list as the central
  source of truth -- the trie can be re-generated on demand after changes to the
  key table.

- I'm considering starting from the List-Redux el cheapo input routine as a
  starting point, and refactor that into a class with an input driver and so on.
