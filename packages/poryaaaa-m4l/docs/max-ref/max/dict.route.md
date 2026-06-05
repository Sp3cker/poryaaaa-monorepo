# dict.route

_max · Dictionary_

> Compare dictionaries

Use the dict.route object to compare two dictionaries. If the dictionary received at the left inlet meets the specifications set by the dictionary at the right inlet, then pass the dictionary through the left outlet. Otherwise passes the dictionary through the right outlet.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary input |
| in1 | dictionary input |
| out0 | dictionary matching specified keys/values |
| out1 | dictionary not-matching specified keys/values |

## Arguments

- **default-values** (`list`) _(optional)_ — Dictionary syntax composing a dictionary to be compared with the object's input
  Dictionary syntax composing a dictionary to be compared with the object's input. See Dictionaries for more information on dictionary syntax. This dictionary will be replaced by any dictionaries received at the object's right inlet.

## Messages

- `dictionary(name: symbol)` — Dictionary in the first inlet is compared with the reference. Dictionary in the second inlet sets the reference.

## Help patcher examples

### basic

```
Example — [dict.route]
  fan-in:
    in0 ← [dict.pack frog:] ← [number]
    in1 ← [dict.pack frog:4] ← [loadbang]    # In this example a dict will be passed when it has a 'frog' key with a value of 4
  fan-out:
    out0 → [button]:in0
    out0 → [dict.view]:in0
    out1 → [dict.view]:in0
    out1 → [button]:in0
```

## See also

`dictionaries`, `dict.compare`, `dict.deserialize`, `dict.group`, `dict.iter`, `dict.join`, `dict.pack`, `dict.print`, `dict.serialize`, `dict.slice`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
