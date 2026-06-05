# jit.gl.node

_jit · Jitter OpenGL_

> Create hierarchical rendering groups

Use jit.gl.node to construct hierarchical rendering groups. jit.gl.node creates sub-contexts of child objects that can be modified, rendered, and captured together as a functional group.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | texture output if capture enabled |
| out1 | connect to 3d objects to draw to this gl.node |
| out2 | dumpout |

## Messages

- `draw` — Draw the node
- `getscene_dict([attribute mode: symbol])` — Outputs a dictionary of scene elements
  Outputs a dictionary of scene elements out the dumpout prepended by the symbol "scene_dict". The optional argument specifies the attribute mode. If no argument is supplied only the spatial transform attributes are included.
  If the arg all_attrs is supplied all object attributes are included.
  If the arg modified_attrs is supplied any attributes with modified state are included.
- `sendoutput(message: symbol, [values: list])` — Send messages to the internal texture objects used for capture output

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `capture` — seen as: `capture $1`
- `enable` — seen as: `enable $1`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out2` — dumpout

### multi-node

> jit.gl.node

```
Example #1 — [jit.gl.node @name raster @position 0. 0. 0. @scale 2. 2. 2. @color 1. 1. 0. 1. @capture 1]  Capture is enabled
  fan-in:
    in0 ← [jit.gl.node node-ctx @enable 0]
  fan-out:
    out0 → [jit.gl.asyncread]:in0
```

```
Example #2 — [jit.gl.node @name right @position 0.5 0. 0.]
  fan-in:
    in0 ← [attrui @color]
    in0 ← [jit.gl.node node-ctx @enable 0]
```

```
Example #3 — [jit.gl.node @name left @position -0.5 0. 0.]
  fan-in:
    in0 ← [attrui @color]
    in0 ← [jit.gl.node node-ctx @enable 0]
```

```
Example #4 — [jit.gl.node node-ctx @enable 0]
  fan-in:
    in0 ← [message "enable $1"]
  fan-out:
    out1 → [jit.gl.node @name left @position -0.5 0. 0.]:in0
    out1 → [jit.gl.node @name right @position 0.5 0. 0.]:in0
    out1 → [jit.gl.node @name raster @position 0. 0. 0. @scale 2. 2. 2. @color 1. 1. 0. 1. @capture 1]:in0    # Capture is enabled
```

Attributes demonstrated: `@color`

### multiples

> jit.gl.node

```
Example #1 — [jit.gl.node @enable 0]
  fan-in:
    in0 ← [message "enable $1"]
  fan-out:
    out1 → [jit.gl.multiple 3 @glparams position rotatexyz scale]:in0
```

```
Example #2 — [jit.gl.node @position 0. 0.2 0.4 @color 1 1 1 1]
  fan-in:
    in0 ← [jit.gl.node]
  fan-out:
    out1 → [jit.gl.gridshape @shape torus @scale 0.1 0.1 0.1 @position 0.25 0. 0.]:in0
    out1 → [jit.gl.gridshape @shape torus @scale 0.1 0.1 0.1 @position -0.25 0. 0.]:in0
```

```
Example #3 — [jit.gl.node]
  fan-in:
    in0 ← [jit.gl.multiple 3 @glparams position rotatexyz scale]
  fan-out:
    out1 → [jit.gl.gridshape @shape sphere @scale 0.15 0.5 0.5 @color 0 0 0 1 @position 0 0.25 -0.25]:in0
    out1 → [jit.gl.gridshape @shape sphere @scale 0.15 0.15 0.4 @position 0. 0. 0.5 @color 0.5 0.8 0.8 1]:in0
    out1 → [jit.gl.gridshape @shape sphere @scale 0.5 0.5 0.5 @color 1 0.5 0.3 1]:in0
    out1 → [jit.gl.node @position 0. 0.2 0.4 @color 1 1 1 1]:in0
```

### hierarchy

> jit.gl.node

> Create hierarchical groups of GL objects with jit.gl.node. This is useful for organizing complex geometry scenes as well as managing render state

```
Example #1 — [jit.gl.node @position 0. 0.2 0.4]  This node is a child of the node above
  fan-in:
    in0 ← [jit.gl.node @enable 0]
    in0 ← [attrui @rotatexyz]    # jit.gl.node can override color and other attributes for children / Try adjusting the transforms of the 2 nodes to see how they relate.
    in0 ← [attrui @gl_color]
  fan-out:
    out1 → [jit.gl.gridshape @shape torus @scale 0.1 0.1 0.1 @position 0.25 0. 0.]:in0
    out1 → [jit.gl.gridshape @shape torus @scale 0.1 0.1 0.1 @position -0.25 0. 0.]:in0
```

```
Example #2 — [jit.gl.node @enable 0]
  fan-in:
    in0 ← [attrui @position]
    in0 ← [attrui @rotatexyz]
    in0 ← [message "enable $1"]
  fan-out:
    out1 → [jit.gl.gridshape @shape sphere @scale 0.15 0.15 0.1 @position 0. 0. 0.5 @color 1 0.8 0.8 1]:in0
    out1 → [jit.gl.gridshape @shape sphere @scale 0.5 0.5 0.5 @color 1 0 1 1]:in0
    out1 → [jit.gl.node @position 0. 0.2 0.4]:in0    # This node is a child of the node above
```

Attributes demonstrated: `@gl_color`, `@position`, `@rotatexyz`

### basic

```
Example #1 — [jit.gl.node]
  fan-in:
    in0 ← [message "enable $1"]
  fan-out:
    out1 → [jit.gl.node @scale 0.75 0.75 0.75]:in0    # connect objects to middle outlet to include in the node group
    out1 → [jit.gl.videoplane @enable 0]:in0
```

```
Example #2 — [jit.gl.node @scale 0.75 0.75 0.75]  connect objects to middle outlet to include in the node group
  fan-in:
    in0 ← [attrui @dim]
    in0 ← [attrui @type]
    in0 ← [message "capture $1"]
    in0 ← [jit.gl.node]
    in0 ← [attrui @position]
  fan-out:
    out0 → [jit.gl.videoplane @enable 0]:in0
    out1 → [jit.gl.gridshape @color 1 0.2 0.2 1 @scale 0.3 0.3 0.3]:in0
    out1 → [jit.gl.gridshape @shape torus]:in0
```

Attributes demonstrated: `@dim`, `@position`, `@type`

## See also

`jit.gl.gridshape`, `jit.gl.render`, `jit.gl.texture`, `jit.gl.multiple`
