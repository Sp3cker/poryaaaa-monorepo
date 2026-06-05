# dict.strip

_max · Dictionary_

> Remove keys from a dictionary

Use the dict.strip object to remove the specified keys from a dictionary and return their values through an outlet. Following this operation, the dictionary will no longer contain the specified keys.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary input |
| out0 | dictionary output stripped of specified keys |
| out1 | series of key/value pairs stripped from the dictionary |

## Arguments

- **keys** (`list`) _(optional)_ — Keys to be stripped from the dictionary

## Messages

- `dictionary(name: symbol)` — Name of a dictionary from which to strip the specified keys

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)

## Help patcher examples

### hierarchy

```
Example — [dict.strip Events::1903]
  fan-in:
    in0 ← [dict] ← [dict eventlist @embed 1] ← [loadbang]    # This dict object clones the 'eventlist' dict to create a new dictionary. We clone before stripping so we don't modify the source dict.
  fan-out:
    out0 → [dict new-eventlist]:in0
    out1 → [route Events::1903]:in0
```

### basic

```
Example — [dict.strip cow sheep]
  fan-in:
    in0 ← [dict.group] ← [message "cow : moo moo, sheep : baa baa, dog woof woof, bang"]
  fan-out:
    out0 → [dict.iter]:in0
    out1 → [print stripped_keys]:in0
```

## See also

`dictionaries`, `dict.compare`, `dict.deserialize`, `dict.group`, `dict.iter`, `dict.join`, `dict.pack`, `dict.print`, `dict.route`, `dict.serialize`, `dict.slice`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
