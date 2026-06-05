# jit.gl.asyncread

_jit · Jitter OpenGL_

> Read back from an OpenGL framebuffer

Uses Pixel Buffer Objects (PBOs) to perform asynchronous reads of the OpenGL context at high framerates. The performance gain comes from using two pixel buffer objects in tandem to amortize the cost of the read operation over time without blocking other rendering commands from executing as is typically the case when naive methods are used.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | matrix output if enabled |
| out1 | dumpout |

### Port details

**`out0` (matrix output if enabled):** Outputs a Jitter matrix equivalent in size to jit.window object it is reading.

**`out1` (dumpout):** Outputs 'get' queries.

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### texture input

> jit.gl.asyncread can readback directly from a texture stream

```
Example — [jit.gl.asyncread async-node @enable 0]
  fan-in:
    in0 ← [jit.gl.node async-ctx @name async-node @enable 0 @capture 1 @erase_color 0 0 0 0] ← [attrui @enable] ← [toggle]
    in0 ← [attrui @enable] ← [toggle]
  fan-out:
    out0 → [jit.pwindow]:in0    # The captured texture is read and output as a standard Jitter matrix
```

Attributes demonstrated: `@enable`

### basic

```
Example — [jit.gl.asyncread async-ctx]
  fan-out:
    out0 → [jit.pwindow]:in0
```

Attributes demonstrated: `@output_texture`

## See also

`jit.gl.node`, `jit.gl.pix`, `jit.gl.slab`, `jit.gl.videoplane`, `jit.world`
