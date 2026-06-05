# jit.gl.physdraw

_jit · Jitter OpenGL_

> A physics opengl debug drawer

The jit.gl.physdraw object performs debug drawing of the objects in a physics simulation, including jit.phys.body, jit.phys.multiple, and constraint objects such as jit.phys.hinge and jit.phys.6dof. A valid opengl context and a valid jit.phys.world context are required for debug drawing of the physics world.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages in |
| out0 |  |

## Attributes

- `@label` (symbol)

## Help patcher examples

### basic

> NOTE: jit.gl.physdraw is not optimized for efficient drawing and therefore is only intended for debugging and development of physics patches.

```
Example — [jit.gl.physdraw]
  fan-in:
    in0 ← [attrui @rgb]
    in0 ← [attrui @enable]
    in0 ← [attrui @constraintsize]
    in0 ← [attrui @color]
    in0 ← [attrui @contactsize]
    in0 ← [attrui @draw_aabb]
    in0 ← [attrui @draw_worldbox]
    in0 ← [attrui @draw_bodies]
```

Attributes demonstrated: `@color`, `@constraintsize`, `@contactsize`, `@draw_aabb`, `@draw_bodies`, `@draw_worldbox`, `@dynamics`, `@enable`, `@rgb`

## See also

`jit.phys.body`, `jit.phys.world`, `jit.phys.multiple`
