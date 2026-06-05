# jit.gl.volume

_jit · Jitter OpenGL_

> Create a volume visualization

Creates a transparent volume from a volumetric density field. This process is GL accelerated by using graphics hardware.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | messages to this 3d object |
| out0 | disabled | matrix output if enabled |
| out1 | disabled | dumpout |

## Attributes

- `@basic` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled
> - `out1` — dumpout

### basic

```
Example — [jit.gl.volume vol-ctx]
  fan-in:
    in0 ← [jit.gl.handle vol-ctx]
    in0 ← [attrui @axes]
    in0 ← [attrui @density]
    in0 ← [attrui @bounds]
    in0 ← [attrui @slices]
    in0 ← [attrui @intensity]
    in0 ← [jit.bfg 1 char 32 32 32 @basis fractal.turbulence @scale 5 5 5]
    in0 ← [attrui @depth_enable]
    in0 ← [attrui @scale]
    in0 ← [attrui @gl_color]
```

Attributes demonstrated: `@axes`, `@basis`, `@bounds`, `@density`, `@depth_enable`, `@gl_color`, `@intensity`, `@offset`, `@scale`, `@slices`

## See also

`jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.videoplane`
