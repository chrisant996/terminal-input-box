# Issues

# TODO

- Commands:
  - Reset the input to empty (as an undo-able operation, unlike the command to undo all changes).  What side effects should occur...?  How should it integrate with a history provider?
- Some way to automagically treat upper case keys the same as their lower case equivalent?  Not sure that even makes any sense without a trie.  Maybe it should be handled by default bindings for Alt-UpperLetter keys to signal reevaluating the binding with LowerLetter for the last byte?

## Open Questions

- Does this need the concepts of `point` and `mark`?
- Does this need support for numeric arguments?  Probably yes.  But it might be reasonable to abstain from providing commands for entering a numeric argument, and instead only provide an API such that a host (such as Clink) could implement commands for entering a numeric argument.  As long as the built-in commands respect the numeric argument similarly to how Readline does, then that could be a sufficient level of support built into the library.

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

## Editor overwrite mode

> The shape of this looks very nice.  What happens if the cursor is moved while
> the overwrite accumulator is not empty, and then additional overwrite input
> occurs?  What happens if undo occurs while the overwrite accumulator is not
> empty, and then additional overwrite input occurs?

Currently:

- If the cursor moves to a different position, the stored caret no longer matches, so the next overwrite byte discards the accumulator and starts a new overwrite unit at the new caret.
- If the cursor moves away and then returns to the stored position, the movement is not detected. The next byte continues the old accumulator, restores its original baseline, and merges into its undo record. That is undesirable.
- Undo increments `m_change_counter`, so subsequent input detects the mismatch, discards the accumulator, and starts fresh from the undone state. A continuation byte after undo is therefore treated as standalone invalid UTF-8, which is appropriate because undo cancelled its lead bytes.
- Undo followed by redo also invalidates the accumulator because both change the counter.

So undo is safe, but caret movement has a round-trip hole. The proper refinement is to explicitly clear the accumulator whenever an operation interrupts self-insertion—caret/selection movement, deletion, undo/redo, transpose, etc.—rather than infer continuity solely from caret and change-counter equality. That also makes undo behavior intentional instead of incidental.
