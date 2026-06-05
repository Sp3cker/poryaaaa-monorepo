# jit.gl.multiple

_jit · Jitter OpenGL_

> Create multiple object instances

Uses several jit.matrix objects to repeatedly draw an instance of a jit.gl object like jit.gl.mesh or jit.gl.gridshape. It attaches to a named instance of a jit.gl (OB3D) object provided by the targetname attribute.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | matrix | position array |
| in1 | matrix | scale array |
| out0 | — | bind gl objects |
| out1 | — | dumpout |

## Messages

- `color_matrix(matrix name: symbol)` — Set the color parameter
  Specifies a named matrix to use for the color parameter if it's in the glparams list. Must be a 4-plane float32 matrix in RGBA format.
- `position_matrix(matrix name: symbol)` — Set the position parameter
  Specifies a named matrix to use for the position parameter if it's in the glparams list. Must be a 3-plane float32 matrix.
- `rotate_matrix(matrix name: symbol)` — Set the rotation parameter
  Specifies a named matrix to use for the rotate (angle-axis) parameter if it's in the glparams list. Must be a 4-plane float32 matrix.
- `rotatexyz_matrix(matrix name: symbol)` — Set the rotatexyz matrix
  Specifies a named matrix to use for the rotatexyz parameter if it's in the glparams list. Must be a 3-plane float32 matrix.
- `scale_matrix(matrix name: symbol)` — Set the scale parameter
  Specifies a named matrix to use for the scale parameter if it's in the glparams list. Must be a 3-plane float32 matrix.
- `texture_matrix(matrix name: symbol)` — Set the texture parameter
  Specifies a named matrix to use for the texture parameter if it's in the glparams list. Must be a char matrix.

## GUI behaviors

- `(drag)` — Drag and drop a Jitter material file

## Attributes

- `@label` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### param-order

```
Example — [jit.gl.multiple 3 @glparams rotatexyz position scale]
  fan-in:
    in0 ← [jit.gen]
    in0 ← [jit.gl.handle]
    in0 ← [attrui @enable]
    in1 ← [jit.gen]
    in2 ← [jit.gen]
  fan-out:
    out0 → [jit.gl.gridshape @shape cube]:in0
```

Attributes demonstrated: `@enable`, `@offset`

### multitexture

```
Example — [jit.gl.multiple 3 @glparams position rotatexyz texture @enable 0]
  fan-in:
    in0 ← [jit.gl.polymovie @vol 0 @loop 1]
    in0 ← [p make-cube] ← [button] ← [sel 1]    # p make-cube emits: "1 0 0, 0 -1 0, -1 0 0, 0 1 0, 0 0 1, 0 0 -1" | "0 90 0, 90 0 0, 0 90 0, 90 0 0, 0 0 0, 0 0 0"
    in0 ← [jit.gl.handle @enable 0]
    in0 ← [attrui @enable] ← [routepass 1] ← [active]
    in1 ← [p make-cube] ← [button] ← [sel 1]    # p make-cube emits: "1 0 0, 0 -1 0, -1 0 0, 0 1 0, 0 0 1, 0 0 -1" | "0 90 0, 90 0 0, 0 90 0, 90 0 0, 0 0 0, 0 0 0"
    in2 ← [jit.matrix 1 char 6]
  fan-out:
    out0 → [jit.gl.gridshape @shape plane @dim 2 2 @color 1 1 1 1]:in0
```

Attributes demonstrated: `@enable`

### basic

```
Example — [jit.gl.multiple 4 @glparams position rotatexyz scale color]
  fan-in:
    in0 ← [attrui @scale]
    in0 ← [p generate_motion]    # p generate_motion emits: "exprfill (norm[0]*0.5), bang"
    in0 ← [jit.gl.handle]
    in1 ← [p generate_motion]    # p generate_motion emits: "exprfill (norm[0]*0.5), bang"
    in2 ← [p generate_motion]    # p generate_motion emits: "exprfill (norm[0]*0.5), bang"
    in3 ← [jit.matrix 3 float32 50] ← [jit.gradient 3 float32 50 @start 0.96 0.28 0.28 @end 0. 0.32 0.56] ← [loadbang]
  fan-out:
    out0 → [jit.gl.gridshape @shape torus @color 1 1 1 1 @cull_face 1]:in0    # The shape to be multiplied
```

Attributes demonstrated: `@lighting_enable`, `@scale`, `@smooth_shading`

## See also

`jit.gl.gridshape`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.text`, `jit.gl.mesh`, `jit.gl.node`, `jit.gl.texture`, `jit.gl.shader`, `jit.gl.material`, `jit.gl.pbr`, `jit.phys.multiple`
