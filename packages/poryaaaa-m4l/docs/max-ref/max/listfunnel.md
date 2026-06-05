# listfunnel

_max · Lists_

> Index and output list elements

Outputs the elements of an incoming list in the format:

 [index] [element]

 for each element of the list.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | list in, Elements Will Be Output Preceded by Their Index |
| out0 | list, (Index, Element) |

## Arguments

- **offset** (`int`) _(optional)_ — Initial offset
  An integer argument is used to specify an offset for the first index value. If no argument is present, the list elements are numbered beginning with the default index of 0.

## Messages

- `int(input: int)` — Output indexed data
  The low index value and the received number are sent out as a two-element list.
- `float(input: float)` — Output indexed data
  The low index value and the received number are sent out as a two-element list.
- `list(input: list)` — Output indexed data
  Each element of the list is indexed and this index is prepended to the list element and sent out the outlet as a two-element list. The input list may contain ints, floats, and symbols (provided that the first element of the list is not a symbol).
- `anything(input: list)` — Output indexed data
  Performs the same as list.
- `offset(offset: int)` — Set index offset
  The word offset followed by an integer argument is used to specify an offset for the first index value.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `x` — seen as: `x y z`
- `yola` — seen as: `yola 3.3 2. 8.333 bob`

## Help patcher examples

### basic

```
Example #1 — [listfunnel 74]
  fan-in:
    in0 ← [message "x y z"]
    in0 ← [message "10 20 30 40 50"]
  fan-out:
    out0 → [print @popup 1]:in0
```

```
Example #2 — [listfunnel]
  fan-in:
    in0 ← [message "yola 3.3 2. 8.333 bob"]
  fan-out:
    out0 → [print @popup 1]:in0
```

## See also

`funnel`, `spray`
