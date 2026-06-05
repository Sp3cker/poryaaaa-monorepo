# jit.gl.nurbs

_jit · Jitter OpenGL_

> Generate NURBS surface

Renders a Non-Uniform Rational B-Spline (NURBS) surface. A NURBS is a mathematical model that lets you represent virtually any desired shape, from points, straight lines, and polylines to conic sections (circles, ellipses, parabolas, and hyperbolas) to free-form curves with arbitrary shapes.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | messages to this 3d object |
| out0 | disabled | matrix output if enabled |
| out1 | disabled | dumpout |

## Messages

- `ctlmatrix(matrix-name: symbol)` — Copy to control points
  Copies the named matrix to the control point matrix. Matrices must have a planecount of 3 or 4, and should be type float32 or float64
- `rand` — Generate random control points
  Generates a random set of control points.

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled
> - `out1` — dumpout

### basic

```
Example — [jit.gl.nurbs jgn @lighting_enable 1]
  fan-in:
    in0 ← [attrui @poly_mode]
    in0 ← [attrui @closed]
    in0 ← [message "rand"]
    in0 ← [attrui @dim]
    in0 ← [jit.gl.handle jgn]
    in0 ← [attrui @order]
    in0 ← [attrui @ctlshow]
```

Attributes demonstrated: `@closed`, `@ctlshow`, `@dim`, `@order`, `@poly_mode`

## See also

`jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.videoplane`, `jit.gl.volume`
