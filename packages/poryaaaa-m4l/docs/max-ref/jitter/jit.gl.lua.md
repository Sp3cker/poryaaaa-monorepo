# jit.gl.lua

_jit · Jitter OpenGL_

> Script OpenGL and Jitter with Lua.

jit.gl.lua provides and interface to both OpenGL and Jitter through the Lua scripting language. jit.gl.lua is similar to the js object for JavaScript.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | script outlet |
| out1 | matrix output if enabled |

## Messages

- `int` — Send an int to the msg_int function.
- `float` — Send an int to the msg_float function.
- `list` — Send an int to the msg_list function.
- `anything` — Call a function with arbitrary arguments.
- `call` — Explicitly call a function with arbitrary arguments.
- `closebang` — Called on patcher close
  When the closebang message is triggered, jit.gl.lua will look for a closebang() function in the currently running Lua script and call it.
- `loadbang` — Called on patcher load
  When the loadbang message is triggered, jit.gl.lua will look for a loadbang() function in the currently running Lua script and call it.
- `open` — Open a window for Lua editing
  Opens the text window where the object's Lua source file can be edited.
- `read` — Read a Lua script
- `wclose` — Close the editing window
  Closes the text window where the object's Lua source file is edited.

## GUI behaviors

- `(mouse)` — Double-click to open Lua editing window

## Attributes

- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `dtheta` — seen as: `dtheta $1`
- `getpath` — seen as: `getpath`
- `shape` — seen as: `shape $1`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — script outlet

### more

```
Example — [jit.gl.lua lua-ctx2 @file mouse.draw.lua]  can specify file in object box
  fan-in:
    in0 ← [message "open"]
    in0 ← [message "wclose"]
    in0 ← [attrui @gc]    # turn on garbage collection
    in0 ← [attrui @autowatch]    # watch for any changes to script and recompile
    in0 ← [jit.pwindow] ← [message "name lua-ctx2"]    # this script uses the mouse position from jit.pwindow to draw
    in0 ← [message "getpath"]    # get path of loaded script
  fan-out:
    out1 → [message ""]:in1
```

Attributes demonstrated: `@autowatch`, `@gc`

### basic

```
Example — [jit.gl.lua lua-ctx]
  fan-in:
    in0 ← [message "open"]
    in0 ← [message "wclose"]    # close editor window
    in0 ← [message "getpath"]    # get the path of the loaded script
    in0 ← [message "read lua.draw.lua"]    # load a different lua script
    in0 ← [message "dtheta $1"]    # function of the lua.draw script / open editor (or double click on jit.gl.lua object box)
    in0 ← [message "shape $1"]
    in0 ← [jit.gl.handle] ← [message "reset"]
    in0 ← [message "read basic.draw.lua"]
  fan-out:
    out1 → [message "path "/Applications/Max6/Cycling '74/jitter-help""]:in1
```

Attributes demonstrated: `@fsaa`

## See also

`lua/jit_gl_lua_color_bindings`, `lua/jit_gl_lua_opengl_bindings`, `lua/jit_gl_lua_overview`, `lua/jit_gl_lua_vector_math`, `js`, `jit.gl.sketch`
