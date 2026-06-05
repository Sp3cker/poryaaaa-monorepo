# jit.gl.shader

_jit · Jitter OpenGL_

> Manage a GL shader

Manages the process of compiling, binding and submitting a shader to OpenGL. A shader consists of both a vertex program and a fragment (aka pixel) program, which can be defined in a xml shader description file (JXS), or submitted individually. Currently the high level language GLSL is supported.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | matrix output if enabled |
| out1 | dumpout |

## Messages

- `bind` — Binds and compiles the shader object
  Manually binds and compiles the shader object.
- `compile` — Compile the shader object
  Manually compiles the shader object.
- `dispose` — Dispose and unload the current shader
  Dispose of and unload the current shader.
- `dump(type: symbol)` — Dumps shader data to the Max Console
  Dumps the indicated shader data to the max console window. Valid types are params, source, assembly.
- `getparamdefault(name: symbol)` — Report parameter defaults
  Sends the default data values for the indicated shader parameter out the right-most outlet.
- `getparamdescription` — Get a parameter description
- `getparamlist` — Report shader parameters
  Sends the names of all the shader parameters out the right-most outlet.
- `getparamtype(name: symbol)` — Report shader parameter data types
  Sends the name of the datatype for the indicated shader parameter out the right-most outlet.
- `getparamval(name: symbol)` — Report current shader parameter values
  Sends the data values for the indicated shader parameter out the right-most outlet.
- `link` — Link the shader object.
  Manually links the shader object.
- `param` — Set a shader parameter value
  Sets the given shader parameter with the given atom values as defined in a JXS (Jitter shader) file.
- `program_param` — Set a geometry shader program parameter
  A geometry shader program parameter.
- `read(filename: symbol)` — Loads a JXS shader file from disk
  Loads the given JXS shader file from disk.
- `unbind` — Unbind the shader object
  Manually unbinds the shader object.

## GUI behaviors

- `(drag)` — Load a shader file
  Dragging a JXS file from the Max File Browser or desktop to a jit.gl.shader object, will load the file.
- `(mouse)` — Double click to open the shader editor
  Double click to open the shader editor. If no file is loaded the editor will load a starter shader to use as the basis for a new shader. This file must be saved to disk for use after the editor is closed.

## Attributes

- `@introduced` (symbol)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled

### basic

```
Example — [jit.gl.shader shade-ctx @name myshady @file td.sinefold.jxs]
  fan-in:
    in0 ← [message "read mat.xray.jxs"]
    in0 ← [message "getparamlist"]
    in0 ← [message "read td.sinefold.jxs"]    # load a shader file
    in0 ← [prepend param]
  fan-out:
    out1 → [print]:in0
```

Attributes demonstrated: `@output_texture`

## See also

`external_text_editor`, `jitter/jxs_file_format`, `jit.gl.mesh`, `jit.gl.pix`, `jit.gl.slab`, `jit.gl.texture`, `jit.gl.material`, `jit.gl.pass`
