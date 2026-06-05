# jit.gl.skybox

_jit · Jitter OpenGL_

> Render a skybox in OpenGL

The jit.gl.skybox object renders a skybox in a opengl world. A skybox is a cube that remains infinitely far away from the camera, and gives the illusion of distant 3D surroundings. The jit.gl.cubemap object is used to texture the skybox.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | matrix output if enabled |
| out1 | dumpout |

## Attributes

- `@invisible` (int)
- `@label` (symbol)
- `@obsolete` (int)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled
> - `out1` — dumpout

### basic

```
Example — [jit.gl.skybox]
  fan-in:
    in0 ← [jit.gl.cubemap @file panorama_cube_map.png] ← [button]
    in0 ← [attrui @infinite]
    in0 ← [attrui @scale] ← [pak f f f]    # disable infinite to use scale
```

Attributes demonstrated: `@infinite`, `@locklook`, `@scale`

## See also

`jit.gl.camera`, `jit.gl.cubemap`, `jit.gl.material`, `jit.gl.pbr`, `jit.gl.environment`
