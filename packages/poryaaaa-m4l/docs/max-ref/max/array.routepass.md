# array.routepass

_max · Array_

> Route a complete input array object based on input matching

Like routepass, but for array objects.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array in |
| out0 | array out if there are no matches |

## Arguments

- **match keys** (`list`) — keys to match
  Determines the number of outlets. The object will route arrays to the associated outlet if the first element of the array matches the argument key. Unmatched arrays are routed through the rightmost outlet. The special keys emptystring and emptyarray match the empty string and empty array, and the key matches the empty symbol.

## Messages

- `bang` — Trigger output
  Reprocess previously received arrays and trigger output.
- `int(value: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(value: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Route the array to a specific outlet based on the first element of the array.
  The arguments to array.routepass determine the number of outlets, as well as how array inputs will be routed. If the first element of the input array does not match any of the arguments, the whole array is sent out of the last outlet. Like the routepass object, the input is routed without being altered.
- `dictionary(dictionary-value: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(string-value: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@match` (int) — Matching Mode
  By default array.routepass will attempt to match starting at the beginning of the input array. However, some options are available.
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `alpha` — seen as: `alpha blue green red`, `alpha tuna fish red`
- `blue` — seen as: `blue skies`
- `red` — seen as: `red dress`, `red green blue alpha`
- `redress` — seen as: `redress my grievances`
- `unmatched` — seen as: `unmatched excellence`

## Help patcher examples

### match

```
Example — [array.routepass red green blue alpha]
  fan-in:
    in0 ← [message "red green blue alpha"]
    in0 ← [message "alpha blue green red"]    # set array
    in0 ← [attrui @match]
    in0 ← [message "alpha tuna fish red"]
  fan-out:
    out0 → [print red green blue alpha overflow @popup 1]:in0
    out1 → [print red green blue alpha overflow @popup 1]:in1
    out2 → [print red green blue alpha overflow @popup 1]:in2
    out3 → [print red green blue alpha overflow @popup 1]:in3
    out4 → [print red green blue alpha overflow @popup 1]:in4
```

Attributes demonstrated: `@match`

### basic

```
Example #1 — [array.routepass emptystring <empty> emptyarray]  Special keys to match empty objects.
  fan-in:
    in0 ← [string] ← [button]
    in0 ← [array.wrap] ← [array] ← [button]
    in0 ← [array] ← [button]
  fan-out:
    out0 → [print empty-string empty empty-array @popup 1]:in0
    out0 → [button]:in0
    out1 → [button]:in0
    out1 → [print empty-string empty empty-array @popup 1]:in1
    out2 → [button]:in0
    out2 → [print empty-string empty empty-array @popup 1]:in2
```

```
Example #2 — [array.routepass red blue]  array out if input matches "red" / array out if input matches "blue"
  fan-in:
    in0 ← [message "red dress"]
    in0 ← [message "unmatched excellence"]
    in0 ← [message "redress my grievances"]
    in0 ← [message "blue skies"]
  fan-out:
    out0 → [print red blue overflow @popup 1]:in0    # array out if there are no matches
    out1 → [print red blue overflow @popup 1]:in1    # array out if there are no matches
    out2 → [print red blue overflow @popup 1]:in2    # array out if there are no matches
```

## See also

`array`, `routepass`, `select`
