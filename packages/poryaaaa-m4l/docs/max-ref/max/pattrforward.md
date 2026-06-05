# pattrforward

_max · Data_

> Send any message to a named object

Routes messages or selects new message routing destinations according to the messages it receives. You can also use the pattrforward object to route messages directly to a specific inlet of an object exposed by pattr or autopattr objects, and also send messages directly to a subpatcher, abstraction or bpatcher.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages or data in |
| out0 | dumpout |

## Arguments

- **target** (`symbol`) _(optional)_ — Target object
  A symbol argument may be optionally used to specify the target object.

## Messages

- `bang` — Forward data to the target
  bang is passed to the target object.
- `int(input: int)` — Forward data to the target
  An int is passed to the target object.
- `float(input: float)` — Forward data to the target
  A float is passed to the target object.
- `list(input: list)` — Forward data to the target
  A list is passed to the target object.
- `anything(input: list)` — Forward data to the target
  Incoming messages to the pattrforward object are analyzed. If the first element of the message matches the symbols in0, in1 ... inN, the pattrforward object will forward all remaining arguments to a specific inlet of the target object. in0 refers to the leftmost inlet (this is the default behavior of the pattrforward object), in1 refers to the inlet to the 2nd inlet from the left, and so on.
  If the target object is a subpatcher, abstraction or bpatcher, the special element inx can be used to send messages directly to the patcher object (in essence, in order to communicate with a virtual thispatcher object associated with the patch).

## GUI behaviors

- `(mouse)` — Reveal the current target object
  Double-clicking on the pattrforward object reveals the current target object in its parent patcher.

## Attributes

- `@send` (symbol) — Target object
  The word send, followed by the patcher name of any object in the patcher hierarchy, sets the target object. Names can refer to objects in patchers other than the one in which the pattrforward object resides. A double-colon syntax ('::') is used to separate levels of the patcher hierarchy. For example, some_subpatcher::some_object or parent::some_other_object would be valid target object names.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `getsend` — seen as: `getsend`
- `in0` — seen as: `in0 0 0 0`, `in0 1`, `in0 bang`
- `in1` — seen as: `in1 2`, `in1 bang`
- `in2` — seen as: `in2 3`
- `inx` — seen as: `inx front`
- `select` — seen as: `select`
- `send` — seen as: `send helppattrforward01`, `send helppattrforward02`
- `set` — seen as: `set 74`

## Help patcher examples

### subpatchers

> A pattrforward in a subpatcher can address a named UI-object in its parent patcher by using the syntax "parent::<Scripting_Name>".

> "Scripting Name" in the Inspector is the attribute "varname".

### inlets

> You can also route messages to an arbitrary inlet of the target object by using the special reserved messages in0, in1,…in9, where in0 is the first inlet, etc.

> If the target object is a patcher, abstraction or bpatcher, the special message "inx" can be used to send messages to the patcher object itself.

```
Example #1 — [pattrforward helppattrforward04]
  fan-in:
    in0 ← [message "inx front"]
    in0 ← [message "in1 bang"]
    in0 ← [message "in0 bang"]
```

```
Example #2 — [pattrforward helppattrforward03]
  fan-in:
    in0 ← [message "in0 1"]
    in0 ← [message "in1 2"]
    in0 ← [message "in2 3"]
    in0 ← [message "in0 0 0 0"]    # Send messages to pak
```

### basic

> The argument sets the initial target, which can be changed with the "send <target>" message.

```
Example — [pattrforward helppattrforward01]
  fan-in:
    in0 ← [message "getsend"]
    in0 ← [number]    # Send a number to the target (named UI-object).
    in0 ← [message "send helppattrforward01"]    # The send message is used only to change targets. It can not be used as a message to the target.. / Set a new target
    in0 ← [message "set 74"]
    in0 ← [message "select"]    # send any message to the target.
    in0 ← [message "bang"]
    in0 ← [message "send helppattrforward02"]
  fan-out:
    out0 → [message ""]:in1
```

## See also

`autopattr`, `forward`, `pattr`, `pattrhub`, `pattrmarker`, `pattrstorage`, `receive`, `send`, `thispatcher`
