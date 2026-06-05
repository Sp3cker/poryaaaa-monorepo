# jit.expr

_jit · Jitter Math_

> Evaluate an expression to fill a matrix

Evaluates expressions to fill an output matrix. The expression can contain any operator available from within jit.op, any functor available from within jit.bfg, and many jitter MOPs. A variable number of inputs can be specified with an attribute argument setting the inputs attribute.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | matrix | in |
| in1 | matrix | in2 |
| out0 | matrix | out |
| out1 | matrix | dumpout |

## Messages

- `int` — Treat data as int cells
  Sets all matrix cells corresponding with input to int.
- `float` — Treat data as float cells
  Sets all matrix cells corresponding with input to float.
- `list(input: list)` — Treat list as per-plane data
  Sets all matrix cells corresponding with input to list, on a per plane basis.

## Attributes

- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `expr` — seen as: `expr "fractal.hetero(norm[0]*20, norm[1]*20)"`, `expr "jit.clip(in[0], @min 0.9)"`, `expr "noise.gradient(norm[0]*2, norm[1]*2, @seed 313)"`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### examples

```
Example — [jit.expr @expr (1-hypot(snorm[0]\,snorm[1]))*in[0]*2.]  generating a soft vignette
  fan-in:
    in0 ← [bpatcher]
  fan-out:
    out0 → [jit.pwindow]:in0
```

### functions

> You can also use jit.bfg functions and many Jitter MOP objects within an expression

```
Example — [jit.expr @expr "1-noise.voronoi(norm[0]*2.,norm[1]*2., @weight 5.)"]
  fan-in:
    in0 ← [message "expr "noise.gradient(norm[0]*2, norm[1]*2, @seed 313)""]
    in0 ← [message "expr "fractal.hetero(norm[0]*20, norm[1]*20)""]
    in0 ← [message "expr "jit.clip(in[0], @min 0.9)""]
    in0 ← [bpatcher]
    in0 ← [message "expr "pow(jit.noise(), 4)""]
    in0 ← [attrui @cache]    # Non-input expressions will cache the calculation by default, for efficiency. For operations like jit.noise, you might want to turn off 'cache'
    in0 ← [message "expr 1-jit.robcross(in[0]"]
  fan-out:
    out0 → [jit.pwindow]:in0
```

Attributes demonstrated: `@cache`

### variables

> These variables can be used in an expression. The number between the brackets defines the index. For example 'in[0]' is the first input, 'in[1]' the second input. Similarly 'norm[0]' generates normalized values across the first dimension (horizontal) and 'norm[1]' across the second (vertical).

```
Example — [jit.expr @expr in[0]]
  fan-in:
    in0 ← [message "expr cell[0]"]
    in0 ← [message "expr norm[1]"]    # normalized values
    in0 ← [message "expr norm[0]"]
    in0 ← [message "expr snorm[0]"]
    in0 ← [message "expr snorm[1]"]    # signed normalized values
    in0 ← [message "expr dim[1]"]    # Also supports these mathematically useful constants: PI, TWOPI, HALFPI, INVPI, DEGTORAD, RADTODEG, E, LN2, LN10, LOG10E, LOG2E, SQRT2, SQRT1_2 / size of matrix dimension
    in0 ← [message "expr in[0]"]
    in0 ← [message "expr dim[0]"]
    in0 ← [message "expr PI"]
    in0 ← [message "expr HALFPI"]
    in0 ← [jit.matrix 1 float32 10 5]
    in0 ← [message "expr in[1]"]    # inputs
    in0 ← [message "expr cell[1]"]    # cell number
    in1 ← [jit.noise 1 float32 15 10] ← [button]
  fan-out:
    out0 → [jit.pwindow]:in0
    out0 → [jit.cellblock]:in0
```

### basic

```
Example — [jit.expr]
  fan-in:
    in0 ← [attrui @expr]    # The 'expr' attribute is used to define the expression used by jit.expr to calculate the output.
    in0 ← [message "expr in[0]*in[1]"]    # multiply left inlet and right inlet
    in0 ← [message "expr in[0]"]
    in0 ← [message "expr norm[0]"]    # generate normalized {0...1} values horizontally
    in0 ← [message "expr snorm[1]"]    # generate signed normalized {-1...1} values vertically
    in0 ← [message "expr hypot(snorm[0]\,snorm[1])"]    # normalized distance from center cell
    in0 ← [message "expr cell[0]"]    # generate cell numbers horizontally
    in0 ← [jit.noise 1 float32 5 5]
    in1 ← [flonum]    # right inlet(s) can be matrix or scalar value
  fan-out:
    out0 → [jit.pwindow]:in0
    out0 → [jit.cellblock]:in0
```

Attributes demonstrated: `@expr`

## See also

`expr`, `jit.charmap`, `jit.op`, `jit.bfg`, `vexpr`
