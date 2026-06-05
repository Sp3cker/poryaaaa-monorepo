# jit.gl.render

_jit · Jitter OpenGL_

> Render Jitter OpenGL objects

Use jit.gl.render to render Jitter OpenGL objects to a rendering destination. jit.gl.render drives the rendering of 3D graphics, setting up and invoking the drawing of each frame. jit.gl.render can draw to jit.window and jit.pwindow.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | bang upon render completion |
| out1 | dumpout |

## Messages

- `drawclients` — Draw all automatic clients
- `drawswap` — Draw all automatic clients and swap buffers
- `erase` — Erase the render destination contents
- `getscene_dict([attribute mode: symbol])` — Outputs a dictionary of scene elements
  Outputs a dictionary of scene elements out the dumpout prepended by the symbol "scene_dict". The optional argument specifies the attribute mode. If no argument is supplied only the spatial transform attributes are included.
  If the arg all_attrs is supplied all object attributes are included.
  If the arg modified_attrs is supplied any attributes with modified state are included.
- `screentoworld(x: float, y: float, z: float)` — Converts screen coordinates to world coordinates.
  Converts screen coordinates to world coordinates, output out the dump outlet. The input x and y input coordinates are in pixels, and the input z coordinate is in normalized distance from camera (0.-1.), where 0. is the near clipping plane and 1. is the far clipping plane.
- `swap` — Swap rendering buffers
- `worldtoscreen(x: float, y: float, z: float)` — Converts world coordinates to screen coordinates
  Converts world coordinates to screen coordinates, output out the dump outlet. The output x and y input coordinates are in pixels, and the output z coordinate is in normalized distance from camera (0.-1.), where 0. is the near clipping plane and 1. is the far clipping plane.

## Attributes

- `@introduced` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `drawto` — seen as: `drawto pwin-ctx`, `drawto win-ctx`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — bang upon render completion
> - `out1` — dumpout

### basic

```
Example — [jit.gl.render win-ctx]
  fan-in:
    in0 ← [message "drawto pwin-ctx"]
    in0 ← [jit.gl.handle win-ctx @inherit_transform 1]
    in0 ← [message "drawto win-ctx"]
    in0 ← [attrui @camera]
    in0 ← [attrui @erase_color]
    in0 ← [t b erase] ← [qmetro 33] ← [toggle]
    in0 ← [t b erase] ← [qmetro 33] ← [toggle]
```

Attributes demonstrated: `@auto_rotate`, `@camera`, `@erase_color`, `@name`

## See also

`jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.videoplane`, `jit.gl.volume`
