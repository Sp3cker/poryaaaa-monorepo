# pattr

_max · Data_

> Provide an alias with a named data wrapper

Stores its own data, or binds to another object to share its contents with other pattr-based objects (such as pattrstorage). Can be used for data routing or preset creation.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages or data in |
| out0 | attribute value |
| out1 | bindto connection |
| out2 | dumpout |

## Arguments

- **name** (`symbol`) _(optional)_ — Connection name
  A symbol argument may be optionally used to set the pattr object's name. In the absence of an argument (or the explicit setting of the name attribute using the @name syntax), the pattr object is given an arbitrary, semi-random name, such as u197000004.

## Messages

- `bang` — Output current values
  Outputs the data maintained by the pattr object from the left outlet.
- `int(input: int)` — Store and output data
  An int is stored inside the pattr object and output from its left outlet. Optionally, the value is passed along to a bound object. (See the bindto attribute for more information on bound objects).
- `float(input: float)` — Store and output data
  float is stored inside the pattr object and output from its left outlet. Optionally, the value is passed along to a bound object. (See the bindto attribute for more information on bound objects).
- `list(input: list)` — Store and output data
  list is stored inside the pattr object and output from its left outlet. Optionally, the value is passed along to a bound object. (See the bindto attribute for more information on bound objects).
- `anything(input: list)` — Store and output data
  Any message is stored inside the pattr object and output from its left outlet. Optionally, the value is passed along to a bound object. (See the bindto attribute for more information on bound objects).
- `assign(input: float)` — Store and output data
  The word assign, followed by a floating point value, causes that value to be stored and displayed and sent out the pattr object's left outlet. If the object’s Parameter Enabled attribute is set (checked) and the Parameter Visibility attribute is set to Stored Only, the assign message will not add the new value to the Live application’s undo chain.
- `dictionary(input: symbol)` — Store and output data.
  A copy of a dictionary is stored inside the pattr object and output from its left outlet. Optionally, the dictionary is passed along to a bound object. (See the bindto attribute for more infomation on bound objects).
- `init` — Revert to the initial value
  If the pattr object's initial attribute has been set, the init message will cause the pattr object's value to be set to value of the initial attribute.

## GUI behaviors

- `(mouse)` — Open the Parameters window
  Double-clicking on a pattr object that is parameter-enabled will open the Parameters Window in Max for Live.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `The` — seen as: `The proportions seem so right.`
- `bindto` — seen as: `bindto boxster`, `bindto pantry::oil`, `bindto pantry::oregano`
- `golden` — seen as: `golden`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out2` — dumpout

### snapshots

```
Example — [pattr myplug]  a connected pattr object will gather the plug-in or device parameters as a dictionary / Snapshots can be easily integrated into your pattr workflow. Using a pattr object, the internal state of a VST, AU or AMXD can be saved and recalled.
  fan-out:
    out1 → [amxd~ "Additive Heaven.amxd"]:in0
```

### MaxForLive

> In Max for Live, if you activate the parameter_enable attribute (Parameter Mode Enable), the pattr value will be saved with the Live set. Must be used in a Max For Live device in order to see it in action.

> These two pattrs have their Parameter Mode Enabled, and their Parameter Type set to "Int". Other types, such as "Float" and "Enum" (list) work similarly. These three types are automatable in Live.

```
Example — [pattr Function] (Function)  Store complex sets of data with Parameter Type "Blob",
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [message "1000. 0. 1. 103.723404 0.6 0 353.723404 0.84 0 686.170227 0.453333 0 linear"]:in1
    out1 → [function]:in0
```

```
Example — [pattr Slider_Y] (Slider_Y)
  fan-in:
    in0 ← [pictslider]
  fan-out:
    out0 → [pictslider]:in1
```

```
Example — [pattr Slider_X] (Slider_X)
  fan-in:
    in0 ← [pictslider]
  fan-out:
    out0 → [pictslider]:in0
```

### subpatchers

> this number box is named "salt"
>
> this number box is named "pepper"
>
> double-click subpatcher "pantry", which contains number boxes "oil" and "oregano"

> One pattr object can be bound to several named interface objects. If the interface objects are inside a subpatch, the double-colon syntax is used.

```
Example — [pattr]
  fan-in:
    in0 ← [message "bindto pepper"]
    in0 ← [message "bindto pantry::oregano"]
    in0 ← [message "bindto pantry::oil"]
    in0 ← [message "bindto salt"]
    in0 ← [number]    # send a message to the number box
  fan-out:
    out0 → [print pattr-out @popup 1]:in0
```

### bind

> Bind:
>
> A pattr object is usually bound to a user-interface object, so that the pattr becomes a "wrapper" for the data in the user-interface object, storing it and recalling it. There are several ways to bind a UI-object to pattr:
>
> connect UI-object to pattr's second outlet:
>
> Use the second argument:
>
> Bind to an attribute of a named object:

```
Example #1 — [pattr attrChange @bindto boxster::bgcolor]
  fan-in:
    in0 ← [swatch]    # change the bgcolor of 'boxter'
```

```
Example #2 — [pattr whatever boxster]  pattr's second argument is the name of the bound object.
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [print pattr-out @popup 1]:in0
```

```
Example #3 — [pattr]
  fan-in:
    in0 ← [button]    # bang to output stored message
    in0 ← [message "bindto boxster"]    # send pattr the "bindto <UI-object-name> message:
    in0 ← [number]
  fan-out:
    out0 → [print pattr-out @popup 1]:in0
```

```
Example #4 — [pattr]  enter a number to be stored in pattr
  fan-in:
    in0 ← [button]    # bang to output stored message
  fan-out:
    out0 → [print pattr-out @popup 1]:in0
    out1 → [number]:in0
```

### basic

> If pattr's name (its scripting name) is not explicitly given (as an argument or attribute), Max will generate a unique name that looks something like u000000000. Look in the Inspector, at Scripting Name.

```
Example — [pattr]
  fan-in:
    in0 ← [button]    # bang to output stored message
    in0 ← [message "The proportions seem so right."]
    in0 ← [message "1 1 2 3 5 8 13 21 34"]
    in0 ← [message "golden"]
    in0 ← [message "1.618034"]
  fan-out:
    out0 → [print pattr-out @popup 1]:in0
```

## See also

`autopattr`, `pattrforward`, `pattrhub`, `pattrmarker`, `pattrstorage`
