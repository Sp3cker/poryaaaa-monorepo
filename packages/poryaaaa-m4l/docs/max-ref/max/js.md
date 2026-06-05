# js

_max · Languages_

> Execute Javascript (Legacy Engine)

Exposes the Javascript language (ECMAScript 5) and some Max specific extensions. The js object can be instantiated with a javascript filename or with numerical arguments to specify the number of outlets and inlets respectively. The default number of outlets and inlets are both 1.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | js: Inlet 0 |
| out0 | js: Outlet 0 |

## Arguments

- **filename** (`symbol`) _(optional)_ — Javascript source file name
  Specifies the name of a text file to be used as the Javascript source. If no argument is specified, it will not initially have any Javascript associated with it. You can still open a text window and edit and save the Javascript source, but unless you recreate the object with the saved source filename as an argument, the file will not be used when a patch containing the js object is loaded.
- **inlets-outlets** (`list`) _(optional)_ — Number of inlets and/or outlets
  If no filename is present as an argument, the number of inlets and outlets is specified. If one int argument is present, the number of desired outlets is specified. If two int arguments are present, the first number specifies the number of outlets and the second number specifies the number of inlets.
- **jsarguments** (`list`) _(optional)_ — Script arguments
  Following the optional filename or number of outlets and inlets, any symbols or numbers can be entered that will be assigned to the Javascript variable jsarguments. jsarguments[0] is the filename entered, and jsarguments[1] is the first typed-in argument following the filename. The Javascript expression jsarguments.length will be one more than the number of typed-in arguments

## Messages

- `bang` — Send a bang to the Javascript
  Invokes the function named bang if defined.
- `int(user-defined: int)` — Call the msg_int function
  Invokes the function named msg_int if defined.
- `float(user-defined: float)` — Call the msg_float function
  Invokes the function named msg_float if defined.
- `list(user-defined: list)` — Evaluate and call a function
  Performs the same as anything.
- `anything(user-defined: list)` — Evaluate and call a function
  Invokes the function with the message name, assigning the message arguments to the arguments to the function. For example, if the object has a function named xyz defined, the message xyz 1 2 3 would invoke the xyz function with arguments 1 2 and 3.
- `autowatch(flag: int)` — Watch for changes in the source file
  The message autowatch, followed by a 1, turns on file watching for the Javascript source file. When file watching is on, the file is recompiled automatically when it is modified. This allows you to use an external editor for your Javascript file. When you save the file, the js object will notice. autowatch 0 turns off file watching.
- `compile(filename: symbol)` — Recompiles the current file
  Recompiles the current file. If followed by a symbol, will load, compile, and set the currently loaded Javascript file to be the file specified by the symbol argument.
- `delprop(propertyname: symbol)` — Delete a named property
  The word delprop, followed by a name, deletes the named property.
- `editfontsize(font-size: int)` — Change the editing window's font size
  Changes the font-size of the text used in the editing window which contains the object's Javascript source file.
- `getprop(property: symbol)` — Report a property's value
  The word getprop, followed by a name, outputs the value of the property name stored in the object out the left outlet.
- `loadbang` — Call the loadbang function
  Invokes the function named loadbang if defined. This message is sent when the file is loaded.
- `open` — Open a window for Javascript editing
  Opens the text window where the object's Javascript source file can be edited.
- `setprop(property: symbol, values: list)` — Set a named property's value
  The word setprop, followed by name and one or more names or numbers, sets the named property to what follows the name. For example, after sending setprop xyz 1 2 3 to a js object. the xyz property would have a value of the list 1 2 3.
- `statemessage(messages: list)` — Test passed messages
  Allows for the testing of messages passed to functions within the Javascript source file.
- `wclose` — Close the editing window
  Closes the text window where the object's Javascript source file is edited.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `foo` — seen as: `foo bar`

## Help patcher examples

### properties

```
Example — [js jshelptest.js hello]
  fan-in:
    in0 ← [message "setprop myval goofball, bang"]    # javascript properties can be dynamically set, queried, added, or removed with the setprop, getprop, and delprop messages
    in0 ← [message "autowatch $1"]    # enable automatic file watching--i.e. anytime the currently loaded javascript file has been modified by another editor, js will reload and recompile the file
    in0 ← [message "1 2 3"]
    in0 ← [message "foo bar"]
    in0 ← [button]
    in0 ← [number]
    in0 ← [flonum]
    in0 ← [message "delprop binky"]
    in0 ← [message "getprop binky"]
    in0 ← [message "setprop binky 1984"]
  fan-out:
    out0 → [message ""]:in1
```

### basic

```
Example — [js jshelptest.js hello]
  fan-in:
    in0 ← [message "compile jshelptest2.js"]    # load and compile a different javascript file
    in0 ← [message "compile"]    # reload and compile the current javascript file
    in0 ← [message "open"]    # open editor window (or double click on object)
```

## See also

`jstrigger`, `jsui`, `mxj`
