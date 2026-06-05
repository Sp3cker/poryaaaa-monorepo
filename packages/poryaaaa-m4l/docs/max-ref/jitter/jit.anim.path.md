# jit.anim.path

_jit · Jitter Animation_

> Evaluate a path of 3D transform points

Takes a series of 3D transform points and interpolates between them. See the jit.path object for more information on this interpolation. Each point stores 11 values: a time value, 3 position values, 3 scale values and 4 quaternion rotation values.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages or matrix in |
| out0 | evaluated position/scale/quat values |
| out1 | dumpout |

## Messages

- `bang` — Update the path animation
  Cause the animation to update, if currently active, and the transform messages to output.
- `addquat(x: float, y: float, z: float, w: float)` — Add quaternion orientation values
  Add quaternion orientation values to the last point in the path.
- `addrotate(angle: float, x: float, y: float, z: float)` — Add angle-axis orientation values
  Add angle-axis orientation values to the last point in the path. The values are converted to a quaternion before adding.
- `addrotatexyz(x: float, y: float, z: float)` — Add euler orientation values
  Add euler orientation values to the last point in the path. The values are converted to a quaternion before adding.
- `addscale(x: float, y: float, z: float)` — Add scale values
  Add scale values to the last point in the path.
- `append(time: float, x: float, y: float, z: float, [scale-x: float], [scale-y: float], [scale-z: float], [quat-x: float], [quat-y: float], [quat-z: float], [quat-w: float])` — Append point values
  Append point values to the end of the path. If necessary, the path will be resorted based on the time value prior to evaluation.
- `calchandles` — Calculate the path handles
  Calculate the path handles of each point in the path using the Catmull-Rom method. This will overwrite any previously stored handles.
- `clear` — Remove all points from the path
- `closepath` — Create a closed path
  Close the path by adding a point to the end equal to the first point.
- `delete(index: int)` — Delete the point at index
- `edit(index: int, time: float, x: float, y: float, z: float, scale-x: float, scale-y: float, scale-z: float, quat-x: float, quat-y: float, quat-z: float, quat-w: float)` — Edit the point at index.
- `edithandle(index: int, time: float, x: float, y: float, z: float, scale-x: float, scale-y: float, scale-z: float, quat-x: float, quat-y: float, quat-z: float, quat-w: float)` — Edit the point handle at index
- `eval(parameter: float)` — Evaluate the path using parameter
  Evaluate the path using parameter, (between 0. and 1.) and output the interpolated values.
- `evallength(length-parameter: float)` — Evaluate the path using length-parameter
  Evaluate the path using the length-parameter (between 0 and pathlength) and output the interpolated values.
- `evaltime(time: float)` — Evaluate the path using time
  Evaluate the path using time (between 0 and duration) and output the interpolated values.
- `gethandle(index: int)` — Get handle values at index
  Get handle values at index and output through the right (dumpout) outlet.
- `getpoint(index: int)` — Get point values at index
  Get point values at index and output through the right (dumpout) outlet.
- `insert(index: int, time: float, x: float, y: float, z: float, [scale-x: float], [scale-y: float], [scale-z: float], [quat-x: float], [quat-y: float], [quat-z: float], [quat-w: float])` — Insert a new point at index
- `next` — Advance to next point
  Advance the path animation to the next point.
- `prev` — Rewind to previous point
  Rewind the path animation to the previous point
- `setquat(index: int, x: float, y: float, z: float, w: float)` — Set quaternion orientation values at index
- `setrotate(index: int, angle: float, x: float, y: float, z: float)` — Set angle-axis orientation orientation values at index
- `setrotatexyz(index: int, x: float, y: float, z: float)` — Set euler orientation orientation values at index
- `setscale(index: int, x: float, y: float, z: float)` — Set scale values at index
- `settime(index: int)` — Set time at index
  Set time at index. If necessary, the path will be resorted prior to evaluation.
- `sorttime` — Sort path based on time values
- `start` — Start path animation
- `stop` — Stop path animation

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example — [jit.anim.path @drawpath glpath]
  fan-in:
    in0 ← [attrui @interpmode]
    in0 ← [attrui @loop]
    in0 ← [attrui @rate]
    in0 ← [r topath]
    in0 ← [attrui @evalscale]
    in0 ← [attrui @evalquat]
    in0 ← [attrui @timemode]
  fan-out:
    out0 → [jit.gl.gridshape @shape torus @scale 0.2 0.2 0.2]:in0
    out1 → [print @popup 1]:in0
    out1 → [s pathdumpout]:in0
```

Attributes demonstrated: `@evalquat`, `@evalscale`, `@interpmode`, `@loop`, `@rate`, `@timemode`

## See also

`jit.anim.drive`, `jit.anim.path`, `jit.path`, `jit.gl.path`
