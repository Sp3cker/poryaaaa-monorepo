# dict.iter

_max · Dictionary_

> Stream the content of a dictionary

Use the dict.iter object to send all key-value pairs out from a dictionary as a series of standard Max list messages.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary input |
| out0 |  |

## Messages

- `array(ARG_NAME_0: symbol)` — TEXT_HERE
- `dictionary(name: symbol)` — Name of the dictionary through which to iterate

## Help patcher examples

### advanced

```
Example #1 — [dict.iter]  if any of the keys are mapped to sub-dictionaries within the dictionary, then those can be further iterated with another dict.iter.
  fan-in:
    in0 ← [route child_0 child_1] ← [dict.iter]    # outputs one level of the dictionary
  fan-out:
    out0 → [print CC]:in0
```

```
Example #2 — [dict.iter]
  fan-in:
    in0 ← [route child_0 child_1] ← [dict.iter]    # outputs one level of the dictionary
  fan-out:
    out0 → [print BB]:in0
```

```
Example #3 — [dict.iter]  outputs one level of the dictionary
  fan-in:
    in0 ← [route child_dict] ← [jit.phys.body]
  fan-out:
    out0 → [print A]:in0
    out0 → [route child_0 child_1]:in0
```

### basic

```
Example — [dict.iter]
  fan-in:
    in0 ← [dict ark] ← [message "import dict_help_ark.json, bang"]
  fan-out:
    out0 → [print unload]:in0    # each key is passed out as the first item of a list, with N values for that key following.
```

## See also

`dictionaries`, `dict.compare`, `dict.deserialize`, `dict.group`, `dict.join`, `dict.pack`, `dict.print`, `dict.route`, `dict.serialize`, `dict.slice`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
