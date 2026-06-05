# live.path

_m4l · Live API Objects_

> Navigate to objects in the Live application

live.path is used to navigate to Live objects on which the live.object, live.observer and live.remote~ objects operate. The navigation is purely path-based and is independent of the objects currently present in Live (navigating to a nonexistent path will result in the message id 0 being sent out the left and middle outputs rather than an error message).

 Note: The Live API runs in the main thread in Live, and all messages to and from the API are automatically deferred.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Single inlet |
| out0 | Left outlet |
| out1 | Middle outlet |
| out2 | Right outlet |

### Port details

**`in0` (Single inlet):** Gets all command messages described below.

**`out0` (Left outlet):** Sends the message id nn in response to a goto, bang or getid message only. Use this outlet if you want to keep working with a particular object determined at goto or bang time, even if its position in Live changes.

 For example, consider a fresh Live set with two tracks, "1 Audio" as the leftmost track and "2 MIDI" to the right of it. If you navigate to the "2 MIDI" track (goto live_set tracks 1) and you create a new Audio track between "1 Audio" and "2 MIDI", your original MIDI track now would be at live_sets tracks 2. But since the id number of the MIDI track would stay the same and no new id is sent out to the left outlet, the live.xxx objects connected to this outlet keep working with the MIDI track, until you sent another goto.

**`out1` (Middle outlet):** Sends id nn whenever the id of the object at the current path changes (because the current path is changed or because the object at this place in Live has changed, for example. Use this outlet if you want to keep working with the same path, whatever object there might be. This outlet is very useful for things like live_set view detail_clip.

 Consider the example above. If the live.xxx objects would be connected to the middle outlet of live.path, then they would work with the newly created audio track.

 The spontaneous sending of object ids out of the middle outlet, i.e. without an inlet message causing it, but caused by a change in Live, is called a notification.

 Note: It is not possible to modify the Live set from such a notification.

**`out2` (Right outlet):** Sends responses to getpath, getchildren, getcount.

## Arguments

- **initial path** (`symbol`) _(optional)_ —
  Specify an initial path as argument to live.path, without any quotes.

## Messages

- `getid` — TEXT_HERE
- `getcount(child-name: symbol)` — Arguments:

 child-name is the name of a child of the object at the current path.
  Arguments:
  child-name is the name of a child of the object at the current path.
  Operation:
  Sends a count message to the right outlet, containing the name of the child and its number of entries.
  Remarks:
  The given child must be a list.
  Outlet
  Output
  Example
  right
  count child-name count
  count clip_slots 2
- `getchildren` — Operation:

 Sends a list of children of the object at the current path, if any, to the right outlet.
  Operation:
  Sends a list of children of the object at the current path, if any, to the right outlet.
  Remarks:
  The child names are the same names as used in the goto message.
  Outlet
  Output
  Example
  right
  children list-of-child-names
  children canonical_parent clip_slots
- `getpath` — Operation:

 Sends a path message with the current path to the right outlet.
  Operation:
  Sends a path message with the current path to the right outlet.
  Outlet
  Output
  Example
  right
  path path
  path live_set scenes 1
- `bang, getid` — Operation:

 Sends the id of the object at the current path to left and middle outlets.
  Operation:
  Sends the id of the object at the current path to left and middle outlets. Sends id 0 if there is no object at the current path.
  Outlet
  Output
  Example
  left
  id nn
  id 5
  middle
  id nn
  id 5
- `path(absolute-path: symbol)` — Same as goto but limited to absolute paths that start with a root object name like live_app, live_set, this_device or control_surfaces N .
- `goto(path: symbol)` — Arguments:

 path is an absolute path (starts with live_app, live_set or control_surfaces N)
  Arguments:
  path is an absolute path (starts with live_app, live_set or control_surfaces N) or a relative path, or up
  Operation:
  Navigates to given path and sends id of the object at that path out the left and middle outlets. If there is no object at the path, id 0 is sent.
  Remarks:
  You cannot go to a list property, only to one of its members.
  invalid: goto live_set scenes
  correct: goto live_set scenes 3
  Outlet
  Output
  Example
  left
  id nn
  id 5
  middle
  id nn
  id 5

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Left outlet
> - `out2` — Right outlet

### basic

```
Example — [live.path]
  fan-in:
    in0 ← [loadmess path live_set tracks 0 mixer_device volume]    # path to the volume slider of the first track
  fan-out:
    out1 → [t b l l]:in0
```

## See also

`JS API`, `m4l/live_api_overview`, `m4l/live_api`, `Live Object Model`, `live.object`, `live.observer`, `live.remote~`
