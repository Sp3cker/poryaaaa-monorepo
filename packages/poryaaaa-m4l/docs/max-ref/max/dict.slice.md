# dict.slice

_max · Dictionary_

> Split a dictionary into two dictionaries

Use the dict.slice object to split a dictionary into two dictionaries. The first dictionary will be created from a set of keys you provide, sent to the left outlet. The remaining dictionary content will form the second dictionary, sent to the right outlet.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary input |
| out0 | dictionary output sliced of specified keys |
| out1 | series of key/value pairs sliced from the dictionary |

## Messages

- `dictionary(name: symbol)` — Name of a dictionary to be split into two dictionaries

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)

## Help patcher examples

### basic

```
Example — [dict.slice @keys dog cat]
  fan-in:
    in0 ← [dict.pack cow: moo lion: roar 5 times cat: meow dog:woof noah: 1 @name ark] ← [button]
  fan-out:
    out0 → [dict.iter]:in0
    out1 → [dict.iter]:in0
```

## See also

`dictionaries`, `dict.compare`, `dict.deserialize`, `dict.group`, `dict.iter`, `dict.join`, `dict.pack`, `dict.print`, `dict.route`, `dict.serialize`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
