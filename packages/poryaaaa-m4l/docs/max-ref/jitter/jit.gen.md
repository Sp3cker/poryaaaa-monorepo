# jit.gen

_jit · Jitter Code Generation_

> Generate new Jitter MOP objects

Generates new Jitter Matrix Operator (MOP) objects from Gen patcher and code expressions. The patcher and code describes how each cell of a jit.matrix will be processed by the jit.gen object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | in1 |
| out0 | out1 |
| out1 | dumpout |

## Messages

- `anything` — Set parameter values in the Gen patcher
- `compile` — Compile gen patcher
- `destroy` — Destroy the currently compiled kernel
- `open` — Open the Gen patcher window
- `param(name: symbol, values: list)` — Set parameter values
- `wclose` — Close gen patcher

## GUI behaviors

- `(drag)` — Drag and drop a .genjit Gen patcher
- `(mouse)` — Double-click to open gen patcher window

## Attributes

- `@dirty` (int) — Dirty flag
  Gen patcher needs to recompile
- `@default` (symbol)
- `@label` (symbol)

## Help patcher examples

### expr

```
Example — [jit.gen]
  fan-in:
    in0 ← [r gen-video]
  fan-out:
    out0 → [jit.pwindow]:in0
    out1 → [jit.pwindow]:in0
```

### sampling

```
Example — [jit.gen]
  fan-in:
    in0 ← [r gen-video]
    in0 ← [attrui @offset]
    in0 ← [attrui @amp]
  fan-out:
    out0 → [jit.pwindow]:in0
```

Attributes demonstrated: `@amp`, `@offset`

### basic

```
Example — [jit.gen]
  fan-in:
    in0 ← [bpatcher]
    in0 ← [attrui @amp]
  fan-out:
    out0 → [jit.pwindow]:in0
```

Attributes demonstrated: `@amp`

## See also

`gen/gen_common_operators`, `gen/gen_genexpr`, `gen/gen_jitter_operators`, `gen/gen_overview`, `jit.pix`, `jit.gl.pix`, `jit.gl.slab`, `jit.op`, `jit.expr`, `jit.matrix`, `gen~`
