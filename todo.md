# Issues

# TODO

- Numeric argument:
  - [x] Begin undo grouping.
  - [ ] Add tests for undo grouping and numeric argument.
  - [ ] Add tests for negative numeric arguments and opposites for maybe 3 representative commands that use `do_with_numeric_argument`.
  - [x] Fix tests that assumed numeric argument and/or quoted insert implementation are within the binding resolver.
  - [x] Negative numeric argument stack exhaustion due to incorrect inversion.
  - [ ] `digit-argument` followed by non-meta digits should behave the same as meta digits.
  - [ ] Bash seems to go into a modal dispatch loop inside `digit-argument`??
  - [ ] Display message for the numeric argument (and add a color for it).
  - [ ] A `universal-argument` command that mimics the documented Readline behavior.
  - [ ] Test whether the r-value ctor for a binding results in RVO and a single cstring ctor, or two cstring ctor invocations...
- Commands:
  - An analog to `Ctrl-G` `abort` in Readline.  It needs to clear all the inputs as well (overwrite, quoted insert, numeric argument, etc).
  - Reset the input to empty (as an undo-able operation, unlike the command to undo all changes).  What side effects should occur...?  How should it integrate with a history provider?
- Some way to automagically treat upper case keys the same as their lower case equivalent?  Not sure that even makes any sense without a trie.  Maybe it should be handled by default bindings for Alt-UpperLetter keys to signal reevaluating the binding with LowerLetter for the last byte?

## Open Questions

- Does this need the concepts of `point` and `mark`?

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
