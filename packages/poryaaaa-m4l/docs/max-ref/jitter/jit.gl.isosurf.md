# jit.gl.isosurf

_jit · Jitter OpenGL_

> Generate a GL based surface extraction

Creates a geometric surface from a volumetric density field. The polygonization occurs at locations where the density values intersect the edges of cells inside a subdivided cartesian grid.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | messages to this 3d object |
| out0 | disabled | matrix output if enabled |
| out1 | disabled | dumpout |

## GUI behaviors

- `(drag)` — Accepts a jitmtl material file and applies it to the shape.

## Attributes

- `@autocolor` (symbol) — Autocolor mode
  The autocolor mode for calculating vertex colors (default = normal) Supported modes are:

 none

 normal (normal direction)

 sample (density values)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `dim` — seen as: `dim $1 $1 $1, bang`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled
> - `out1` — dumpout

### basic

```
Example — [jit.gl.isosurf @isolevel 0.1 @smooth_shading 1 @lighting_enable 1]
  fan-in:
    in0 ← [jit.clip] ← [jit.bfg 1 float32 32 32 32 @basis noise.voronoi @scale 3.5 3.5 3.5]
    in0 ← [message "dim $1 $1 $1, bang"]
    in0 ← [attrui @isolevel]    # surface intersection
    in0 ← [attrui @epsilon]    # delta between samples
    in0 ← [prepend mode] ← [umenu]    # spatial subdivision method
```

Attributes demonstrated: `@epsilon`, `@isolevel`

## See also

`jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.videoplane`, `jit.gl.volume`
