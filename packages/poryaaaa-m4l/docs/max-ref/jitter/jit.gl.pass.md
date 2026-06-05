# jit.gl.pass

_jit · Jitter Code Generation_

> Render scene passes with shader processing

The jit.gl.pass object encapsulates processing of one or more sub-passes. A sub-pass consists of a single frame of gl, and post-processing shader, and is defined in a xml pass description file (JXP). Complex scene-processing hierarchies can be obtained by chaining multiple jit.gl.pass objects.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | texture output |
| out1 | connect to child jit.gl.pass objects |
| out2 | dumpout |

## Messages

- `anything` — Dynamic attributes
  Get and set dynamic attributes generated from sub-pass shader parameters.
- `param` — Sets the given shader parameter with the given atom values as defined in a JXS (Jitter shader)
  Sets the given shader parameter with the given atom values as defined in a JXS (Jitter shader) file.
- `read` — Load a JXP file from disk

## GUI behaviors

- `(drag)` — Drag and drop a .jxp file
- `(mouse)` — Open the JXP file

## Attributes

- `@documented` (int)
- `@dynamicattr` (int)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out2` — dumpout

### genjit support

```
Example — [jit.gl.pass @file gen-test.jxp @xfade 0.5]  This pass demonstrates using genjit files in a JXP pass effect
  fan-in:
    in0 ← [attrui @xfade]
    in0 ← [substitute out_name texture] ← [jit.gl.pix]
    in0 ← [jit.gl.node pass-ctx @enable 0] ← [attrui @enable] ← [toggle]
    in0 ← [attrui @xfade_1]
    in0 ← [attrui @xfade_2]
  fan-out:
    out0 → [jit.gl.layer pass-ctx @layer 1 @enable 0]:in0
```

Attributes demonstrated: `@automatic`, `@enable`, `@xfade`, `@xfade_1`, `@xfade_2`

### basic

```
Example #1 — [jit.gl.pass pass-sub-node @fxname dof]
  fan-in:
    in0 ← [attrui @focal_distance] ← [message "2."]
    in0 ← [attrui @focal_range] ← [message "1"]
    in0 ← [attrui @blur_width] ← [message "0.5"]
  fan-out:
    out1 → [jit.gl.pass pass-sub-node @fxname bloom]:in0
```

```
Example #2 — [jit.gl.pass pass-sub-node @fxname bloom]
  fan-in:
    in0 ← [attrui @threshold] ← [message "0.95"]
    in0 ← [attrui @blur_width] ← [message "1"]
    in0 ← [attrui @bloom_amt]
    in0 ← [jit.gl.pass pass-sub-node @fxname dof]
  fan-out:
    out0 → [jit.gl.layer pass-ctx]:in0
```

Attributes demonstrated: `@bloom_amt`, `@blur_width`, `@focal_distance`, `@focal_range`, `@threshold`

## See also

`external_text_editor`, `jitter/render_passes`, `jit.gl.node`, `jit.gl.slab`, `jit.gl.pix`, `jit.gl.shader`, `jit.gl.camera`, `jit.gl.light`
