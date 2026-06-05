# jit.world

_jit · Jitter OpenGL_

> The Jitter world context

The jit.world object encapsulates the functionality of several jitter objects, including jit.window, jit.gl.render, jit.gl.node, jit.gl.cornerpin and jit.phys.world. Physics and GL objects are automatically added to the jit.world context and video objects have their automatic output enabled, when in the same patch.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | texture or matrix output if enabled |
| out1 | render draw bang |
| out2 | dumpout |

## Arguments

- **name** (`symbol`) _(optional)_ —
  The drawing context name

## Messages

- `bang` — Draw and update all automatic clients and animations
  Draw and update all automatic clients and animations. Should only be called if enable is 0.
- `int` — Set enable attribute
  Set the enable attribute. When enabled frame updates are handled automatically.
- `clear` — Clear background
  Clear the matrix or texture image from the background, replacing with the erase_color.
- `draw` — Draw and update all automatic clients and animations
  Draw and update all automatic clients and animations. Should only be called if enable is 0.
- `front` — Bring window to front
- `jit_gl_texture(texture-name: symbol)` — Draw texture to background
  Draw the named texture to the background.
- `sendcornerpin(message: symbol, [values: list])` — Send messages to the internal jit.gl.cornerpin
  Sends messages to the internal jit.gl.cornerpin object.
- `sendhandle(message: symbol, [values: list])` — Send messages to the internal jit.gl.handle
  Sends messages to the internal jit.gl.handle object.
- `sendnode(message: symbol, [values: list])` — Send messages to the internal jit.gl.node
  Sends messages to the internal jit.gl.node object.
- `sendphys(message: symbol, [values: list])` — Send messages to the internal jit.phys.world
  Sends messages to the internal jit.phys.world object.
- `sendrender(message: symbol, [values: list])` — Send messages to the internal jit.gl.render
  Sends messages to the internal jit.gl.render object.
- `sendwindow(message: symbol, [values: list])` — Send messages to the internal jit.window
  Sends messages to the internal jit.window object.

## GUI behaviors

- `(mouse)` — Mouse events and double-click to bring window to front

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example — [jit.world my-world @fps 60 @fsaa 1]
  fan-in:
    in0 ← [attrui @output_texture]
    in0 ← [r to-world]
    in0 ← [attrui @enable_cornerpin]
    in0 ← [message "clear"]
    in0 ← [p grabber]    # p grabber emits: "automatic $1"
    in0 ← [toggle]
    in0 ← [attrui @output_matrix]
  fan-out:
    out0 → [jit.fpsgui]:in0
    out0 → [jit.matrix]:in0
    out1 → [jit.fpsgui]:in0
    out1 → [s draw]:in0
    out2 → [print @popup 1]:in0
```

Attributes demonstrated: `@enable_cornerpin`, `@locklook`, `@output_matrix`, `@output_texture`

## See also

`jit.window`, `jit.gl.render`, `jit.gl.node`, `jit.gl.cornerpin`, `jit.phys.world`, `jit.pworld`
