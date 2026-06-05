# matrix

__

> Event routing matrix

The matrix object controls the connections between inlets and outlets. You can route any inlet to any combination of outlets. Each connection has an associated gain factor and all values travelling through matrix can be scaled. It shares the same control protocol with the signal matrix~ object, but unlike matrix~, the outlets of the matrix object do not add the values of multiple inputs. matrix is best understood as a combination of gate and switch with many more features.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | list | Connect or Disconnect |
| in1 | list | Input 0 |
| in2 | list | Input 1 |
| out0 | dictionary | Current Connections |
| out1 | dictionary | Output 0 |
| out2 | dictionary | Output 1 |

### Port details

**`in0` (Connect or Disconnect):** The leftmost inlet is dedicated to messages to add or delete connections between the other inlets and outlets.

**`in1` (Input 0):** The other inlets receive messages that are relayed to any outlets connected to the inlet.

**`out0` (Current Connections):** The leftmost outlet will output a dictionary containing a list of the current connections in response to the dumpconnections message.

**`out1` (Output 0):** The other outlets output messages that are relayed by any connected inlets.

## Arguments

- **number-of-inputs** (`int`) _(optional)_ — Input Count
  Sets the number of inputs; if not present the default is 2.
- **number-of-outputs** (`int`) _(optional)_ — Output Count
  Sets the number of outputs; if not present the default is 2.
- **default-connect-gain** (`float`) _(optional)_ — Default Gain for Connections
  If a float value is provided as a third argument, it sets a default gain to be used for the connect message when a gain argument to connect is not supplied.

## Messages

- `list(ARG_NAME_0: list)` — Connect or disconnect; data to be routed
  In left inlet: A list either adds or removes a connection between an inlet and outlet. The format of the message is input index (starting at 0 for the object's secont inlet from left), output index (starting at 0), gain. A non-zero gain adds a connection with the designated gain; a gain of 0 deletes the connection if it exists.
  A list received in any other inlet is routed to any connected outlets.
- `clear` — Delete all connections
  In left inlet: The clear message removes all inlet - outlet connections.
- `connect(index: list)` — Connect
  In left inlet: The word connect followed by an inlet index, outlet index, and optional gain, adds a connection between the inlet and outlet. If the gain value is not present, the default gain (set either via typed-in argument or the defaultgain attribute) will be used for the connection.
- `dictionary(dictionary name: symbol)` — Set All Connections
  The word dictionary followed a symbol naming a Max dictionary object, replaces all connections with the ones in the dictionary. If the dictionary has no connections, the existing connections are cleared. To see the format of the dictionary, send the dumpdictionary message and print or otherwise capture the resulting dictionary that is sent out the object's left outlet.
- `disconnect(index: list)` — Disconnect
  In left inlet: The word disconnect followed by an inlet index and outlet index deletes a connection between the inlet and outlet if it exists.
- `dumpconnections` — Report Connections
  The dumpdictionary message outputs a Max dictionary message listing all current connections of the matrix object.

## Attributes

- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `convolution` — seen as: `convolution`
- `outscale` — seen as: `outscale 0 100`
- `supra` — seen as: `supra $1`

## Help patcher examples

### any message

```
Example — [matrix 2 2 @scalemode 1]  out 1 / out 0 / in 1 / in 0
  fan-in:
    in0 ← [message "1 1 0.5"]    # using scalemode 1, numeric arguments to messages will be scaled if the gain is something other than 1
    in0 ← [message "0 0 1"]
    in0 ← [message "1 1 1"]
    in0 ← [message "1 0 1"]    # connect
    in0 ← [message "1 0 0"]    # disconnect
    in0 ← [message "0 0 0"]
    in0 ← [message "1 1 0"]
    in1 ← [message "convolution"]
    in1 ← [metro 1000] ← [toggle]
    in2 ← [message "supra $1"]
  fan-out:
    out1 → [button]:in0
    out1 → [message "bang"]:in1
    out2 → [button]:in0
    out2 → [message "supra 1.5"]:in1
```

### crosspatch

```
Example — [matrix 4 4]  out 3 / out 2 / out 1 / out 0 / in 3 / in 2 / in 1 / in 0
  fan-in:
    in0 ← [crosspatch]    # make connections with crosspatch
    in1 ← [number] ← [slider]
    in2 ← [number] ← [slider]
    in3 ← [number] ← [slider]    # move sliders
    in4 ← [number] ← [slider]
  fan-out:
    out1 → [slider]:in0
    out2 → [slider]:in0
    out3 → [slider]:in0
    out4 → [slider]:in0
```

### dictionaries

> To see the current contents of a matrix~ as a dictionary, use the dumpconnections message.

```
Example — [matrix 4 4]
  fan-in:
    in0 ← [dict @embed 1] ← [button]
    in0 ← [message "dumpconnections"]
    in0 ← [dict @embed 1] ← [button]    # set the state of the matrix with these dictionaries
    in1 ← [metro 500] ← [toggle]    # start the noise
    in2 ← [metro 700] ← [toggle]    # start the noise
    in3 ← [metro 1100] ← [toggle]    # start the noise
    in4 ← [metro 800] ← [toggle]    # start the noise
  fan-out:
    out0 → [dict.view]:in0
    out1 → [message "36 64"]:in0
    out1 → [button]:in0
    out2 → [message "48 70"]:in0
    out2 → [button]:in0
    out3 → [message "60 70"]:in0
    out3 → [button]:in0
    out4 → [message "70 64"]:in0
    out4 → [button]:in0
```

### gain + scaling

> When used by itself, the input is multiplied by the gain BEFORE it is constrained by the input range

> With both inrange and outscale the gain is a proportion of the input scaled by the output. So in this example, the gain is applied AFTER being constrained by the input

```
Example #1 — [matrix 1 1 @inrange 0 1 @outscale 0 1]
  fan-in:
    in0 ← [message "outscale 0 100"]    # rescale the output
    in0 ← [message "0 0 0.5"]    # connect with gain multiplication
    in0 ← [message "0 0 1"]    # connect with no gain multiplication
    in1 ← [flonum] ← [slider]    # move the slider to see the result
  fan-out:
    out1 → [flonum]:in0
```

```
Example #2 — [matrix 1 1 @inrange 0 1]
  fan-in:
    in0 ← [message "0 0 1"]    # connect with no gain multiplication
    in0 ← [message "0 0 0.5"]    # connect with gain multiplication
    in1 ← [flonum] ← [slider]    # move the slider to see the result
  fan-out:
    out1 → [flonum]:in0
```

### basic

> O U T S

```
Example — [matrix 4 4]  out 3 / out 2 / out 1 / out 0 / in 3 / in 2 / in 1 / in 0
  fan-in:
    in0 ← [matrixctrl]    # make connections with matrixctrl / 3 / 2 / 1 / 0 / 3 / 2 / 1 / 0
    in0 ← [message "dumpconnections"]    # get connections
    in1 ← [number] ← [slider]
    in2 ← [number] ← [slider]
    in3 ← [number] ← [slider]    # move sliders
    in4 ← [number] ← [slider]
  fan-out:
    out0 → [dict.view]:in0
    out1 → [slider]:in0
    out2 → [slider]:in0
    out3 → [slider]:in0
    out4 → [slider]:in0
```

## See also

`crosspatch`, `gate`, `matrix~`, `matrixctrl`, `receive`, `router`, `switch`, `send`, `switch`
