# jit.gl.text

_jit · Jitter OpenGL_

> Render text in a GL context

Draws text in the named drawing context. The text is drawn as 2D, 3D, or outline, depending on the mode attribute. The text can be sent as a symbol, a list of symbols, or as a jit.matrix containing char data. When a jit.matrix is used, each row of the matrix is interpreted as one line of text.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | messages to this 3d object |
| out0 | disabled | matrix output if enabled |
| out1 | disabled | dumpout |

## Messages

- `append(text-to-append: list)` — Appends to the stored text
  Appends the specified symbol or list to the stored text string.
- `clear` — Clear the text
  Clear the text and reset all parameters to the default state.
- `face(face-variant: list)` — Set the font face
  Specifies the face variant of the current font. One or more face variants may be specified. The Supported font faces are normal, bold, italic.
- `font(fontname: symbol, size: int)` — Set the drawing font
  Specifies the font in which to draw.
- `size` — Set the font size in points
- `style` — Set the font face
  Equivalent to face.
- `text(text: list)` — Replace the stored text
  Replace the current text string with a symbol or list of symbols.

## Attributes

- `@align` (int) — Text alignment mode
  Text alignment mode (default = 0 (left))
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled
> - `out1` — dumpout

### basic

```
Example — [jit.gl.text @color 1 1 1 1 @fontsize 20 @align 1]
  fan-in:
    in0 ← [message "text Hello Jitter!"]
    in0 ← [attrui @tracking]
    in0 ← [attrui @leadscale]
    in0 ← [attrui @lighting_enable]
    in0 ← [attrui @depth]    # 3d only
    in0 ← [attrui @smooth_shading]
    in0 ← [attrui @line_length]
    in0 ← [jit.textfile]
    in0 ← [attrui @slant]
    in0 ← [attrui @weight]
    in0 ← [attrui @precision]    # glyph
    in0 ← [prepend append] ← [uzi 9] ← [button]
    in0 ← [message "clear"]
    in0 ← [textedit]
    in0 ← [attrui @align]    # layout
    in0 ← [attrui @scale] ← [flonum]
    in0 ← [attrui @mode]
    in0 ← [jit.gl.handle] ← [message "reset"]
    in0 ← [attrui @fontsize]    # 2d only
    in0 ← [prepend font] ← [tosymbol] ← [umenu]
```

Attributes demonstrated: `@align`, `@depth`, `@fontsize`, `@leadscale`, `@lighting_enable`, `@line_length`, `@mode`, `@precision`, `@scale`, `@slant`, `@smooth_shading`, `@tracking`, `@weight`

## See also

`jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.sketch`, `jit.gl.slab`, `jit.gl.texture`, `jit.gl.videoplane`, `jit.gl.volume`
