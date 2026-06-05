# v8ui

_max · U/I, Languages_

> Javascript user interfaces and graphics (Modern Engine)

Provides an environment to make user interface elements using Javascript (ECMAScript 6+). This provides all of the programming tools available in the v8 object, but also exposes the mgraphics and sketch drawing routines for visual output.

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
- `array(ARG_NAME_0: list)` — TEXT_HERE
- `assiststr(ARG_NAME_0: list)` — TEXT_HERE
- `compile(filename: symbol)` — Recompiles the current file
  Recompiles the current file. If followed by a symbol, will load, compile, and set the currently loaded Javascript file to be the file specified by the symbol argument.
- `delprop(property: symbol)` — Delete a named property
  The word delprop, followed by a name, deletes the named property.
- `getprop(property: symbol)` — Get the value of a named property
  The word getprop, followed by a name, outputs the value of the property name stored in the object out the left outlet.
- `jsfile(filename: symbol)` — Load and compile a Javascript file
  The word jsfile, followed by a symbol , loads, compiles, and sets the currently loaded Javascript file to be the Javascript file specified by the symbol argument.
- `loadbang` — Invokes the function named loadbang if defined.
  Call the loadbang function
- `open` — Open the editing window
  Opens the text window where the object's Javascript source file can be edited.
- `read(ARG_NAME_0: list)` — TEXT_HERE
- `setprop(property and settings: list)` — Set a named property
  The word setprop, followed by name and one or more names or numbers, sets the named property to what follows the name. For example, after sending setprop xyz 1 2 3 to the object, the xyz property would have a value of the list 1 2 3.
- `size(width: int, height: int)` — Set the object size
  The word size, followed by two int arguments, sets the width and height of the v8ui object.
- `string(ARG_NAME_0: list)` — TEXT_HERE
- `wclose` — Close the editing window
  Closes the text window where the object's Javascript source file is edited.

## GUI behaviors

- `(drag)` — Load a Javascript source file
  When a file is dragged from the File Browser to a v8ui object, the file is loaded and executed.
- `(mouse)` — Open the editing window
  Double-clicking on the object opens a text window where the Javascript source file can be edited. When the text window is saved, the text is compiled as the object's script.

## Attributes

- `@basic` (int)
- `@category` (atom)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@save` (int)
- `@style` (symbol)

## Help patcher examples

### pointer events

> v8ui supports new PointerEvent Handler methods. These adhere as close as possible to the standard PointerEvent structure: https://developer.mozilla.org/en-US/docs/Web/API/Pointer_events . Multi-touch and tablet events are currently only supported on Windows. We do have a handful of extensions to this standard for convenience working in Max: eventType, commandKey (command key on Mac, ctrl key on Win), contextModifier(right click or control click on Mac, with right click on Win, capsLock, tipInverted (pen tip inverted--i.e. eraser pointed at tablet). Currently unsupported: width (contact area width), height (contact area height), tangentialPressure

> mouse events are blue rectangles, touch events are big yellow circles, pen events are small purple circles

```
Example #1 — [v8ui]
  (no patch cords)
```

```
Example #2 — [v8ui]
  fan-in:
    in0 ← [message "open"]
  fan-out:
    out0 → [print @popup 1]:in0
```

### sketch

> The "this" object in v8ui contains an instance of Sketch which exposes a set of drawing methods including a large portion of OpenGL calls. Sketch supports both 2D and 3D rendering, and full scene anti-aliasing (on by default). Mouse events are handled by defining event handlers such as "onclick", "ondrag", "onidle", "onidleout", and "ondblclick" functions in your Javascript file. For more info about jsui and Sketch, pease consult the Javascript in Max documentation. For more info about OpenGL, please consult opengl.org. A link to the useful and informative "Redbook" is below.

```
Example #1 — [v8ui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #2 — [v8ui]
  (no patch cords)
```

```
Example #3 — [v8ui]
  fan-out:
    out0 → [number]:in0
```

```
Example #4 — [v8ui]
  fan-out:
    out0 → [toggle]:in0
```

```
Example #5 — [v8ui]
  fan-out:
    out0 → [button]:in0
```

```
Example #6 — [v8ui]
  fan-in:
    in0 ← [message "open"]
```

```
Example #7 — [v8ui]
  fan-in:
    in0 ← [message "open"]
  fan-out:
    out0 → [flonum]:in0
    out1 → [flonum]:in0
```

```
Example #8 — [v8ui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #9 — [v8ui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #10 — [v8ui]
  fan-in:
    in0 ← [message "open"]    # click on the open message to open the javascript source
```

```
Example #11 — [v8ui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #12 — [v8ui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #13 — [v8ui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #14 — [v8ui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #15 — [v8ui]
  fan-in:
    in0 ← [r dials_and_sliders]
```

```
Example #16 — [v8ui]
  fan-in:
    in0 ← [r dials_and_sliders]
  fan-out:
    out0 → [flonum]:in0
```

### basic

> open editor window (or cmd/ctrl double click on object while patcher is unlocked)

```
Example — [v8ui]
  fan-in:
    in0 ← [flonum]
    in0 ← [message "open"]
  fan-out:
    out0 → [flonum]:in0
```

## See also

`v8`, `js`, `jsui`
