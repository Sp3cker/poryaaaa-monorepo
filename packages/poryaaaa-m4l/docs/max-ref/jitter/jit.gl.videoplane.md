# jit.gl.videoplane

_jit · Jitter OpenGL_

> Display video in OpenGL

Provides a quick way to show video in OpenGL. Unlike other Jitter OpenGL objects it also has video specific controls.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | messages to this 3d object |
| out0 | disabled | matrix output if enabled |
| out1 | disabled | dumpout |

## Messages

- `sendtexture(name: symbol, message: symbol, [values: list])` — Send the output texture a message

## Attributes

- `@colormode` (symbol) — Matrix color mode
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled
> - `out1` — dumpout

### jit.gl.layer

> jit.gl.layer is an alias for jit.gl.videoplane that instantiates with the following defaults to make layering images and videos easier: preserve_aspect 1 blend_enable 1 depth_enable 0

Attributes demonstrated: `@blend_enable`, `@depth_enable`, `@enable`, `@preserve_aspect`

### basic

```
Example — [jit.gl.videoplane vid-ctx]
  fan-in:
    in0 ← [jit.playlist]
    in0 ← [attrui @blend]
    in0 ← [attrui @blend_enable]
    in0 ← [attrui @transform_reset]
    in0 ← [attrui @preserve_aspect]
```

Attributes demonstrated: `@blend`, `@blend_enable`, `@erase_color`, `@output_texture`, `@preserve_aspect`, `@transform_reset`

## See also

`jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.volume`
