# jit.gl.gridshape

_jit · Jitter OpenGL_

> Generate simple geometric shapes as a grid

Creates one of several simple shapes (sphere, torus, cylinder, opencyclinder, cube, opencube, plane, circle) laid out on a connected grid. These shapes may be either rendered directly, or sent out the leftmost outlet as a matrix of values.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | messages to this 3d object |
| out0 | disabled | matrix output if enabled |
| out1 | disabled | dumpout |

## GUI behaviors

- `(drag)` — Apply material to the shape
  Accepts a jitmtl material file and applies it to the shape.

## Attributes

- `@basic` (int)
- `@label` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### basic

```
Example — [jit.gl.gridshape @depth_enable 1 @smooth_shading 1 @lighting_enable 1 @scale 0.75]
  fan-in:
    in0 ← [attrui @dim]    # set dimensions of grid
    in0 ← [attrui @shape]    # for 'torus' only / choose shape
    in0 ← [attrui @rad_minor]
    in0 ← [attrui @matrixoutput] ← [t i i] ← [toggle]    # output matrix and draw mesh
    in0 ← [attrui @position]    # set basic OB3D attributes
  fan-out:
    out0 → [jit.gl.mesh @depth_enable 1 @smooth_shading 1 @lighting_enable 1 @poly_mode 1 1]:in0
```

Attributes demonstrated: `@dim`, `@enable`, `@matrixoutput`, `@position`, `@rad_minor`, `@shape`

## See also

`jit.gl.graph`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.multiple`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.videoplane`, `jit.gl.volume`
