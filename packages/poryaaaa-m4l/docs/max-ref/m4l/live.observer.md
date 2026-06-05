# live.observer

_m4l · Live API Objects_

> Monitor changes in Live objects

live.observer
 is used to listen to changes in the values of properties of Live
 objects. This object works in conjunction with the
 live.path
 object, which sends

 id
 nn

 messages into the right inlet of
 live.observer
 .

 After an object id and property is specified, its value is sent out the
 left outlet. From this moment on, the value is sent on each change of the
 property ('notification') as well as in response to bang messages.

 The left outlet is reserved for value messages, all other output is sent to
 the right outlet.

 Not all properties can be observed, please consult the
 Live Object Model
 to see which can. Also, it is not possible to modify the live set from a
 notification, i.e. while you are receiving a value message spontaneously
 sent by a
 live.observer
 's outlet.

 Besides properties, it is also possible to observer children of Live
 objects. Their values are object ids or lists of them.

	Note: The Live API runs in the main thread in Live, and all messages to and from the API are automatically deferred.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Left inlet |
| in1 | Right inlet |
| out0 | Left outlet |
| out1 | Right outlet |

### Port details

**`in0` (Left inlet):** Gets all command messages described below.

**`in1` (Right inlet):** Gets object id message

 id
 nn

 to select the object to operate upon. In response to the id message,
 the current value of the property, if a property was already selected,
 is sent out the left outlet.

 id 0
 means no object, i.e. all messages to the left inlet are ignored, which
 is also the initial state.

**`out0` (Left outlet):** Sends the current value of the selected property of the selected
 object. The value type depends on the property, as described in the Live
 Object Model, and may be int, float, symbol,

 id
 nn

 or lists of ids.

**`out1` (Right outlet):** Sends responses to
 getproperty
 ,
 gettype
 ,
 getid
 .

## Arguments

- **property** (`symbol`) _(optional)_ — Initial Property
  Specify a property or child name as argument to
 live.observer
 .

## Messages

- `property(property: symbol)` — Arguments:

 property

 the name of a property of the current object

 Operation:

 Selects the property to be observed.
  Arguments:
  property
  the name of a property of the current object
  Operation:
  Selects the property to be observed. Outputs the current value to the
  left outlet if a proper Live object is selected.
  Remark:
  Not all properties can be observed.
  The types of the properties are given in the Live Object Model.
  Outlet
  Output
  Example
  left
  value
  3.1415926
- `property(child: symbol)` — Arguments:

 child

 the name of a child of the current object

 Operation:

 Selects the child id to be observed. Outputs the id (or "id 0")
  Arguments:
  child
  the name of a child of the current object
  Operation:
  Selects the child id to be observed. Outputs the id (or "id 0") to the
  left outlet if the selected Live object has such a child.
  Remark:
  Not all children can be observed.
  Outlet
  Output
  Example
  left
  id
  nn
  id 17
- `property(list-child: symbol)` — Arguments:

 child

 the name of a child list of the current object

 Operation:

 Selects the child list to be observed. Outputs the id list (or nothing)
  Arguments:
  child
  the name of a child list of the current object
  Operation:
  Selects the child list to be observed. Outputs the id list (or nothing)
  to the left outlet if the selected Live object has such a list child.
  Remark:
  Not all child lists can be observed.
  Outlet
  Output
  Example
  left
  id
  nn
  ... id
  mm
  id 4 id 5
- `getproperty` — Operation:

 Sends the name of the selected property (or child resp. list-child)
  Operation:
  Sends the name of the selected property (or child resp. list-child) out
  the right outlet.
  Outlet
  Output
  Example
  right
  property
  property
  or
  property
  child
  property name or
  property selected_track
- `gettype` — Operation:

 Sends the type of currently observed property or child to the right
 outlet.
  Operation:
  Sends the type of currently observed property or child to the right
  outlet. The types of the properties and children are given in the Live
  Object Model.
  For list-children it just sends
  type tuple
  , w/o further type information.
  Outlet
  Output
  Example
  right
  type
  property-type
  or
  type
  object-type
  type int
  or
  type Track
- `getid` — Operation:

 Sends the id of the currently observed Live object to the right outlet.
  Operation:
  Sends the id of the currently observed Live object to the right outlet.
  Outlet
  Output
  Example
  right
  id
  nn
  id 20
- `bang` — Operation:

 Sends current value of selected property of current object to the left
 outlet.
  Operation:
  Sends current value of selected property of current object to the left
  outlet. Does nothing if no property or no Live object is selected or if
  they don't match.
  Outlet
  Output
  Example
  left
  value
  Drums
- `id nn` — Operation:

 Sets the current object.
  Operation:
  Sets the current object. The message has the same effect if sent to
  both the right or the left inlet. For clarity it is suggested to always
  use the right inlet to supply the object id.
  - no output -

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — Right outlet

### basic

```
Example — [live.observer]  notify when the value changes
  fan-in:
    in0 ← [message "property value"]
    in1 ← [t b l l] ← [live.path] ← [loadmess path live_set tracks 0 mixer_device volume]
  fan-out:
    out0 → [message "set $1"]:in0
```

## See also

`JS API`, `m4l/live_api_overview`, `m4l/live_api`, `Live Object Model`, `live.path`, `live.object`, `live.remote~`
