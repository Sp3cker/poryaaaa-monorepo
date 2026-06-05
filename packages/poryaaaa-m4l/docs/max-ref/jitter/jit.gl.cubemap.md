# jit.gl.cubemap

_jit · Jitter OpenGL_

> Manage a cubemap texture target

Maintains a cubemap texture target in an OpenGL context. It has 6 inputs -- one for each face of the cube. Cubemaps are typically used to map an environment for material effects such as reflection and refraction. When sent a texture to any inlet, jit.gl.cubemap adapts to the input type of the texture.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Positive X Matrix |
| in1 | Negative X Matrix |
| in2 | Positive Y Matrix |
| in3 | Negative Y Matrix |
| in4 | Positive Z Matrix |
| in5 | Negative Z Matrix |
| out0 | matrix output if enabled |
| out1 | dumpout |

### Port details

**`in0` (Positive X Matrix):** Sets the cube face on the positive X side of the cube.

**`in1` (Negative X Matrix):** Sets the cube face on the negative X side of the cube.

**`in2` (Positive Y Matrix):** Sets the cube face on the positive Y side of the cube.

**`in3` (Negative Y Matrix):** Sets the cube face on the negative Y side of the cube.

**`in4` (Positive Z Matrix):** Sets the cube face on the positive Z side of the cube.

**`in5` (Negative Z Matrix):** Sets the cube face on the negative Z side of the cube.

**`out0` (matrix output if enabled):** This outlet is not used by jit.gl.cubemap

**`out1` (dumpout):** Outputs the results of 'get' messages.

## Messages

- `bind` — Bind cubemap to texture geometry
  Sets binding of the cubemap to texture geometry.
- `equirect_matrix` — TEXT_HERE
- `panorama_matrix` — Load a panorama formatted matrix
  Load a matrix formatted in a cross shape containing every face of the cubemap. The panorama matrix layout is a cross shape holding the 6 faces of the cube. The faces are in a 4x3 grid where the rows are formatted as follows:
  ** py ** **
  nx pz px nz
  ** ny ** **
- `read` — Read in a panorama formatted image file.
- `unbind` — Unbind the cubemap
  Unbinds the cubemap after a bind operation.

## GUI behaviors

- `(drag)` — TEXT_HERE

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `file` — seen as: `file bokeh.cubemap.jpg, bang`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### material

```
Example — [jit.gl.cubemap cube-ctx]
  fan-in:
    in0 ← [message "file bokeh.cubemap.jpg, bang"]    # use cubemap image for an environment_texture
    in0 ← [prepend panorama_matrix] ← [jit.noise 4 char 10 10] ← [button]    # set all faces of cube with 'panorama_matrix' message
  fan-out:
    out0 → [prepend environment_texture]:in0    # set environment_texture of material
```

### basic

```
Example — [jit.gl.cubemap @name cmap]
  fan-in:
    in0 ← [jit.noise 4 char 1 1] ← [button]    # define each face individually
    in0 ← [message "read bokeh.cubemap.jpg"]    # read an image file in cubemap format
    in0 ← [prepend panorama_matrix] ← [jit.noise 4 char 10 10] ← [button]    # set all faces of cube with 'panorama_matrix' message
    in1 ← [jit.noise 4 char 1 1] ← [button]    # define each face individually
    in2 ← [jit.noise 4 char 1 1] ← [button]    # define each face individually
    in3 ← [jit.noise 4 char 1 1] ← [button]    # define each face individually
    in4 ← [jit.noise 4 char 1 1] ← [button]    # define each face individually
    in5 ← [jit.noise 4 char 1 1] ← [button]    # define each face individually
```

## See also

`jit.gl.texture`, `jit.gl.shader`, `jit.gl.material`, `jit.gl.pbr`, `jit.gl.skybox`, `jit.gl.environment`
