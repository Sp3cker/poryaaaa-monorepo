# jit.gl.plato

_jit · Jitter OpenGL_

> Generate platonic solids

Produces one of five platonic solids: tetrahedron, cube, octahedron, dodecahedron, or icosahedron.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | messages to this 3d object |
| out0 | disabled | matrix output if enabled |
| out1 | disabled | dumpout |

## GUI behaviors

- `(drag)` — Drag and drop a Jitter material file

## Attributes

- `@shape` (int) — Platonic shape
  The Platonic solid to be drawn (default = 1 (tetrahedron)) The solid may by specified by name. The allowable names are tetrahedron, cube, octahedron, dodecahedron, or icosahedron.
 The solid may alternately be specified by number:

 1 = tetrahedron

 2 = cube

 3 = octahedron

 4 = dodecahedron

 5 = icosahedron
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled
> - `out1` — dumpout

### basic

```
Example — [jit.gl.plato plato-ctx @color 1. 0.8 0.6 1. @scale 0.5 0.5 0.5 @lighting_enable 1]
  fan-in:
    in0 ← [jit.gl.handle plato-ctx @auto_rotate 1] ← [message "reset"]
    in0 ← [attrui @shape]    # Change shape by number, name or nickname
    in0 ← [attrui @poly_mode]
```

Attributes demonstrated: `@poly_mode`, `@shape`

## See also

`jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.videoplane`, `jit.gl.volume`
