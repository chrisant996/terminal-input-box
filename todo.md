# Issues

# TODO

- Commands:
  - Toggle-able overwrite mode.
  - Reset the input to empty (as an undo-able operation, unlike the command to undo all changes).  What side effects should occur...?  How should it integrate with a history provider?
  - Bigword commands that only treat whitespace as word breaks.
- Some way to automagically treat upper case keys the same as their lower case equivalent?  Not sure that even makes any sense without a trie.  Maybe it should be handled by default bindings for Alt-UpperLetter keys to signal reevaluating the binding with LowerLetter for the last byte?

## Open Questions

- Does this need the concepts of `point` and `mark`?
- Does this need support for numeric arguments?  Probably yes.  But it might be reasonable to abstain from providing commands for entering a numeric argument, and instead only provide an API such that a host (such as Clink) could implement commands for entering a numeric argument.  As long as the built-in commands respect the numeric argument similarly to how Readline does, then that could be a sufficient level of support built into the library.

## Sufficiency

- [ ] Make sure a host (e.g. example.exe) can provide a command/mode to read a command name and invoke it.
- [ ] Enough built-in capabilities for a host to support recording keyboard macros.
- [x] Make sure the host has enough influence to easily allow some hooks:
  - _before dispatch_ (can probably be handled by `dispatcher_target::dispatch` callback)
  - _after dispatch_ (can probably be handled by `dispatcher_target::dispatch` callback)
  - _on input line changed_ (can probably be handled by `dispatcher_target::dispatch` callback)

## History Provider

- How to integrate with a history provider?  Or is that something that can be delegated to the host?
- Maybe provide a simple in-memory history list, but without history searching or filtering or etc (delegate that to the host).
