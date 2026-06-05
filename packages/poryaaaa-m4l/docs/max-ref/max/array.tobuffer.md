# array.tobuffer

_max · Array_

> Write array object values to an audio buffer

Values are assumed to be between -1 and 1. If the array contains multichannel data, each channel should be wrapped inside of a subarray (e.g. [ [ -1, 0, 1 ], [ -1, 0, -1 ] ]).

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | array, messages in |
| in1 | array, messages in |
| out0 | bang on successful buffer write |

## Arguments

- **buffer-name** (`symbol`) — Buffer name
  The name of the target buffer~ object

## Messages

- `bang` — Trigger output
  Reprocess previously received array and trigger output.
- `int(ARG_NAME_0: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(ARG_NAME_0: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(ARG_NAME_0: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `array(ARG_NAME_0: list)` — Write array values into a buffer and trigger output.
  The buffer~ object expects floating-point values between -1 and 1.
- `dictionary(ARG_NAME_0: list)` — Wrap a dictionary object in an array.
  Wrap an incoming dictionary object in an array, then process as described for the array message.
- `string(ARG_NAME_0: list)` — Wrap a string object in an array.
  Wrap a string object in an array, then process as described for the array message.

## Attributes

- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — bang on successful buffer write
> - `in1` — array, messages in

### basic

```
Example — [array.tobuffer arraytobuf @resize 1]
  fan-in:
    in0 ← [array.concat]    # the top-level array contains the channels
```

## See also

`array`, `array.frombuffer`, `peek~`, `poke~`
