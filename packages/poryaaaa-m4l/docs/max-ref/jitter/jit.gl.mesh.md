# jit.gl.mesh

_jit · Jitter OpenGL_

> Generate GL geometry from matrices

Creates a geometric surface from a jit.matrix connected to the left-most inlet containing spatial coordinates. Additional geometry can be specified by attaching other jit.matrix or jit.gl.buffer objects to the other inlets.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | matrix | vertex array |
| in1 | matrix | texcoord array |
| in2 | matrix | normal array |
| in3 | matrix | color array |
| in4 | matrix | specular unused in glcore |
| in5 | matrix | edgeflag unused in glcore |
| in6 | matrix | tangent array |
| in7 | matrix | bitangent unused in glcore |
| in8 | matrix | index array |
| out0 | — | disabled |
| out1 | — | dumpout |

## Messages

- `color_matrix` — Set color values
  Specifies a matrix of color values. This must be the same size as the matrix specified by the vertex_matrix message, and can be a 3 or 4 plane matrix.
- `index_matrix` — Specify a matrix of indices
  The word index_matrix, followed by a symbol, specifies a matrix of indices. It can be any size, but it must have 1 plane and be an integer matrix.
- `jit_gl_buffer` — Bind a jit.gl.buffer object to this mesh
  Bind a jit.gl.buffer object to this mesh. When bound, the buffer data is used by the mesh when rendering. The mesh inlet determines how the data is used (e.g. as color data or normal data), but this can be overridden by the jit.gl.buffer type attribute.
- `normal_matrix` — Specifies a matrix of normal values
  The word normal_matrix, followed by a symbol, specifies a matrix of normal values. This must be the same size as the matrix specified by the vertex_matrix message. It must be a 3 plane matrix.
- `reset` — Reset to default state
  Reset all parameters to the default state and clear any stored mesh data.
- `tangent_matrix` — Specifies a matrix of tangent values
  The word tangent_matrix, followed by a symbol, specifies a matrix of tangent values. This must be the same size as the matrix specified by the vertex_matrix message. It must be a 3 plane matrix.
- `texcoord_matrix` — Specifies a matrix of texture coordinates
  The word texcoord_matrix, followed by a symbol, specifies a matrix of texture coordinate values. This must be the same size as the matrix specified by the vertex_matrix message. It can be a 2, 3, or 4 plane matrix.
- `vertex_attr_matrix` — Specify a matrix of arbitrary per-vertex data.
  The word vertex_attr_matrix, followed by a symbol, specifies a matrix of arbitrary per-vertex data
- `vertex_matrix` — Specify a matrix of vertex values
  The word vertex_matrix, followed by a symbol, specifies a matrix of vertex values. It can be 3 or 4 planes.

## GUI behaviors

- `(drag)` — Drag and drop a Jitter material file

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `draw_mode` — seen as: `draw_mode tri_grid, poly_mode 2 2, auto_normals 1, lighting_enable 1`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — disabled
> - `out1` — dumpout
> - `in3` — color array
> - `in4` — specular unused in glcore
> - `in5` — edgeflag unused in glcore
> - `in6` — tangent array
> - `in7` — bitangent unused in glcore
> - `in8` — index array

### inputs

> Texcoord matrix animation

> In order to manipulate the vertex attributes from gridshape, we must split up the matrices using jit.unpack. In this case we want individual outputs for position (3 planes), texcoord (2 planes), and normals (3 planes)

```
Example — [jit.gl.mesh @auto_tangents 1 @scale 0.5 @rotatexyz 120. 0. 0.]
  fan-in:
    in0 ← [jit.gl.pbr @parallax 1 @tex_repeat 2. 2. @mat_diffuse 4 4 4 1]
    in0 ← [p vertex-mangler]    # and looking in here / p vertex-mangler emits: "auto_normals $1"
    in1 ← [jit.rota @anchor_x 100 @anchor_y 100 @boundmode 4]
    in2 ← [p vertex-mangler]    # and looking in here / p vertex-mangler emits: "auto_normals $1"
```

Attributes demonstrated: `@automatic`, `@height_scale`, `@offset_x`

### pointcloud

> convert camera input image into a pointcloud rendered by jit.gl.mesh

```
Example — [jit.gl.mesh @draw_mode points @point_size 5 @point_mode circle_depth @color 1 1 1 1]
  fan-in:
    in0 ← [p convert-luma-to-z-displacement]
    in0 ← [jit.gl.texture] ← [jit.grab @automatic 0]
    in0 ← [message "draw_mode tri_grid, poly_mode 2 2, auto_normals 1, lighting_enable 1"]    # render with lighting
    in0 ← [attrui @point_mode]
    in0 ← [attrui @point_size]
```

Attributes demonstrated: `@automatic`, `@point_mode`, `@point_size`

### basic

```
Example — [jit.gl.mesh @auto_colors 1 @draw_mode quad_grid @scale 0.5]
  fan-in:
    in0 ← [attrui @color_mode]    # Set color mode for auto-colors
    in0 ← [attrui @auto_colors]
    in0 ← [attrui @poly_mode]
    in0 ← [attrui @draw_mode]
    in0 ← [jit.gl.gridshape @matrixoutput 1 @shape torus]
    in0 ← [attrui @enable]
```

Attributes demonstrated: `@auto_colors`, `@automatic`, `@color_mode`, `@dim`, `@draw_mode`, `@enable`, `@floating`, `@poly_mode`, `@shape`

## See also

`jitter/graphics_processing`, `jit.gl.gridshape`, `jit.gl.isosurf`, `jit.gl.model`, `jit.gl.multiple`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.text`, `jit.gl.texture`, `jit.gl.material`, `jit.gl.pbr`, `jit.gl.tf`, `jit.gl.buffer`, `jit.geom.shape`, `jit.geom.tomesh`, `jit.geom.tomatrix`
