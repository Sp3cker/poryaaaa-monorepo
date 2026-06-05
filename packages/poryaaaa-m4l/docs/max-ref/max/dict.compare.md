# dict.compare

__

> Compare two dictionaries for equivalence.

Dictionaries are considered equivalent if they contain the identical keys, and if those keys contain identical data.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary to compare with dictionary at right inlet |
| in1 | dictionary to compare with dictionary at left inlet |
| out0 | 1 if equivalent, 0 if different |

## Messages

- `bang` — Output comparison result
  Repeat the comparison and output a 1 if the dictionaries are equivalent, or a 0 if not.
- `dictionary(name: symbol)` — Send a dictionary for comparison
  In the left inlet, compare to any dictionary previously sent to the right inlet and output the comparison result from the outlet. In the right inlet, set the dictionary for future comparison (without output).

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### unordered

> Normally, arrays in dictionaries are compared for both contents and order. When @unordered is enabled, only the contents are compared, order is ignored.

```
Example — [dict.compare @unordered 1]
  fan-in:
    in0 ← [dict.pack cow: moo lion: roar 5 times noah: 1] ← [button]
    in0 ← [dict.pack cow: moo lion: roar times 5 noah: 1] ← [button]    # @unordered
    in1 ← [dict.pack cow: moo lion: roar 5 times noah: 1] ← [button]
  fan-out:
    out0 → [print compare_unordered @popup 1]:in0
```

### basic

```
Example — [dict.compare]
  fan-in:
    in0 ← [dict.pack cow: moo lion: roar 5 times noah: 1] ← [button]
    in0 ← [dict.pack cow: moo lion: roar times 5 noah: 1] ← [button]
    in1 ← [dict.pack cow: moo lion: roar 5 times noah: 1] ← [button]
  fan-out:
    out0 → [print compare @popup 1]:in0
```

## See also

`dict.deserialize`, `dict.group`, `dict.iter`, `dict.join`, `dict.pack`, `dict.print`, `dict.route`, `dict.serialize`, `dict.slice`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
