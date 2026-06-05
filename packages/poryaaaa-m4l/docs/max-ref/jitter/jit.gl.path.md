# jit.gl.path

_jit · Jitter OpenGL_

> Generate and render a path in OpenGL

The jit.gl.path object generates and renders a 3D path. See the jit.path object for more information on the underlying path structure. The 3D visualization can be rendered as either a line, an extruded line (ribbon), an extruded circle (tube), or an extruded 2D contour. The path stores 10 values: position x/y/z, color r/g/b/a, scale x/y, and orient angle.

 For more information on how the extrusion is handled, see the GLE library:

 http://www.linas.org/gle

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | matrix output if enabled |
| out1 | dumpout |

## Messages

- `addcolor(r: float, g: float, b: float, a: float)` — Add color values
  Add color values to the last point in the path.
- `addcontour(contour-vals: list)` — Set the contour
  Set the contour to be used for pathstyle contour. The contour-vals are a list of X and Y position values describing a contour in 2D space. A minimum of 4 are required.
- `addorient(orient: float)` — Add an orientation value
  Add an orientation value to the last point in the path.
- `addscale(scale-x: float, scale-y: float)` — Add scale values
  Add scale values to the last point in the path.
- `append(x: float, y: float, z: float, [r: float], [g: float], [b: float], [a: float], [scale-x: float], [scale-y: float], [orient: float])` — Append point values
  Append point values to the end of the path.
- `calchandles` — Calculate the path handles
  Calculate the path handles of each point in the path using the Catmull-Rom method. This will overwrite any previously stored handles.
- `clear` — Remove all points from the path
- `closepath` — Create a closed path
  Close the path by adding a point to the end equal to the first point.
- `delete(index: int)` — Delete the point at index
- `edit(index: int, x: float, y: float, z: float, [r: float], [g: float], [b: float], [a: float], [scale-x: float], [scale-y: float], [orient: float])` — Edit the point at index
- `edithandle(index: int, x: float, y: float, z: float, [r: float], [g: float], [b: float], [a: float], [scale-x: float], [scale-y: float], [orient: float])` — Edit the point handle at index
- `gethandle(index: int)` — Get handle values at index
  Get handle values at index and output through dump outlet.
- `getpoint(index: int)` — Get point values at index
  Get point values at index and output through dump outlet.
- `insert(index: int, x: float, y: float, z: float, [r: float], [g: float], [b: float], [a: float], [scale-x: float], [scale-y: float], [orient: float])` — Insert a new point at index
- `setcolor(index: int, r: float, g: float, b: float, a: float)` — Set color values at index
- `setorient(index: int, orient: float)` — Set orientation value at index
- `setscale(index: int, scale-x: float, scale-y: float)` — Set scale values at index

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled

### contours

```
Example — [jit.gl.path path-ctx @pathstyle contour @extrudescale 0.2 0.2 @lighting_enable 1 @smooth_shading 1 @normgen facet]  path style must be set to 'contour'
  fan-in:
    in0 ← [prepend addcontour] ← [message "1. 1. 1. 2.9 0.9 3. -0.9 3. -1. 2.9 -1. 1. -2.9 1. -3. 0.9 -3. -0.9 -2.9 -1. -1. -1. -1. -2.9 -0.9 -3. 0.9 -3. 1. -2.9 1. -1. 2.9 -1. 3. -0.9 3. 0.9 2.9 1. 1. 1."]
    in0 ← [attrui @extrudescale]    # change extrusion scaling
    in0 ← [message "clear"]
    in0 ← [message "append 0 1 0, append 0 -1 0, addorient 180"]
    in0 ← [message "setorient 1 $1"]    # set orientation of path
```

Attributes demonstrated: `@extrudescale`

### textures

```
Example — [jit.gl.path path-ctx @pathstyle tube @extrudescale 0.2 0.2 @lighting_enable 1 @smooth_shading 1]
  fan-in:
    in0 ← [message "clear"]
    in0 ← [jit.gl.texture path-ctx @filter none @wrap repeat @rectangle 0] ← [jit.noise 4 char 4 4] ← [button]    # make a random texture
    in0 ← [attrui @joinstyle]
    in0 ← [attrui @texscale]
    in0 ← [attrui @texturemode]
    in0 ← [prepend append] ← [message "0 0 0, 1 0 0, 1 1 0, 0 1 0"]    # append to path
    in0 ← [attrui @endcap]
```

Attributes demonstrated: `@endcap`, `@joinstyle`, `@texscale`, `@texturemode`

### basic

```
Example — [jit.gl.path path-ctx]
  fan-in:
    in0 ← [attrui @normgen]
    in0 ← [attrui @smooth_shading]    # calculate handles/tangents
    in0 ← [attrui @lighting_enable]
    in0 ← [message "clear"]
    in0 ← [message "calchandles"]
    in0 ← [attrui @evalin]    # path render beginning
    in0 ← [attrui @evalout]    # path render ending
    in0 ← [attrui @extrudescale]
    in0 ← [r toglpath]
    in0 ← [jit.gl.handle]
    in0 ← [attrui @interpmode]
    in0 ← [attrui @pathstyle]
    in0 ← [attrui @joinstyle]
    in0 ← [attrui @segments]
    in0 ← [jit.expr @expr (norm[0]*5)*cos(6*(norm[0]*5)) (norm[0]*5)*sin(6*(norm[0]*5)) (norm[0]*5)] ← [jit.matrix 3 float32 80] ← [button]    # make a path with a matrix
  fan-out:
    out1 → [print @popup 1]:in0
```

Attributes demonstrated: `@evalin`, `@evalout`, `@extrudescale`, `@fsaa`, `@interpmode`, `@joinstyle`, `@lighting_enable`, `@normgen`, `@pathstyle`, `@segments`, `@smooth_shading`

## See also

`jit.path`, `jit.anim.path`, `jit.gl.sketch`
