# jit.gl.bfg

__

> Procedural basis function texture generator

Generates OpenGL texture output from a library of procedural basis functions. The functions are processed on the graphics card as OpenGL GLSL shaders. The three categories of functions include noise, fractal and distorted.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | texture output 0 |
| out1 | dumpout |

## Messages

- `full_source_code` — Open an editor window containing the generated JXS shader code based on object state.
- `getparamdefault(name: symbol)` — Report shader parameter's default value
- `getparamdescription(name: symbol)` — Get a parameter description
- `getparamlist` — Report shader parameter names
- `getparamtype(name: symbol)` — Report shader parameter's type
- `getparamval(name: symbol)` — Report shader parameter's value
- `param(name: symbol, message: symbol, [values: list])` — Set a shader parameter value
- `sendinput([index: int], message: symbol, [values: list])` — Sends an input texture a message
  Sends an input jit.gl.texture object a message.
- `sendoutput(message: symbol, [values: list])` — Sends the output textures a message
  Sends the output jit.gl.texture object a message.
- `sendshader(message: symbol, [values: list])` — Sends the internal shader a message
  Sends the internal jit.gl.shader object a message

## Attributes

- `@invisible` (int)
- `@label` (symbol)
- `@obsolete` (int)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### distorted

> Distortion modes

```
Example — [jit.gl.bfg @basis distorted.2axis @enable 0]
  fan-in:
    in0 ← [jit.bang] ← [!= 1] ← [toggle]
    in0 ← [attrui @basis.inner]    # for distorted or disorted.2axis
    in0 ← [attrui @basis.outer]
    in0 ← [attrui @basis] ← [message "input.distorted.2axis"]    # distort input textues with input.distorted or input.distorted.2axis
    in0 ← [jit.grab @output_texture 1] ← [toggle]
    in0 ← [attrui @distortion]
    in0 ← [attrui @enable]
    in0 ← [attrui @zoom]
  fan-out:
    out0 → [jit.gl.layer @enable 0]:in0
```

Attributes demonstrated: `@basis`, `@basis.inner`, `@basis.outer`, `@distortion`, `@enable`, `@zoom`

### basic

```
Example — [jit.gl.bfg]
  fan-in:
    in0 ← [attrui @fractal_params]    # h, lacunarity, offset, gain
    in0 ← [attrui @colorize]
    in0 ← [attrui @time] ← [jit.time]    # use with jit.time to animate time attribute
    in0 ← [message "full_source_code"]
    in0 ← [jit.gl.texture @adapt 0]
    in0 ← [attrui @basis]
    in0 ← [attrui @voronoi_crackle]
    in0 ← [attrui @voronoi_jitter]
    in0 ← [attrui @palette]
    in0 ← [attrui @voronoi_shade]
    in0 ← [attrui @voronoi_smooth]
    in0 ← [attrui @voronoise_amt]
    in0 ← [attrui @zoom]
  fan-out:
    out0 → [jit.world]:in0
```

Attributes demonstrated: `@basis`, `@colorize`, `@enable`, `@fractal_params`, `@palette`, `@speed`, `@time`, `@voronoi_crackle`, `@voronoi_jitter`, `@voronoi_shade`, `@voronoi_smooth`, `@voronoise_amt`, `@zoom`

## See also

`jit.bfg`, `jit.gl.pix`, `jit.gl.shader`, `jit.gl.slab`, `jit.gl.texture`
