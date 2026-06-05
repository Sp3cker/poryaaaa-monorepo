# jsui

_max · U/I, Languages_

> Javascript user interfaces and graphics (Legacy Engine)

Provides an environment to make user interface elements using Javascript (ECMAScript 5). This provides all of the programming tools available in the js object, but also exposes the mgraphics and sketch drawing routines for visual output.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | none: Inlet 0 |
| out0 | none: Outlet 0 |

## Messages

- `bang` — Call the bang function
  Invokes the function named bang if defined.
- `int(user-defined: int)` — Call the msg_int function
  Invokes the function named msg_int if defined.
- `float(user-defined: float)` — Call the msg_float function
  Invokes the function named msg_float if defined.
- `list(user-defined: list)` — Evaluate and execute a function
  Performs the same as anything.
- `anything(user-defined: list)` — Evaluate and execute a function
  Invokes the function with the message name, assigning the message arguments to the arguments to the function. For example, if the object has a function named xyz defined, the message xyz 1 2 3 would invoke the xyz function with arguments 1 2 and 3.
- `autowatch(flag: int)` — Watch for source file changes
  The word autowatch, followed by a 1, turns on file watching for the Javascript source file. When file watching is on, the file is recompiled automatically when it is modified. This allows you to use an external editor for your Javascript file. When you save the file, the jsui object will notice. autowatch 0 turns off file watching.
- `compile(filename: symbol)` — Recompiles the current file
  Recompiles the current file. If followed by a symbol, will load, compile, and set the currently loaded Javascript file to be the file specified by the symbol argument.
- `delprop(property: symbol)` — Delete a named property
  The word delprop, followed by a name, deletes the named property.
- `editfontsize(size: int)` — Change the editing window's font size
  Changes the font-size of the text used in the editing window which contains the object's Javascript source file.
- `getprop(property: symbol)` — Get the value of a named property
  The word getprop, followed by a name, outputs the value of the property name stored in the object out the left outlet.
- `jsargs(arguments: list)` — Set the Javascript arguments
  Sets the current Javascript arguments to any following message arguments.
- `jsfile(filename: symbol)` — Load and compile a Javascript file
  The word jsfile, followed by a symbol , loads, compiles, and sets the currently loaded Javascript file to be the Javascript file specified by the symbol argument.
- `loadbang` — Invokes the function named loadbang if defined.
  Call the loadbang function
- `open` — Open the editing window
  Opens the text window where the object's Javascript source file can be edited.
- `setprop(property and settings: list)` — Set a named property
  The word setprop, followed by name and one or more names or numbers, sets the named property to what follows the name. For example, after sending setprop xyz 1 2 3 to a js object. the xyz property would have a value of the list 1 2 3.
- `size(width: int, height: int)` — Set the object size
  The word size, followed by two int arguments, sets the width and height of the jsui object.
- `statemessage(messages: list)` — Test Javascript functions
  Allows for the testing of messages passed to functions within the Javascript source file.
- `wclose` — Close the editing window
  Closes the text window where the object's Javascript source file is edited.

## GUI behaviors

- `(drag)` — Load a Javascript source file
  When a file is dragged from the File Browser to a jsui object, the file is loaded and executed.
- `(mouse)` — Open the editing window
  Double-clicking on a js object opens a text window where the object's Javascript source file can be edited. When the text window is saved, the text is compiled as the object's script.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

### basic

```
Example #1 — [jsui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #2 — [jsui]
  (no patch cords)
```

```
Example #3 — [jsui]
  fan-out:
    out0 → [number]:in0
    out1 → [prepend set]:in0
```

```
Example #4 — [jsui]
  fan-out:
    out0 → [toggle]:in0
```

```
Example #5 — [jsui]
  fan-out:
    out0 → [button]:in0
```

```
Example #6 — [jsui]
  fan-in:
    in0 ← [message "open"]
```

```
Example #7 — [jsui]
  fan-in:
    in0 ← [message "open"]
  fan-out:
    out0 → [flonum]:in0
    out1 → [flonum]:in0
```

```
Example #8 — [jsui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #9 — [jsui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #10 — [jsui]
  fan-in:
    in0 ← [message "open"]    # click on the open message to open the javascript source
```

```
Example #11 — [jsui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #12 — [jsui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #13 — [jsui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #14 — [jsui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #15 — [jsui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #16 — [jsui]
  fan-in:
    in0 ← [r dials_and_sliders]
  fan-out:
    out0 → [flonum]:in0
```

```
Example #17 — [jsui]  The "this" object in jsui contains an instance of Sketch which exposes a set of drawing methods including a large portion of OpenGL calls. Sketch supports both 2D and 3D rendering, and full scene anti-aliasing (on by default). Mouse events are handled by defining event handlers such as "onclick", "ondrag", "onidle", "onidleout", and "ondblclick" functions in your Javascript file. For more info about jsui and Sketch, pease consult the Javascript in Max documentation. For more info about OpenGL, please consult opengl.org. A link to the useful and informative "Redbook" is below.
  (no patch cords)
```

## See also

`javascript`, `custom_ui_objects`, `js`, `jstrigger`, `mxj`
