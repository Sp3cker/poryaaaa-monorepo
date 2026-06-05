# jit.window

_jit · Jitter UI_

> Display data in a window

Use the jit.window object to draw pixels or graphics to a window. jit.window displays jit.matrix data as well as OpenGL 3D graphics.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | matrix | in |
| out0 | — | bangout |
| out1 | — | dumpout |

## Messages

- `front` — Bring the window to front
- `sendtexture(name: symbol, message: symbol, [values: list])` — Send a message to the internal texture

## GUI behaviors

- `(mouse)` — Double-click to bring the jit.window to front

## Attributes

- `@basic` (int)
- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `fullscreen` — seen as: `fullscreen $1`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — bangout

### basic

```
Example — [jit.window video]
  fan-in:
    in0 ← [attrui @interp]
    in0 ← [message "fullscreen $1"]
    in0 ← [bpatcher]
    in0 ← [attrui @border]
    in0 ← [attrui @floating]
    in0 ← [attrui @grow]
    in0 ← [attrui @visible]
    in0 ← [attrui @idlemouse]
  fan-out:
    out1 → [message "mouseidle 664 423 0 0 0 0 0 0"]:in1
```

Attributes demonstrated: `@border`, `@floating`, `@grow`, `@idlemouse`, `@interp`, `@visible`

## See also

`jit.gl.render`, `jit.pwindow`
