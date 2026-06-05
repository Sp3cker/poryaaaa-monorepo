# funnel

_max · Data, Lists_

> Tag data with its inlet number

Identifies the inlet of incoming data. It can be used to store values into a table or coll based on their source, or used to set a destination with an object such as spray.

## Arguments

- **inlets** (`int`) _(optional)_ — Number of inlets
  The first argument sets the number of inlets in the funnel. If there is no argument there will be two inlets.
- **offset** (`int`) _(optional)_ — Number of inlets
  The second argument specifies an offset for the first inlet number. If no second argument is present, the inlets are numbered beginning with 0.

## Messages

- `bang` — Send most recent data
  In any inlet: The number of the inlet and the stored (most recently received) number in that inlet are sent out as a two-item list.
- `int(input: int)` — Output tagged data
  In any inlet: The number of the inlet and the received number are sent out as a list.
- `float(input: float)` — Output tagged data
  In any inlet: The number of the inlet and the received number are sent out as a list.
- `list(inputs: list)` — Output tagged data
  In any inlet: The number of the inlet is prepended to the list, and the new list is sent out. In a list floats are not converted to ints. The list may contain ints, floats, and symbols (provided that the first element of the list is not a symbol).
- `anything(inputs: list)` — Output tagged data
  Functions the same as list.
- `offset(offset: int)` — Offset the tag value
  The word offset followed by a number will offset the numbering of inlets by the number given.
- `set(inputs: list)` — Store the data with no output
  In any inlet: The word set followed by a list of numbers which correspond with the number of inlets, will set the input list of numbers without sending them through the outputs.

## Help patcher examples

### coll example

```
Example — [funnel 8]  Format and store data
  fan-in:
    in0 ← [number] ← [slider]
    in1 ← [number] ← [slider]
    in2 ← [number] ← [slider]
    in3 ← [number] ← [slider]
    in4 ← [number] ← [slider]
    in5 ← [number] ← [slider]
    in6 ← [number] ← [slider]
    in7 ← [number] ← [slider]
  fan-out:
    out0 → [coll]:in0
```

### basic

> An optional second argument sets a starting offset (other than 0) for the inlet number output to let you combine multiple funnels more easily.

```
Example — [funnel 5]
  fan-in:
    in0 ← [number] ← [slider]
    in1 ← [number] ← [slider]
    in2 ← [number] ← [slider]
    in3 ← [number] ← [slider]
    in4 ← [number] ← [slider]
    in4 ← [button]    # Send a bang in any inlet to report that inlet's last value and index
  fan-out:
    out0 → [spray 5]:in0    # The funnel and spray objects work well together
    out0 → [message "0 8"]:in1
```

## See also

`listfunnel`, `spray`
