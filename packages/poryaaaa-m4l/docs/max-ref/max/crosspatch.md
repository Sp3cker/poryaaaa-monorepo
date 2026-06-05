# crosspatch

__

> Patching Editor for Matrix Objects

Connect a crosspatch to a client object (including matrix~, mcs.matrix~, mc.matrix~, matrix, gate~, mc.gate~, mcs.gate~, gate, selector, mc.selector~, mcs.selector~, and switch) to use a patching interface to edit connections between inputs and outputs.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | list Adds or Deletes a Connection |
| out0 | — | Connect to matrix~ or matrix Object |
| out1 | dictionary | From dump Message |

### Port details

**`in0` (list Adds or Deletes a Connection):** You can update the state of the crosspatch UI as well as any connected objects by sending messages similar to those supported by matrix objects.

**`out0` (Connect to matrix~ or matrix Object):** To allow crosspatch control a matrix object, connect this outlet to the left inlet of the target object.

## Messages

- `bang` — Send Connection State to Clients
  The bang message will copy the current connection state to any objects connected to the left outlet. A dictionary with the current state is sent out the left outlet if any there are any connected objects that aren't compatible with the crosspatch connection protocol. The dump message sends a similar dictionary out the right outlet.
- `list(input-index: int, output-index: int, gain-or-zero: float)` — Add or Delete a Connection
  A list of numbers adds or deletes a connection and updates connected clients. The first number is the input (always zero relative), the second number is the output (always zero relative), and the third number is a gain value. If the gain value is zero, the connection is removed, otherwise it is associated with the connection.
- `clear` — Clear Connections
  The clear message clears all current connections and updates any connected clients.
- `dictionary(dictionary-name: symbol)` — Set All Connections
  When crosspatch receives a dictionary containing connection information, it clears its existing connections and replaces them with those contained in the dictionary, updating connected clients. Connections for inputs or outputs greater than the current input and output size are ignored.
- `dump` — Dump Contents
  The dump message outputs a dictionary out the right outlet containing the current connections.
- `dumpconnections` — Dump Contents
  The dumpconnections message outputs a dictionary out the right outlet containing the current connections.
- `indisable(input-index: int, disable: int)` — Disable an Input Temporarily
  The word indisable, followed by an input index starting at 0, visually disables the input if the second argument is non-zero and enables it if the second argument is 0. Example: indisable 1 1 will disable the second input, and indisable 1 0 will re-enable it. When you disable an input, any connections to it will be removed if allowdisabled is not enabled. Moreover, you cannot connect anything to a disabled input unless allowdisabled is enabled. The disabled state of inputs is not saved in the patcher.
- `outdisable(output-index: int, disable: int)` — Disable an Output Temporarily
  The word outdisable, followed by an output index starting at 0, visually disables the input if the second argument is non-zero and enables it if the second argument is 0. Example: outdisable 1 1 will disable the second output, and outdisable 1 0 will re-enable it. When you disable an output, any connections to it will be removed if allowdisabled is not enabled. Moreover, you cannot connect anything to a disabled output unless allowdisabled is enabled. The disabled state of outputs is not saved in the patcher.

## GUI behaviors

- `(mouse)` — Edit Connections and Gains
  To make a connection between an input and output, click on a white circle on either the input or output side of the object and either move or drag to the desired destination circle. The destination will highlight when the mouse is over it. If you released the mouse to start the connection, click again to complete the connection. If you dragged the mouse, release the button.
  To delete an existing connection click once on the line to select it, then press the delete or backspace key.
  To edit the gain of a connection, click on the line and drag upwards or downwards. A dial will indicates the current gain as you drag.
  As you edit connections with crosspatch, any connected matrix objects will update to reflect changes in gain or connection state.

## Attributes

- `@category` (atom)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@save` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `dividers` — seen as: `dividers in 1`, `dividers none`
- `incolormap` — seen as: `incolormap 3 3 3 2`, `incolormap none`
- `outcolormap` — seen as: `outcolormap 2 2 4 4`, `outcolormap none`

## Help patcher examples

### mc

> Note: you have to set the input + output dimensions of the crosspatch manually to fit the mc.matrix~ dimensions (in this case 4 inputs and 2 outputs)

> This mc.matrix~ contains 10 matrix~ objects; as you patch inputs to outputs, crosspatch is setting each one to the identical state. Crosspatch cannot edit individual matrix~ instances within an mc.matrix~ separately.

```
Example — [crosspatch]
  fan-in:
    in0 ← [button]    # if connections get out of sync, bang will send the crosspatch state to all connected ojects
  fan-out:
    out0 → [mc.matrix~ 4 2]:in0
```

### colormap

```
Example — [crosspatch]
  fan-in:
    in0 ← [attrui @candycane]    # set the number of colors used
    in0 ← [attrui @candycane2]
    in0 ← [attrui @candycane3]
    in0 ← [attrui @candycane4]
    in0 ← [attrui @candycane5]
    in0 ← [attrui @candycane6]
    in0 ← [attrui @candycane7]
    in0 ← [attrui @candycane8]
    in0 ← [attrui @linecolor]    # customize mapped colors
    in0 ← [attrui @candymode]    # set whether inputs or outputs determine the connection color
    in0 ← [message "incolormap none"]    # clear color map (reverts to candycane mode)
    in0 ← [attrui @colorlabels]    # turn on color labels
    in0 ← [message "outcolormap none"]    # clear color map (reverts to candycane mode)
    in0 ← [message "outcolormap 2 2 4 4"]    # set outputs 0 and 1 to be color 1 and outputs 2 and 3 to be color 4
    in0 ← [message "incolormap 3 3 3 2"]    # set inputs 0, 1, and 2 to be color 3 and input 4 to be color 2
```

Attributes demonstrated: `@candycane`, `@candycane2`, `@candycane3`, `@candycane4`, `@candycane5`, `@candycane6`, `@candycane7`, `@candycane8`, `@candymode`, `@colorlabels`, `@linecolor`

### gain

> setting an initial and max gain
>
> You can set an initial gain for a new connection less than the default 1.0 to ramp up to the desired gain after making the connection. The initial gain cannot be zero, however it can be as low as -70 dB (0.000316). Set a maximum gain above 1.0 -- the dial will show the amount above 1 in orange.
>
> gain dial shape
>
> The gain dial can be a full circle at 1.0 or the Max dial 270 degree arc

> Gain dials can be shown always, never, or only when the gain is not equal to 1.0...

> Dragging a gain control can have more (dB) or less (linear) control near the bottom of the amplitude range

```
Example #1 — [crosspatch]
  fan-in:
    in0 ← [attrui @gainstyle]    # Try dial and full circle styles
```

```
Example #2 — [crosspatch]
  fan-in:
    in0 ← [attrui @gaindragmode]    # change and then drag the yellow gain dial up and down
```

```
Example #3 — [crosspatch]
  fan-in:
    in0 ← [attrui @showgain]    # change and observe gain dials
```

```
Example #4 — [crosspatch]
  fan-in:
    in0 ← [attrui @initialgain]    # change the value to something below 1 and then make a connection below
    in0 ← [attrui @maxgain]    # change the value to something other than 1 and drag up and down on a gain dial
```

Attributes demonstrated: `@gaindragmode`, `@gainstyle`, `@initialgain`, `@maxgain`, `@showgain`

### dividers

> connecting across dividers

> restrict either inputs or outputs or both to a single connection per input or output

> By turning off connectacrossdividers, you can prevent any connections outside of the "groups' created by the visual divider

```
Example #1 — [crosspatch]
  fan-in:
    in0 ← [attrui @connectacrossdividers]    # turn on and off and try to make a connection from In 3 to Out 0
```

```
Example #2 — [crosspatch]
  fan-in:
    in0 ← [attrui @dividercolor]    # customize divider color
    in0 ← [message "dividers none"]    # remove all dividers
    in0 ← [message "dividers in 1"]    # add a divider after In 1
```

Attributes demonstrated: `@connectacrossdividers`, `@dividercolor`

### customizing behavior

> disabling connections

> restrict either inputs or outputs or both to a single connection per input or output

> You can disable inputs or outputs. The disabled state is not saved with the object or in presets. As in this example, disabling is intended to be something that happens when certain connections occur. The allowdisabled attribute permits connecting to disabled inputs or outputs.

> with preservegain enabled, try connecting something to Out 1; the new connection will inherit the gain of the existing connection from In 0 to Out 1

```
Example #1 — [crosspatch]  making any connection from In 1 will disable Out 3; deleting a connection from In 1 will re-enable Out 3
  fan-in:
    in0 ← [attrui @allowdisabled]
    in0 ← [message "indisable 0 1"]    # disable input 1
    in0 ← [message "indisable 0 0"]    # enable input 1
    in0 ← [message "clear"]
    in0 ← [message "outdisable 3 $1"]
  fan-out:
    out0 → [route set]:in0
    out0 → [message "dictionary u269001493"]:in1    # parse the output and do something with it
```

```
Example #2 — [crosspatch]
  fan-in:
    in0 ← [attrui @exclusive]
    in0 ← [attrui @preservegain]
```

```
Example #3 — [crosspatch]
  fan-in:
    in0 ← [attrui @exclusive]    # try different combinations of exclusive mode and then try to make multiple connections for the same input or output
```

Attributes demonstrated: `@allowdisabled`, `@exclusive`, `@preservegain`

### gate~ + selector~

```
Example #1 — [crosspatch]
  fan-out:
    out0 → [switch 4]:in0
```

```
Example #2 — [crosspatch]
  fan-out:
    out0 → [gate]:in0
```

```
Example #3 — [crosspatch]
  fan-out:
    out0 → [gate~ 5 2]:in0    # Note this gate has an initial connection argument which is reflected in crosspatch when the patcher is loaded
```

```
Example #4 — [crosspatch]
  fan-out:
    out0 → [selector~ 5]:in0
```

### appearance

```
Example — [crosspatch]
  fan-in:
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @labelwidth]
    in0 ← [attrui @candycane]
    in0 ← [attrui @candycane2]
    in0 ← [attrui @candycane3]
    in0 ← [attrui @candycane4]
    in0 ← [attrui @candycane5]
    in0 ← [attrui @candycane6]
    in0 ← [attrui @candycane7]
    in0 ← [attrui @candycane8]
    in0 ← [attrui @textcolor]
    in0 ← [attrui @dividercolor]    # add a divider
    in0 ← [attrui @linecolor]
    in0 ← [attrui @candymode]
    in0 ← [message "dividers in 1"]
    in0 ← [attrui @colorlabels]
    in0 ← [attrui @showgain]
    in0 ← [attrui @gainradius]
    in0 ← [attrui @overgaincolor]
    in0 ← [attrui @maxgain]
    in0 ← [attrui @labelheight]
```

Attributes demonstrated: `@bgcolor`, `@candycane`, `@candycane2`, `@candycane3`, `@candycane4`, `@candycane5`, `@candycane6`, `@candycane7`, `@candycane8`, `@candymode`, `@colorlabels`, `@dividercolor`, `@gainradius`, `@labelheight`, `@labelwidth`, `@linecolor`, `@maxgain`, `@overgaincolor`, `@showgain`, `@textcolor`

### dictionaries

```
Example — [crosspatch]
  fan-in:
    in0 ← [dict @embed 1] ← [button]    # set the state of the matrix with these dictionaries
    in0 ← [message "clear"]    # remove all connections
    in0 ← [message "dumpconnections"]    # view dictionary in dict.view below
    in0 ← [dict @embed 1] ← [button]
  fan-out:
    out0 → [matrix~ 4 2 1.]:in0
    out1 → [dict.view]:in0    # To see the current contents of a matrix~ as a dictionary, use the dumpconnections message. A dictionary will be sent out the rightmost outlet.
```

### matrix

```
Example — [crosspatch]  make connections and change gains with crosspatch
  fan-out:
    out0 → [matrix 4 4]:in0    # out 3 / out 2 / out 1 / out 0 / in 3 / in 2 / in 1 / in 0
```

### basic

```
Example — [crosspatch]  dictionary out / drag up and down on a connection to change the gain / to delete a connection select it and press the delete key (just like Max!) / outputs / inputs / ...or from output to input / click from input to output to patch... / control out / connect crosspatch to a single object for two-way communication
  fan-in:
    in0 ← [message "2 1 0"]
    in0 ← [message "2 1 1"]    # make connections just like matrix~
  fan-out:
    out0 → [matrix~ 4 4 1.]:in0    # matrix~ third argument is required for gain control to be enabled in crosspatch
```

## See also

`gate~`, `matrix`, `matrix~`, `matrixctrl`, `selector~`
