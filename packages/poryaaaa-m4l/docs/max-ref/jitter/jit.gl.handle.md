# jit.gl.handle

_jit · Jitter OpenGL_

> Use mouse movement to control position/rotation

jit.gl.handle responds to mouse clicks and drags in the destination by generating rotate and position messages out its left outlet.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | attribute settings, reset |
| out0 | position/rotate to ob3d |
| out1 | dumpout |

## Messages

- `reset` — Reset to initial origin and rotation
  Returns jit.gl.handle and attached objects to the original viewing origin and undoes all rotation.

## Attributes

- `@label` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### autohandle

```
Example — [jit.gl.handle @hover 1]
  fan-in:
    in0 ← [attrui @hover]
    in0 ← [attrui @hilite_color]
    in0 ← [attrui @select_mode]
    in0 ← [attrui @auto_handle]
```

Attributes demonstrated: `@auto_handle`, `@hilite_color`, `@hover`, `@select_mode`

### basic

```
Example — [jit.gl.handle ctx]
  fan-in:
    in0 ← [attrui @radius]
    in0 ← [attrui @hover]
    in0 ← [attrui @auto_rotate]
    in0 ← [message "reset"]
  fan-out:
    out0 → [jit.gl.plato @shape 4 @scale 0.5 0.5 0.5]:in0
```

Attributes demonstrated: `@auto_rotate`, `@hover`, `@radius`

## See also

`jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.videoplane`, `jit.gl.volume`
