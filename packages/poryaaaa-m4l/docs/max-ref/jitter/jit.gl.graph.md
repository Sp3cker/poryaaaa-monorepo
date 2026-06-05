# jit.gl.graph

_jit · Jitter OpenGL_

> Graph floats into 3D space

Renders one-dimensional floating point data as a three-dimensional shape.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | messages to this 3d object |
| out0 | disabled | matrix output if enabled |
| out1 | disabled | dumpout |

## Attributes

- `@label` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled
> - `out1` — dumpout

### basic

```
Example — [jit.gl.graph graph-ctx]
  fan-in:
    in0 ← [jit.op @op * @val 1.]
    in0 ← [attrui @gl_color]
    in0 ← [attrui @circpoints]
    in0 ← [jit.gl.handle graph-ctx]
    in0 ← [attrui @lighting_enable]
    in0 ← [attrui @smooth_shading]
```

Attributes demonstrated: `@circpoints`, `@gl_color`, `@lighting_enable`, `@smooth_shading`

## See also

`jit.buffer~`, `jit.catch~`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.videoplane`, `jit.gl.volume`, `jit.graph`, `jit.plot`
