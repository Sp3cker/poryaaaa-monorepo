# jit.gl.cornerpin

_jit · Jitter OpenGL_

> Map textures in a window

Provides controls for mapping textures and matrices to an output window by repositioning the image corners. Mouse input is received from the context window allowing for easy manipulation of corner positions.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | matrix output if enabled |
| out1 | dumpout |

## Messages

- `reset` — Reset corner position attributes
- `sendtexture(name: symbol, message: symbol, [values: list])` — Send the output texture a message

## Attributes

- `@colormode` (symbol) — Matrix color mode
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled
> - `out1` — dumpout

### basic

```
Example — [jit.gl.cornerpin @corner_radius 50]
  fan-in:
    in0 ← [attrui @upper_left]
    in0 ← [message "reset"]
    in0 ← [attrui @upper_right]
    in0 ← [attrui @lower_left]
    in0 ← [attrui @lower_right]
    in0 ← [attrui @enable_mouse]
    in0 ← [attrui @corner_radius]
    in0 ← [attrui @interp]
    in0 ← [attrui @cornermode]
    in0 ← [attrui @drawcorners]
    in0 ← [jit.noise 4 char 80 60] ← [loadbang]
    in0 ← [attrui @preserve_aspect]
```

Attributes demonstrated: `@corner_radius`, `@cornermode`, `@drawcorners`, `@enable_mouse`, `@interp`, `@lower_left`, `@lower_right`, `@preserve_aspect`, `@upper_left`, `@upper_right`

## See also

`jit.gl.render`, `jit.gl.videoplane`, `jit.gl.texture`, `jit.gl.slab`, `jit.gl.skybox`
