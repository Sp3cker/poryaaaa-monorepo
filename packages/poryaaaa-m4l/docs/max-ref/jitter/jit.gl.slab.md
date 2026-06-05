# jit.gl.slab

_jit · Jitter OpenGL_

> Process texture data

Generate, process and combine images efficiently using fragment shaders. Develop custom texture effects for processing on the graphics card.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| in1 |  |
| out0 | matrix output if enabled |
| out1 | dumpout |

## Messages

- `anything` — TEXT_HERE
- `getparamdefault(name: symbol)` — Report slab shader parameter's default value
- `getparamdescription` — Get a parameter description
- `getparamlist` — Report slab shader parameter names
- `getparamtype(name: symbol)` — Report slab shader parameter's type
- `getparamval(name: symbol)` — Report slab shader parameter's value
- `param(name: symbol, message: symbol, [values: list])` — Set a slab shader parameter value
- `read(filename: symbol)` — Loads a JXS shader file from disk
  Loads the given JXS shader file from disk.
- `sendinput([index: int], message: symbol, [values: list])` — Sends an input texture a message
  Sends a message to an input texture. If the first arg is an int, it specifies the texture index to send the message, otherwise all input textures receive the message.
- `sendoutput(message: symbol, [values: list])` — Sends the output textures a message
  Sends the output jit.gl.texture objects a message.
- `sendshader(message: symbol, [values: list])` — Sends the internal shader a message
  Sends the internal jit.gl.shader object a message

## GUI behaviors

- `(drag)` — Load a shader file
  Dragging a JXS file from the Max File Browser or desktop to a jit.gl.slab object, will load the file.
- `(mouse)` — Double click to open the shader editor
  Double click to open the shader editor. If no file is loaded the editor will load a starter shader to use as the basis for a new shader. This file must be saved to disk for use after the editor is closed.

## Attributes

- `@introduced` (symbol)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### basic

```
Example #1 — [jit.gl.slab @file co.average.jxs]
  fan-in:
    in0 ← [jit.playlist]
    in0 ← [sprintf file co.%s.jxs] ← [umenu]    # change the compositing shader
    in0 ← [message "param amount $1"]
    in1 ← [jit.gl.slab @file td.kaleido.jxs @param div 6]
  fan-out:
    out0 → [jit.gl.layer]:in0
```

```
Example #2 — [jit.gl.slab @file td.kaleido.jxs @param div 6]
  fan-in:
    in0 ← [jit.playlist]
  fan-out:
    out0 → [jit.gl.slab @file co.average.jxs]:in1
```

Attributes demonstrated: `@output_texture`

## See also

`external_text_editor`, `jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.videoplane`, `jit.gl.volume`
