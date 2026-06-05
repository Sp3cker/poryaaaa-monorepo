# dict.join

_max · Dictionary_

> Merge the content of two dictionaries

Use the dict.join object to merge the content of two dictionaries together into a single dictionary.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary to combined with dictionary at right inlet |
| in1 | dictionary to combined with dictionary at left inlet |
| out0 | dictionary of entries combined from both inlets |

## Arguments

- **default-values** (`list`) _(optional)_ — Dictionary syntax composing a dictionary to be joined with the object's input
  Dictionary syntax composing a dictionary to be joined with the object's input. See Dictionaries for more information on dictionary syntax. This dictionary will be replaced by any dictionaries received at the object's right inlet.

## Messages

- `bang` — Resend the most recently combined dictionary
- `array(ARG_NAME_0: symbol)` — TEXT_HERE
- `dictionary(name: symbol)` — Dictionary from the second inlet is combined with the dictionary from the first inlet and a new dictionary is sent

## Help patcher examples

### advanced

> if two dictionaries are joined, and both contain the same key, the key in the dictionary being joined (right inlet) overrides the key for the dictionary input (left inlet)

```
Example — [dict.join]
  fan-in:
    in0 ← [dict.pack brick : 2 wheat : 1 sheep : 3] ← [button]
    in1 ← [dict.pack brick : 5 ore : 4] ← [button]    # conflicts
  fan-out:
    out0 → [dict.iter]:in0
```

### basic

> optional arguments use dictionary syntax to define the dictionary being joined with the input at the left inlet. input received at the right inlet will override this initial dictionary.

```
Example — [dict.join frog: pig: horse: duck: "quack quack"]
  fan-in:
    in0 ← [dict.pack cow: moo lion: roar 5 times noah: 1 @name ark] ← [button]
  fan-out:
    out0 → [dict.iter]:in0
```

## See also

`dictionaries`, `dict.compare`, `dict.deserialize`, `dict.group`, `dict.iter`, `dict.pack`, `dict.print`, `dict.route`, `dict.serialize`, `dict.slice`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
