# jit.gl.texture

_jit · Jitter OpenGL_

> Create OpenGL textures

Creates OpenGL textures - buffers of image data used in drawing 3D geometry. jit.gl.texture is similar to jit.matrix except that textures reside on the graphics card.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | matrix output if enabled |
| out1 | dumpout |

## Messages

- `begin_capture` — Start texture capture
- `bind` — Bind the texture
- `end_capture` — End texture capture
- `read(filename: symbol)` — Load image into texture
- `subtex_matrix` — Submit a matrix as a subtexture
  Submit a matrix as a subtexture. See the
  subtex.3d
  example patch for a demonstration.
- `tomatrix(name: symbol)` — Update the named jit.matrix with the texture
- `unbind` — Unbind the texture

## GUI behaviors

- `(drag)` — Load a compatible media file
  Drag and drop a compatible media file onto the object to load it.

## Attributes

- `@basic` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `clear` — seen as: `clear`
- `dstdimstart` — seen as: `dstdimstart 80 60, dstdimend 159 119`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### subtex_matrix

> the subtex_matrix message is used in conjunction with the @usedstdim and @usesrcdim attributes. for dynamic control of texture placement, the td.rota.jxs shader is recommended. see the file subtex.3d.maxpat for a demonstration of using the subtex_matrix message to create 3D textures

```
Example — [jit.gl.texture @usedstdim 1]
  fan-in:
    in0 ← [message "dstdimstart 80 60, dstdimend 159 119"]
    in0 ← [t l b] ← [substitute jit_matrix subtex_matrix] ← [jit.playlist]
    in0 ← [jit.matrix 4 char 160 120] ← [message "0"]
  fan-out:
    out0 → [jit.gl.videoplane @transform_reset 2]:in0
```

### readback

> jit.gl.slab outputs a jit.gl.texture with the message jit_gl_texture <texture name>

### basic

```
Example #1 — [jit.gl.texture tex-ctx @name tex2]
  fan-in:
    in0 ← [message "clear"]
    in0 ← [attrui @defaultimage]
    in0 ← [jit.matrix] ← [message "importmovie chilis.jpg, bang"]
```

```
Example #2 — [jit.gl.texture tex-ctx @name tex1]
  fan-in:
    in0 ← [jit.playlist]
```

Attributes demonstrated: `@defaultimage`, `@gl_color`, `@lighting_enable`, `@output_texture`

## See also

`jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.videoplane`, `jit.gl.volume`
