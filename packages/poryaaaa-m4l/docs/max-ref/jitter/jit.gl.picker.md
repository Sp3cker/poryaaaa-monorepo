# jit.gl.picker

_jit · Jitter OpenGL_

> Mouse picking in an opengl context

The jit.gl.picker object responds to mouse interaction in the destination by reporting the name of jit.gl (OB3D) objects intersecting with the mouse. If an intersection occurs object outputs the message mouse followed by the intersecting object name, followed by a 0 or 1 representing the left mouse button state. If a previously intersecting object is no longer intersecting, a mouseout message is output followed by the object name and mouse button state.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages in |
| out0 | mouse picker messages |
| out1 | dumpout |

## Messages

- `touch(screen-x: long, screen-y: long)` — Detect and report object intersections from screen coordinate args
  Detect and report object intersections from screen coordinate args. If an intersection is detected, the dump outlet outputs the object name.
- `touch_ray(x-from: float, y-from: float, z-from: float, x-to: float, y-to: float, z-to: float)` — Detect and report object intersections from ray coordinate args
  Detect and report object intersections from ray coordinate args. The first 3 args set the ray start position, and the last 3 set the ray end. If an intersection is detected, the dump outlet outputs the object name.

## Attributes

- `@label` (symbol)

## Help patcher examples

### touch

> touch and touch_ray will output intersecting objects out the dump outlet. touch takes screen coordinates, and touch_ray ray start and end points

```
Example — [jit.gl.picker]
  fan-in:
    in0 ← [prepend touch_ray]
    in0 ← [prepend touch] ← [pack i i]
  fan-out:
    out1 → [routepass touch touch_ray]:in0
```

### proxy

```
Example — [jit.gl.picker @enable 0]  enable picker and move mouse in window
  fan-in:
    in0 ← [attrui @enable]
  fan-out:
    out0 → [zl change]:in0
```

Attributes demonstrated: `@enable`

### basic

```
Example — [jit.gl.picker]
  fan-in:
    in0 ← [attrui @hover]
  fan-out:
    out0 → [route mouse mouseout]:in0
```

Attributes demonstrated: `@hover`

## See also

`jit.phys.picker`, `jit.gl.handle`
