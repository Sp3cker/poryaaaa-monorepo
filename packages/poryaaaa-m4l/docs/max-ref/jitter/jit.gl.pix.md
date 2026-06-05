# jit.gl.pix

_jit · Jitter Code Generation_

> Generates pixel processing shaders from a gen patcher

The jit.gl.pix object generates new pixel processing shaders from a patcher. jit.gl.pix is essentially a jit.gl.slab object whose shader files are generated from Gen patchers.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| in1 |  |
| out0 | matrix output if enabled |
| out1 | dumpout |

## Messages

- `anything` — Set parameter values in the Gen patcher
- `compile` — Compile the Gen patcher
- `exportcode([target: symbol])` — Export gen patcher as shader code.
  Export a gen patcher as shader code. With no arguments, jit.gl.pix will export a standard Jitter JXS shader file with GLSL code. The optional arguments can be either 'jxs' or 'webgl' or 'isf'.
  In the 'webgl' case, jit.gl.pix will export an .html file suitable for loading in a
  WebGL context
  . The code uses
  TWGL
  as a helper library and requires the
  twgl-full.min.js
  file be in the same directory as the exported html file.
  In the 'isf' case, jit.gl.pix will export a .fs fragment shader suitable for loading in a
  ISF (Interactive Shader Format) context
- `full_source_code` — Opens an editor window with generated source code
  Opens an editor window with the generated source code formatted as a JXS file.
- `getparamdefault` — Sends the default data values for the indicated shader parameter for the internal jit.gl.shader object out the right-most outlet.
- `getparamdescription` — Get a parameter description
- `getparamlist` — Sends the names of all the internal jit.gl.shader object shader parameters out the right-most outlet.
- `getparamtype` — Sends the name of the datatype for the indicated shader parameter for the internal jit.gl.shader object out the right-most outlet.
- `getparamval` — Sends the data values for the indicated shader parameter for the internal jit.gl.shader object out the right-most outlet.
- `open` — Open the Gen patcher window
- `param` — Sets the given shader parameter with the given atom values as defined in a JXS (Jitter shader)
  Sets the given shader parameter with the given atom values as defined in a JXS (Jitter shader) file.
- `sendinput([index: int], message: symbol, [values: list])` — Sends an input texture a message
  Sends an input jit.gl.texture a message. If the first arg is an int, it specifies the texture index to send the message, otherwise all input textures receive the message.
- `sendoutput(message: symbol, [values: list])` — Sends the output textures a message
  Sends the output jit.gl.texture objects a message.
- `sendshader(message: symbol, [values: list])` — Sends the internal shader a message
  Sends the internal jit.gl.shader object a message
- `wclose` — Close the Gen patcher

## GUI behaviors

- `(drag)` — Drag and drop a .genjit Gen patcher
- `(mouse)` — Double-click to open gen patcher window

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### exportcode

```
Example — [jit.gl.pix gl-pix-help]
  fan-in:
    in0 ← [r gl-pix-vid]
    in0 ← [message "exportcode $1"]
  fan-out:
    out0 → [jit.gl.layer gl-pix-help @scale 0.5 0.5 0.5 @position -0.7 0 0 @enable 0 @layer 1]:in0
```

Attributes demonstrated: `@enable`

### basic

```
Example — [jit.gl.pix]
  fan-in:
    in0 ← [attrui @hue_shift]
    in0 ← [jit.playlist]
  fan-out:
    out0 → [jit.gl.layer]:in0
    out0 → [jit.fpsgui]:in0
```

Attributes demonstrated: `@hue_shift`, `@output_texture`

## See also

`gen/gen_common_operators`, `gen/gen_genexpr`, `gen/gen_jitter_operators`, `gen/gen_overview`, `jit.gen`, `jit.pix`, `jit.gl.slab`, `jit.expr`, `jit.matrix`, `gen~`
