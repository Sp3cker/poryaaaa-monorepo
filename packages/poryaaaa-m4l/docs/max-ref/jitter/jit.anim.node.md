# jit.anim.node

_jit · Jitter Animation_

> Perform hierarchical transformation

The jit.anim.node object represents a transformation (position, orientation and scale) in 3D space. OpenGL objects bind to a jit.anim.node and receive position, rotate, and scale attributes. In addition, parent-child relationships can be established in a hierarchical transformation structure, where child jit.anim.node objects are transformed relative to their parents.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages in |
| out0 | bind objects or position/rotate/scale messages |
| out1 | dumpout |

## Messages

- `bang` — Output the current transform coordinates
  Output the current world transform coordinates, which is a concatenation of the local transform with the parent transform.
- `anim_grow(x: float, y: float, z: float)` — Scale along axis
  Scale the node along each axis by the amount provided.
- `anim_move(x: float, y: float, z: float)` — Move the node
  Move the node along the axis provided, relative to the space indicated by movemode.
- `anim_reset` — Reset the transformation
  Reset the local transformation attributes to their default values (position = 0 0 0, rotatexyz = 0 0 0, and scale = 1 1 1)
- `anim_turn(x: float, y: float, z: float)` — Rotate along axis
  Rotate the node along each axis by the amount provided, relative to the space indicated by turnmode.
- `concat` — Concatenate (multiply) the current transform with the passed in 4x4 matrix
  Concatenate (multiply) the current transform with the passed in 4x4 matrix. Local transform attributes are updated after the operation.
- `grow(x: float, y: float, z: float)` — See the anim_grow listing
- `localtoworld(x: float, y: float, z: float)` — Retrieve the world-space position from passed in local coordinates
  Retrieve the world-space position from passed in local coordinates. The local coordinates are relative to the current world-space transform.
- `localtoworld_quat(x: float, y: float, z: float, w: float)` — Retrieve the world-space quaternion from passed in local quaternion
  Retrieve the world-space quaternion from passed in local quaternion. The returned quaternion is the product of to the current world-space quaternion and the local quaternion.
- `move(x: float, y: float, z: float)` — See the anim_move listing
- `reset` — See the anim_reset listing
- `turn(x: float, y: float, z: float)` — See the anim_turn listing
- `update_node` — See the bang listing
- `worldtolocal(x: float, y: float, z: float)` — Retrieve the local-space position from passed in world coordinates
  Retrieve the local-space position from passed in world coordinates. The local coordinates are relative to the current world-space transform.
- `worldtolocal_quat(x: float, y: float, z: float, w: float)` — Retrieve the local quaternion from passed in world-space quaternion
  Retrieve the local quaternion from passed in world quaternion. The returned quaternion is the product of to the current inverse world-space quaternion and the world quaternion.

## Attributes

- `@label` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### basic

```
Example #1 — [jit.anim.node @position -4 0 0]
  fan-in:
    in0 ← [attrui @animmode]
    in0 ← [attrui @inherit_rotate]
    in0 ← [attrui @inherit_scale]
    in0 ← [jit.anim.node]
  fan-out:
    out0 → [jit.gl.gridshape node-ctx @shape torus @color 0 0 1]:in0    # child 2
```

```
Example #2 — [jit.anim.node @position 4 0 0]
  fan-in:
    in0 ← [attrui @animmode]
    in0 ← [attrui @inherit_rotate]
    in0 ← [attrui @inherit_scale]
    in0 ← [message "move 0 0 1"]
    in0 ← [jit.anim.node]
  fan-out:
    out0 → [jit.gl.gridshape node-ctx @shape torus @color 0 1 0]:in0    # child 1
```

```
Example #3 — [jit.anim.node]
  fan-in:
    in0 ← [message "turn 0 5 0"]
    in0 ← [message "grow 1 1 1"]
    in0 ← [message "move 0 0 1"]
    in0 ← [message "anim_reset"]
  fan-out:
    out0 → [jit.anim.node @position -4 0 0]:in0
    out0 → [jit.gl.gridshape node-ctx @shape torus @color 1 0 0]:in0
    out0 → [jit.anim.node @position 4 0 0]:in0
```

Attributes demonstrated: `@animmode`, `@inherit_rotate`, `@inherit_scale`

## See also

`jit.anim.drive`, `jit.anim.path`, `jit.gl.camera`
