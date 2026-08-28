# Issues

# TODO

- Quoted insert.  Needs an internal mode to force self-insert of next char.
- Commands:
  - CUA versions of `screen-line-down` and `screen-line-up`.
  - Automatic clearing of selection when a non-CUA command is dispatched.
  - Transpose characters.
  - Transpose words.
  - UPPER CASE, lower case, Capitalize.
  - Toggle-able overwrite mode.
  - Undo all changes to the input line (undo back to the original state).
  - Reset the input to empty (as an undo-able operation, unlike the command to undo all changes).
  - Bigword commands that only treat whitespace as word breaks.
- Numeric arguments.
- Record or play back keyboard macros.
- Some way to automagically treat upper case keys the same as their lower case equivalent?  Not sure that even makes any sense without a trie.  Maybe it should be handled by default bindings for Alt-UpperLetter keys to signal reevaluating the binding with LowerLetter for the last byte?
- Does this need the concepts of `point` and `mark`?
- Make sure a host (e.g. example.exe) can provide a command/mode to read a command name and invoke it.

## History Provider

- How to integrate with a history provider?  Or is that something that can be delegated to the host?
- Maybe provide a simple in-memory history list, but without history searching or filtering or etc (delegate that to the host).
