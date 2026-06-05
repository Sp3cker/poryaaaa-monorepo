# jit.pix

_jit · Jitter Code Generation_

> Generates Jitter mop pixel processing objects from a patcher.

The jit.pix object generates new Jitter mop objects from Gen patchers specifically for pixel processing. The patcher describes how each cell of a jit.matrix should be processed. jit.pix is exactly the same as jit.gl.pix except that all processing happens on the CPU as with standard Jitter mop objets. jit.pix always outputs a 4-plane matrix.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 |  |
| out0 |  |
| out1 |  |

## Messages

- `anything` — Set parameter values in the Gen patcher
- `compile` — Compile the Gen patcher
- `destroy` — Destroy the currently compiled Gen patcher
- `open` — Open the Gen patcher window
- `param` — Set a parameter of the gen patcher.
- `wclose` — Close the Gen patcher window

## GUI behaviors

- `(drag)` — Drag and drop a .genjit Gen patcher
- `(mouse)` — Double-click to open gen patcher window

## Attributes

- `@dirty` (int) — Gen patcher dirty flag
- `@default` (symbol)
- `@label` (symbol)

## Help patcher examples

### basic

```
Example — [jit.pix]
  fan-in:
    in0 ← [attrui @hue_shift]
    in0 ← [bpatcher]
  fan-out:
    out0 → [jit.pwindow]:in0
```

Attributes demonstrated: `@hue_shift`

## See also

`gen/gen_common_operators`, `gen/gen_genexpr`, `gen/gen_jitter_operators`, `gen/gen_overview`, `jit.gen`, `jit.gl.pix`, `jit.expr`, `jit.matrix`, `gen~`
