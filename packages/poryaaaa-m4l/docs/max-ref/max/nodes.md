# nodes

_max · U/I_

> Interpolate data graphically

The nodes object displays overlapping circular regions ("nodes") and outputs a list of interpolated weights based on the distance between a slider position and each node's center point. You can drag the slider with the mouse or set its position with a list of coordinates.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | list of 2 (0.-1.) floats gives x,y for interpolation |
| out0 | list | Interpolated weights for each active node |
| out1 | list | dumpout |
| out2 | list | Mouse Information (x, y, button) |

## Messages

- `bang` — Outputs the nodes values
  Outputs the nodes values out the second outlet.
- `list(x: number, y: number)` — Set the position of the slider
  Sets the position of the slider (if present) and outputs the new coefficients.
- `active(index: int, state: int)` — Set the active state
  Sets the active state (0 disabled, 1 enabled) of the node(s). Index numbering for the nodes starts at 1. When the index is set to 0, it affects every node.
- `ad(azimuth: float, distance: float)` — Set the position of the slider
  Sets the position of the slider (if present) in polar coordinates (azimuth in degrees, distance) and outputs the new coefficients.
- `clear` — TEXT_HERE
- `getactive([index: int])` — Report the active state
  The word getactive causes the nodes object to send a list of the active states of every node out the dumpout outlet. An optional argument allows you to query the active state of a specific node.
- `getad` — Report the slider polar coordinates
  The word getad causes the nodes object to send a list of polar coordinates of the slider out the dumpout outlet.
- `getnode([index: int])` — Report node information
  The word getnode causes the nodes object to send a list of the values of the node {x, y, size, active} of every node out the dumpout outlet. An optional argument allows you to query a specific node.
- `getsize([index: int])` — Report node size information
  The word getsize causes the nodes object to send a list of the values of the node size of every node out the dumpout outlet. An optional argument allows you to query a specific node.
- `getxy` — Report node cartesian coordinates
  The word getxy causes the nodes object to send a list of cartesian coordinates of the slider out the dumpout outlet.
- `setnode(index: int, x: float, y: float, [size: float], [active: int])` — Sets the position of a node
  Sets the position of the node(s). Index numbering for the nodes starts at 1. When the index is set to 0, it affects every node. You can also provide additional arguments to define the size and active state.
- `setnodead(index: int, azimuth: float, distance: float, [size: float], [active: int])` — Sets the position of a node
  Sets the position of the node(s) in azimuth (angle in degree) and distance. Index numbering for the nodes starts at 1. When the index is set to 0, it affects every node. You can also provide additional arguments to define the size and active state.
- `setnodename(nodename: list)` — Sets the display name for a given node
  Sending the message setnodename followed by an integer and a symbol changes the displayed name for that node from its index value to the specified symbol.
- `setsize(index: int, size: float)` — Set the size of a node
  Sets the size of the specified node. Index numbering for the nodes starts at 1. When the index is set to 0, it affects every node.

## GUI behaviors

- `(mouse)` — Manipulate nodes or the slider
  You can manipulate the nodes or the slider depending on the
  displayknob
  and
  mousemode
  attributes.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `mousemode` — seen as: `mousemode $1`
- `nodesnames` — seen as: `nodesnames 1 2 3 4 5`, `nodesnames U V X Y Z`, `nodesnames cats dogs mice ducks llamas`

## Help patcher examples

### pattrstorage

> Nodes list output works well with "recallmulti" messages, providing a powerful way to morph between different presets.

> Pattrstorage supports the recall of multiple presets at the same time, using decimal values as weights. This allows for smooth transitions between multiple stored patcher states. Try moving the knob around in the nodes object above and watch the UI update.

```
Example — [nodes]  scrub through the presets
  fan-out:
    out0 → [vexpr ($f1*0.9999)+$f2]:in0
```

### dump-get

> ...

```
Example — [nodes]
  fan-in:
    in0 ← [message "getsize 3"]    # output the size of every nodes
    in0 ← [message "getnode 0"]    # output nodes
    in0 ← [message "getad"]    # output the slider position (polar)
    in0 ← [message "getnode 3"]    # output third node
    in0 ← [message "getsize 0"]
    in0 ← [button]
    in0 ← [message "getactive 3"]    # output the active state of the third node
    in0 ← [message "getactive 0"]    # output the active state of all the nodes / output the size of the third node
    in0 ← [message "getxy"]    # output the slider position (cartesian)
  fan-out:
    out0 → [multislider]:in0
    out1 → [route node xy ad size active]:in0
```

### preset connection

> this connection enables the automatic nodes + preset interaction

```
Example — [nodes]  use nodes to scrub through the presets
  fan-out:
    out0 → [preset]:in0    # shift-click to add a new preset in slot 6 and observe that a new node is added
```

### appearance

```
Example #1 — [nodes]
  fan-in:
    in0 ← [message "nodesnames U V X Y Z"]
    in0 ← [message "nodesnames cats dogs mice ducks llamas"]    # set custom display names
    in0 ← [message "nodesnames 1 2 3 4 5"]
```

```
Example #2 — [nodes]
  fan-in:
    in0 ← [attrui @textcolor]
    in0 ← [attrui @candycane2]
    in0 ← [attrui @candycane3]
    in0 ← [attrui @candycane4]
    in0 ← [attrui @candycane5]
    in0 ← [attrui @candycane6]
    in0 ← [attrui @candycane7]
    in0 ← [attrui @candycane8]
    in0 ← [attrui @candycane]    # set the number of different colors used for the nodes
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @fontsize]
    in0 ← [attrui @pointcolor]
    in0 ← [attrui @displayknob]
    in0 ← [attrui @nodecolor]
```

Attributes demonstrated: `@bgcolor`, `@candycane`, `@candycane2`, `@candycane3`, `@candycane4`, `@candycane5`, `@candycane6`, `@candycane7`, `@candycane8`, `@displayknob`, `@fontsize`, `@nodecolor`, `@pointcolor`, `@style`, `@textcolor`

### fun

```
Example — [nodes]
  fan-in:
    in0 ← [message "mousemode $1"]
    in0 ← [pak 0. 0.]
  fan-out:
    out0 → [vexpr sqrt($f1)]:in0
    out2 → [route mouse]:in0
```

### messages

```
Example — [nodes]
  fan-in:
    in0 ← [pak active 4 0]    # active state controls whether the node is visible
    in0 ← [pak setnodead 2 -45 0.3]    # setnodead uses polar coordinates to set the location of the node
    in0 ← [pak setsize 3 0.3]    # set the size of the node
    in0 ← [pak setnode 1 0.5 0.5 0.2]    # setnode is used to change the location and size of the specified node
```

### basic

> Output: Left outlet is a list of interpolation weights. Middle outlet reports node positions and sizes in the form [query, node number, x-position, y-position, size] with various 'get' messages such as "getxy" or "getsize". These lists can be parsed for use in other contexts (see dump-get subpatcher). Right outlet reports mousing information inside the nodes patching rectangle.

```
Example — [nodes]  Click and drag the cross to position the slider. Click and drag center of node to position a node. Option Key enables resizing of node with mousedrag.
  fan-in:
    in0 ← [pak 0. 0.]
    in0 ← [attrui @nodenumber]
  fan-out:
    out0 → [multislider]:in0    # interpolated weight of each node / 9 / 8 / 7 / 6 / 5 / 4 / 3 / 2 / 1
    out1 → [route node]:in0
    out2 → [message ""]:in1
```

Attributes demonstrated: `@nodenumber`

## See also

`multislider`, `pictslider`, `pattrstorage`
