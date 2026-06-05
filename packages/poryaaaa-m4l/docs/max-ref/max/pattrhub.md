# pattrhub

_max · Data_

> Access all pattr objects in a patcher

Centralizes communication with all pattr objects in a patcher.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages in |
| out0 | passout |
| out1 | dumpout |

## Messages

- `bang` — Pass through
  bang is passed through the pattrhub object and output from its left outlet.
- `int(input: int)` — Pass through
  An int is passed through the pattrhub object and output from its left outlet.
- `float(input: float)` — Pass through
  float is passed through the pattrhub object and output from its left outlet.
- `list(input: list)` — Pass through
  list is passed through the pattrhub object and output from its left outlet.
- `anything(arguments: list)` — Send values to pattr objects
  Incoming messages to the pattrhub object are analyzed. If the first element of the message matches the name of a pattr- or autopattr-maintained object, the subsequent arguments in the message set that object's value. If the first element of the message matches get (name), where (name) matches the name of a pattr- or autopattr-maintained object, the value of that object is sent from the pattrhub object's right outlet, preceded by the object's name. Otherwise, the message is passed through the pattrhub object and output from its left outlet.
- `getattributes(target-name(s): list)` — Output a list of pattr object names
  The getattributes message causes a list of all pattr- or autopattr-maintained object names to be output from the pattrhub object's right outlet, preceded by the symbol attributes.
- `getstate(target: list)` — Retrieve pattr objects and values
  The getstate message causes a series of lists to be output from the pattrhub object's right outlet -- one for every pattr- or autopattr-maintained object in the patcher containing the pattrhub object. Each list begins with the name of the object, and is followed by the object's current value.

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `day` — seen as: `day $1`
- `getday` — seen as: `getday`
- `gethour` — seen as: `gethour`
- `getmonth` — seen as: `getmonth`
- `getyear` — seen as: `getyear`
- `month` — seen as: `month $1`
- `year` — seen as: `year $1`

## Help patcher examples

### subpatchers

> Messages to a pattrhub can be routed to pattrs in subpatchers and parent patchers, using a double-colon syntax. Messages to the parent patcher use the format "parent::<pattr-name>".

### basic

```
Example — [pattrhub]
  fan-in:
    in0 ← [message "year $1"]
    in0 ← [message "getyear"]
    in0 ← [message "getmonth"]
    in0 ← [message "month $1"]
    in0 ← [message "getday"]    # get values from pattr objects
    in0 ← [message "day $1"]    # send values to pattr objects
    in0 ← [message "gethour"]    # address non-existant pattr
  fan-out:
    out0 → [message ""]:in1    # passout
    out1 → [message ""]:in1    # dumpout
```

## See also

`autopattr`, `pattr`, `pattrforward`, `pattrmarker`, `pattrstorage`
