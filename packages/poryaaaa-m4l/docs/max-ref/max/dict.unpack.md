# dict.unpack

_max · Dictionary_

> Extract values from a dictionary

Use the dict.unpack object to return the values of specified keys through dedicated outlets.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary input |
| out0 | value for the 'foo' key |
| out1 | dictionary: unmatched keys |

## Arguments

- **name** (`symbol`) _(optional)_ — Name of the dictionary to unpack
  Initial name of the dictionary to unpack when receiving a bang message. This will be replaced by the name of any dictionaries received at the object's inlet.
- **default-values** (`list`) _(optional)_ — Dictionary syntax specifying initial keys and values to be fetched
  Dictionary syntax specifying initial keys and values to be fetched. See Dictionaries for more information on dictionary syntax.

## Messages

- `bang` — Re-fetch the values from the dictionary last received
- `array(ARG_NAME_0: symbol)` — TEXT_HERE
- `dictionary(name: symbol)` — Name of a dictionary from which to fetch values for output

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)

## Help patcher examples

### basic

```
Example — [dict.unpack cow: frog: pig: nose: lion: duck:]
  fan-in:
    in0 ← [message "dictionary ark"]
  fan-out:
    out0 → [message "moo"]:in1
    out1 → [message ""]:in1
    out2 → [message ""]:in1    # dict.unpack extracts only the keys specified as arguments, if they exist in the dictionary received.
    out3 → [message ""]:in1
    out4 → [message "roar 7 times"]:in1
    out5 → [message ""]:in1
```

## See also

`dictionaries`, `dict.compare`, `dict.deserialize`, `dict.group`, `dict.iter`, `dict.join`, `dict.pack`, `dict.print`, `dict.route`, `dict.serialize`, `dict.slice`, `dict.strip`, `dict.view`, `dict`
