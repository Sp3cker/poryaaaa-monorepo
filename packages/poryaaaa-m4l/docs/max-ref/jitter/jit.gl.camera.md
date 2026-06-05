# jit.gl.camera

_jit · Jitter OpenGL_

> Set a rendering view

Sets the properties needed to define a view in OpenGL. These include field of view, clipping planes, and perspective or orthographic projection modes. In addition a position and orientation can be defined for a virtual camera in 3D space, and the proper view will be generated from these transforms.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | texture output if capture enabled |
| out1 | dumpout |

## Messages

- `getviewportray(screen-x: int, screen-y: int)` — Return ray coords from screen coords
  Return 6 values representing the world-space endpoints of a ray cast from the screen-x and screen-y arguments.
- `screentoworld(x: float, y: float, z: float)` — Converts screen coordinates to world coordinates.
  Converts screen coordinates to world coordinates, output out the dump outlet. The input x and y input coordinates are in pixels, and the input z coordinate is in normalized distance from camera (0.-1.), where 0. is the near clipping plane and 1. is the far clipping plane.
- `sendoutput(message: symbol, [values: list])` — Send messages to the internal texture objects used for capture output
- `worldtoscreen(x: float, y: float, z: float)` — Converts world coordinates to screen coordinates
  Converts world coordinates to screen coordinates, output out the dump outlet. The output x and y input coordinates are in pixels, and the output z coordinate is in normalized distance from camera (0.-1.), where 0. is the near clipping plane and 1. is the far clipping plane.

## Attributes

- `@introduced` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `anim_reset` — seen as: `anim_reset`
- `poly_mode` — seen as: `poly_mode`, `poly_mode $1 $1`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### anim

> Mouse click on the display window to animate this camera view with the ui_listen enabled jit.anim.drive object

```
Example — [jit.gl.camera cam-world @enable 0]
  fan-in:
    in0 ← [jit.anim.drive @ui_listen 1]
    in0 ← [attrui @lookat]    # lock the orientation to point at the lookat position
    in0 ← [message "anim_reset"]
    in0 ← [attrui @tripod]    # animate as if mounted on a tripod
    in0 ← [attrui @locklook]
    in0 ← [attrui @enable]
```

Attributes demonstrated: `@enable`, `@locklook`, `@lookat`, `@tripod`

### viewport

> The viewport attribute allows for specifying viewing areas in the window that the camera will draw to

```
Example #1 — [jit.gl.camera @viewport 0.5 0 0.5 1 @poly_mode 1 1]  clear override by sending the attribute name with no arguments
  fan-in:
    in0 ← [message "poly_mode $1 $1"]    # Cameras can override jit.gl attributes.
    in0 ← [message "poly_mode"]
    in0 ← [jit.gl.node cam-world @enable 0] ← [attrui @enable]
```

```
Example #2 — [jit.gl.camera @viewport 0 0 0.5 1]
  fan-in:
    in0 ← [jit.gl.node cam-world @enable 0] ← [attrui @enable]
```

Attributes demonstrated: `@enable`

### capture

> camera views can be captured to texture and processed using shaders

```
Example — [jit.gl.camera @capture 1 @erase_color 0 0 0 1]
  fan-in:
    in0 ← [attrui @position]
    in0 ← [jit.gl.node cam-world @enable 0] ← [attrui @enable]
  fan-out:
    out0 → [jit.gl.slab @file co.lumakey.jxs @param binary 1]:in0
```

Attributes demonstrated: `@enable`, `@position`

### basic

```
Example — [jit.gl.camera @position 0 0 15]
  fan-in:
    in0 ← [attrui @position]
    in0 ← [attrui @rotatexyz]
    in0 ← [attrui @near_clip]
    in0 ← [attrui @far_clip]
    in0 ← [attrui @lens_angle]
    in0 ← [attrui @ortho]
    in0 ← [attrui @poly_mode]    # camera can override gl attributes
    in0 ← [attrui @lookat]
```

Attributes demonstrated: `@far_clip`, `@lens_angle`, `@lookat`, `@near_clip`, `@ortho`, `@poly_mode`, `@position`, `@rotatexyz`

## See also

`jit.gl.render`, `jit.gl.sketch`, `jit.anim.node`, `jit.anim.drive`
