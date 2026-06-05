# match

_max · Control_

> Watch for a message match, then output the message

Watches an incoming stream of ints, floats, symbols, lists, or messages, and outputs the stream after it has met the specification of its arguments.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Input Sequence to be Matched |
| out0 | list of Matched Sequence |

## Arguments

- **match-list** (`list`) — The match list
  The arguments specify numbers to look for, in the proper order. The word nn can be used as a wild card that will match any number.

## Messages

- `int(input: int)` — Compare to argument list
  If the numbers match the arguments, in the proper order, they are sent out as a list.
- `float(input: float)` — Compare to argument list
  If the numbers match the arguments, in the proper order, they are sent out as a list.
- `list(input: list)` — Compare to argument list
  If the input-list matches the arguments, in the proper order, they are sent out as a list.
- `anything(input: list)` — Compare to argument list
  Performs the same as list.
- `clear` — Ignore previously received values
  Causes match to forget all numbers it has received up to that time.
- `set(match-list: list)` — Set a new match list
  The word set, followed by a list of numbers, specifies a new series of numbers match will look for.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `erg` — seen as: `erg 88`

## Help patcher examples

### basic

> You can change what you're trying to match with the set message. This clears what has been seen thus far.

```
Example #1 — [match 34]
  fan-in:
    in0 ← [message "erg 88"]
    in0 ← [message "34"]
    in0 ← [message "set 34 erg 88"]
  fan-out:
    out0 → [print match @popup 1]:in0    # Click on the boxes in order from left to right
```

```
Example #2 — [match 1 2 3 4]
  fan-in:
    in0 ← [message "1"]
    in0 ← [message "4"]    # click these
    in0 ← [message "3"]
    in0 ← [message "2"]
  fan-out:
    out0 → [print exact @popup 1]:in0
```

```
Example #3 — [match 1 2 nn 4]
  fan-in:
    in0 ← [message "1, 2, $1, 4"]
  fan-out:
    out0 → [print wildcard @popup 1]:in0    # use "nn" as a wildcard
```

## See also

`iter`, `join`, `pack`, `select`
