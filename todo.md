# Issues

# TODO

- Numeric argument:
    - [ ] `digit-argument` followed by non-meta digits should behave the same as meta digits.
    - [ ] Bash seems to go into a modal dispatch loop inside `digit-argument`??
    - [ ] A `universal-argument` command that mimics the documented Readline behavior.
- Commands:
    - An analog to `Ctrl-G` `abort` in Readline.  It needs to clear all the inputs as well (overwrite, quoted insert, numeric argument, etc).
    - Reset the input to empty (as an undo-able operation, unlike the command to undo all changes).  What side effects should occur...?  How should it integrate with a history provider?
- Display optimization:
    - Clink does a full parse even when the caret moves; but I'd still like to skip the parse in tib when possible.
    - Unit tests to verify some specific cases for the display optimizations.
    - Any time `m_displayed` is not empty, then new display lines should be compared to the display lines in `m_displayed`, even when `m_change_counter == 0`.

## Open Questions

- Does tib already have the `preserve_window_horiz_scroll_position` from Clink?
- Does this need the concepts of `point` and `mark`?
- Should tib display the message on the first _displayed_ row?  But that's going to have weird side effects, and probably should go on a FUTURE list.
- Some way to automagically treat upper case keys the same as their lower case equivalent?  Not sure that even makes any sense without a trie.  Maybe it should be handled by default bindings for Alt-UpperLetter keys to signal reevaluating the binding with LowerLetter for the last byte?

- CLINK: the numeric argument message in Clink is only drawn on the first row, so if the display is scrolled then the message is not visible.  And if the whole line is reverted, then the place where the message would have been displayed does not get redrawn properly.

## Sufficiency

- [x] Make sure a host (e.g. example.exe) can provide a command/mode to read a command name and invoke it. _[The host can erase the current tib and do whatever they want (including start a new tib) to select a command, and then show the original tib again and run the command (or even run the command and then show the tib).]_
- [x] Enough built-in capabilities for a host to support recording keyboard macros. _[The `dispatcher_target::dispatch` callback has everything that the host needs -- the host can track the reocrding state, and `dispatch` receives the key sequence.]_
- [x] Make sure the host has enough influence to easily allow some hooks:
    - _before dispatch_ (can probably be handled by `dispatcher_target::dispatch` callback)
    - _after dispatch_ (can probably be handled by `dispatcher_target::dispatch` callback)
    - _on input line changed_ (can probably be handled by `dispatcher_target::dispatch` callback)

## History Provider

- How to integrate with a history provider?  Or is that something that can be delegated to the host?
- Maybe provide a simple in-memory history list, but without history searching or filtering or etc (delegate that to the host).

# Work In Progress
