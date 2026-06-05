# jit.gl.material

_jit · Jitter OpenGL_

> Generate materials for 3D objects

Produces shaders for high quality rendering that automatically adapt to texture inputs and the number of active lights.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | Diffuse texture or matrix |
| in1 | texture/matrix | Specular texture or matrix |
| in2 | texture/matrix | Ambient texture or matrix |
| in3 | texture/matrix | Emission texture or matrix |
| in4 | texture/matrix | Normals texture or matrix |
| in5 | texture/matrix | Environment texture or matrix |
| in6 | texture/matrix | Heightmap texture or matrix |
| in7 | texture/matrix | Glossmap texture or matrix |
| out0 | — | dumpout |
| out1 | — | dumpout |

### Port details

**`in5` (Environment texture or matrix):** The environment texture can be either a jit.gl.texture or a jit.gl.cubemap.

## Messages

- `ambient_texture([name: symbol])` — Set ambient texture
- `clear` — Clear the image map at the corresponding input
- `diffuse_texture([name: symbol])` — Set diffuse texture
- `emission_texture([name: symbol])` — Set emission texture
- `environment_texture([name: symbol])` — Set environment texture
  Set the environment texture. The environment texture can be a jit.gl.texture or a jit.gl.cubemap)
- `getparamdefault(name: symbol)` — Get parameter default value
- `getparamlist` — Get list of parameter names
- `getparamtype(name: symbol)` — Get parameter type
- `getparamval(name: symbol)` — Get the parameter value
- `glossmap_texture` — Set glossmap texture
- `heightmap_texture` — Set heightmap texture
- `normals_texture([name: symbol])` — Set normals texture
  Set the normals texture. The normals texture will add a bump-mapping effect to the material.
- `open` — Open the materials browser
- `param(name: symbol, values: list)` — Set material parameter value
- `reset` — Reset shading model and colors
- `reset_colors` — Reset colors to default values
- `reset_shading_model` — Reset shading model to default values
- `specular_texture([name: symbol])` — Set specular texture
- `wclose` — Close material browser

## GUI behaviors

- `(drag)` — Drag and drop a Jitter material file
  Drag and drop a Jitter material file (.jitmtl)
- `(mouse)` — Open the materials browser

## Attributes

- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `export_material` — seen as: `export_material`
- `import_material` — seen as: `import_material`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout
> - `in1` — Specular texture or matrix
> - `in2` — Ambient texture or matrix
> - `in3` — Emission texture or matrix
> - `in5` — Environment texture or matrix
> - `in7` — Glossmap texture or matrix

### height-maps

```
Example #1 — [jit.gl.material @heightmap_mode vtf]
  fan-in:
    in0 ← [fpic] ← [button]
    in4 ← [fpic] ← [button]
    in6 ← [jit.* @val 0.05]
  fan-out:
    out0 → [jit.gl.gridshape @shape torus @scale 0.25 @dim 64 64]:in0    # since vtf mode is a vertex effect, we increase the dimensions of the geometry mesh
```

```
Example #2 — [jit.gl.material @heightmap_mode parallax]
  fan-in:
    in0 ← [fpic] ← [button]
    in4 ← [fpic] ← [button]
    in6 ← [jit.* @val 0.6] ← [fpic]
  fan-out:
    out0 → [jit.gl.gridshape @shape torus @scale 0.25 @position -0.75]:in0
```

### textures

Attributes demonstrated: `@offset`, `@rotation`, `@scale`

### normal-maps

> environment textures get reflected/refracted onto the surface using sphere mapping

### environment-maps

> environment textures get reflected/refracted onto the surface using sphere mapping

### basic

```
Example — [jit.gl.material mtl-ctx]  double-click in locked patcher to open Material Browser window
  fan-in:
    in0 ← [attrui @mat_diffuse]
    in0 ← [attrui @mat_specular]
    in0 ← [attrui @specular_model]
    in0 ← [attrui @diffuse_model]
    in0 ← [message "export_material"]
    in0 ← [attrui @override]
    in0 ← [r mtl]
    in0 ← [message "import_material"]
    in0 ← [attrui @mat_ambient]
  fan-out:
    out0 → [jit.gl.gridshape mtl-ctx @shape torus]:in0
```

Attributes demonstrated: `@diffuse_model`, `@mat_ambient`, `@mat_diffuse`, `@mat_specular`, `@override`, `@shape`, `@specular_model`

## See also

`jit.gl.pbr`, `jit.gl.cubemap`, `jit.gl.model`, `jit.gl.pass`, `jit.gl.shader`, `jit.gl.texture`, `jit.gl.environment`
