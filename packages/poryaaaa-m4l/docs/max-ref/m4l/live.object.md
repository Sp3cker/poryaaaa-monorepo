# live.object

_m4l · Live API Objects_

> Perform operations on Live objects

live.object is used to perform operations on Live objects that have been selected using the live.path object. These operations include retrieving information on the current state of the Live API and setting values to control Live.

 Note: The Live API runs in the main thread in Live, and all messages to and from the API are automatically deferred.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Left inlet |
| in1 | Right inlet |
| out0 | Left outlet |

### Port details

**`in0` (Left inlet):** Gets all command messages described below.

**`in1` (Right inlet):** Gets object id message id nn to select the object to operate upon.

 id 0 means no object, i.e. all messages to the left inlet are ignored, which is also the initial state.

**`out0` (Left outlet):** Sends responses to get, call, bang, getid, getinfo, gettype and getpath.

## Messages

- `getid` — Report the current object's id
  The current object's id is sent from the outlet, preceded by the word id. If there is no current object, id 0 will be sent.
- `id nn` — Set the current object
  Operation:
  Sets the current object. The message has the same effect if sent to both the right or the left inlet. For clarity it is suggested to always use the right inlet to supply the object id.
  - no output -
- `getpath` — Send the canonical path of current object
  Operation:
  Sends the canonical path of current object.
  Outlet
  Output
  Example
  left
  path path
  path live_set return_tracks 0
- `gettype` — Send the type (class)
  Operation:
  Sends the type (a.k.a. class) of the current object.
  Outlet
  Output
  Example
  left
  type object-type
  type Song
- `getinfo` — Send a description of the current object
  Operation:
  Sends a description of the current object.
  Output to left outlet (most lines may occur multiple times, last line is info done):
  info id nn
  info type object-type
  info description description
  info children list-child object-type
  info child child object-type
  info property property property-type
  info function function
  info done
  Example output:
  info id 3
  info type Scene
  info description This class represents a series of ClipSlots in Lives session view matrix
  info children clip_slots ClipSlot
  info child canonical_parent Song
  info property is_triggered bool
  info property name symbol
  info property tempo float
  info function fire
  info function fire_as_selected
  info function set_fire_button_state
  info done
- `bang, getid` — Sends the id of the current Live object to the outlet
  Operation:
  Sends the id of the current Live object to the outlet.
  Outlet
  Output
  Example
  left
  id nn
  id 5
- `call(function: symbol, [parameter-list: list of different types])` — Call the given function of the current object (optional parameter list)
  Arguments:
  function the name of the function
  parameter-list an optional list of parameters
  Operation:
  Calls the given function of the current object, optionally with a list of parameters.
  Remark:
  The types of the parameters are given in the Live Object Model.
  Outlet
  Output
  Example
  left
  functionvalue
  get_beats_loop_length 004.00.00.000
- `set(list-child: symbol, id nn ... id mm: id-list)` — Set the list child to contain the given ids
  Arguments:
  list-child the name of a list child of the current object
  id nn... id mm the new list of objects for the given name
  Operation:
  Remark:
  Not all children can be set.
  - no output -
- `set(child: symbol, id nn: id)` — Set the child name to point to the given child
  Arguments:
  child the name of a child of the current object
  id nn the new child object for this name
  Operation:
  Remark:
  Not all children can be set.
  - no output -
- `set(list-property: symbol, value-list: various types)` — Set the given list property to the value list
  Arguments:
  list-property the name of a list property of the current object
  value-list the new values for the property
  Operation:
  Remark:
  Not all properties can be set. The types of the properties are given in the Live Object Model.
  - no output -
- `set(property: symbol, value: various types)` — Set the value of given property of the current object
  Arguments:
  property the name of a single-value property of the current object
  value the new value for the property
  Operation:
  Remark:
  Not all properties can be set. The types of the properties are given in the Live Object Model.
  - no output -
- `get(list-child: symbol)` — Send the ids of the elements of the list-child of the current object
  Arguments:
  list-child the name of a list-child of the current object
  Operation:
  Sends the ids of the elements of the list-child of the current object.
  Outlet
  Output
  Example
  left
  list-child id nn... id mm
  clip_slots id 4 id 5
- `get(child: symbol)` — Send the id of the child of the current object
  Arguments:
  child the name of a child of the current object
  Operation:
  Sends the id of the child of the current object.
  Outlet
  Output
  Example
  left
  child id nn
  master_track id 10
- `get(list-property: symbol)` — Send the list of values of given property of the current object
  Arguments:
  list-property the name of a list property of the current object
  Operation:
  Sends the list of values of given property of the current object.
  Outlet
  Output
  Example
  left
  list-propertylist of values
  input_routings Ext. In Max Resampling 1-Audio A-Return Master No Input
- `get(property: symbol)` — Output the value of a property of the current object
  Arguments:
  property the name of a single-value property of the current object
  Operation:
  Sends value of given property of the current object.
  Outlet
  Output
  Example
  left
  propertyvalue
  name base solo 3

## Help patcher examples

### basic

```
Example — [live.object]
  fan-in:
    in0 ← [message "getinfo"]
    in0 ← [message "call get_major_version"]
    in1 ← [t b l] ← [live.path] ← [loadmess path live_app]
  fan-out:
    out0 → [route get_major_version]:in0
```

## See also

`JS API`, `m4l/live_api_overview`, `m4l/live_api`, `Live Object Model`, `live.path`, `live.observer`, `live.remote~`
