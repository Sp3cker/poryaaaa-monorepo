# message

_max · Basic_

> Send any message

message displays and sends any given message with the capability to handle specified arguments.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Trigger the Message, set Changes It |
| in1 | Set the Message Without Output |
| out0 | Message Result |

## Arguments

- **message** (`list`) — Any text
  The initial contents of the message box are typed in when the patcher window is unlocked. Any message can be contained in a message box. Certain characters have special meaning.
- **changeable-arg** (`symbol`) — Changeable argument
  A dollar sign ($), followed immediately by a number in the range 1-9, is a changeable argument. This argument's value can be replaced by the corresponding item in a list received in the inlet. (Example: $2 stores the second item in a list as its value before sending out the contents of the message box .) The value of a changeable argument is initially 0.
- **comma** (`symbol`) — Divide a message into separate messages
  A comma (,) divides a message into separate messages which will be sent out in order. (Example: 3, 4, 5 sends out 3, then 4, then 5.)
- **backslash** (`symbol`) — Escape special characters
  A backslash (\) is used to negate (escape) the traits of a special character. When a backslash immediately precedes a dollar sign, comma, or semicolon, the character is treated as a normal character. (Example: Notes played were C\, E\, and G.)

## Messages

- `bang` — Function depends on inlet
  In left inlet: Sends out the contents of the message box. A mouse click on the message box has the same effect.
  In right inlet: Sets the contents of the message box to 'bang' without triggering output.
- `int(input: int)` — Function depends on inlet
  In left inlet: The number replaces the value stored in the argument $1, if such an argument exists, then sends out the contents of the message box.
  In right inlet: Sets the contents of the message box without triggering output.
- `float(input: float)` — Function depends on inlet
  In left inlet: The number replaces the value stored in the argument $1, if such an argument exists, then sends out the contents of the message box.
  In right inlet: Sets the contents of the message box without triggering output.
- `list(input: list)` — Function depends on inlet
  Each item in the list replaces the value of its corresponding $ argument, if such an argument exists, then sends out the contents of the message box.
  In right inlet: Sets the contents of the message box without triggering output.
- `anything(message: list)` — Function depends on inlet
  See the list listing
- `append(message: list)` — Add text at end of current message
  The word append followed by any message will set the message box to display that message after any text which it already contains without triggering output.
- `prepend(message: list)` — Add text in front of current contents
  The word prepend followed by any message will set the message box to display that message before any text which it already contains without triggering output.
- `set(message: list)` — Sets contents of message box
  The word set, followed by a message, sets the contents of the message box to that new message, without triggering output. The word set by itself clears the contents of the message box .
- `setargs(message: list)` — Sets changeable argument(s) in a message box
  The word setargs, followed by a message, sets the changeable argument(s) (e.g. $1) of the receiving message box, without triggering output.
- `symbol(input: symbol)` — Function depends on inlet
  In left inlet: The symbol replaces the value stored in the argument $1, if such an argument exists, then sends out the contents of the message box.
  In right inlet: Sets the contents of the message box without triggering output.

## GUI behaviors

- `(mouse)` — Outputs message box contents
  A mouse click on a message box sends its contents out the object's outlet.

## Attributes

- `@invisible` (int)
- `@legacydefault` (atom, size 4)
- `@obsolete` (int)
- `@paint` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `"some` — seen as: `"some text"`

## Help patcher examples

### converting objects

> The convertobj attribute converts dictionaries, arrays, and strings sent to the right inlet of the message box to text

```
Example #1 — [message "me : you"]  Convert Objects to Text On
  fan-in:
    in1 ← [dict @embed 1] ← [button]    # set dictionary
    in1 ← [array 1 2 3 4 5] ← [button]    # set array
    in1 ← [string sensational] ← [button]    # set string
```

```
Example #2 — [message "dictionary u620005799"]  Convert Objects to Text Off
  fan-in:
    in1 ← [dict @embed 1] ← [button]    # set dictionary
    in1 ← [array 1 2 3 4 5] ← [button]    # set array
    in1 ← [string sensational] ← [button]    # set string
```

### appearance

> Use the format palette to create gradient message boxes

```
Example — [message "hello, world"]
  fan-in:
    in0 ← [attrui @bgfillcolor]
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @textcolor]
```

Attributes demonstrated: `@bgfillcolor`, `@style`, `@textcolor`

### send

> a message box starting with a semicolon will send the message to the named receive object instead of out its outlet

```
Example #1 — [message ";max maxwindow"]  Use ";max" to send messages to the Max application / click to open the Max console
  (no patch cords)
```

```
Example #2 — [message ";helpMsg help me"]  click to send message
  (no patch cords)
```

### basic

```
Example #1 — [message "$3 $2 $4 $1"]  use dollar sign ($) with a number to choose an element in a list
  fan-in:
    in0 ← [message "2.67 rounds to 3"]
  fan-out:
    out0 → [print @popup 1]:in0
```

```
Example #2 — [message "set"]  clear
  fan-out:
    out0 → [message ""]:in0    # click to output contents
```

```
Example #3 — [message "2.67 rounds to 3"]
  fan-out:
    out0 → [message "$1 $2 $3 $4"]:in0
    out0 → [message "$3 $2 $4 $1"]:in0    # use dollar sign ($) with a number to choose an element in a list
```

```
Example #4 — [message "$1 $2 $3 $4"]
  fan-in:
    in0 ← [message "2.67 rounds to 3"]
  fan-out:
    out0 → [print @popup 1]:in0
```

```
Example #5 — [message "1 2 3 4 5"]  use commas to separate messages
  fan-out:
    out0 → [print @popup 1]:in0    # double-click to open Max Window
```

```
Example #6 — [message "1, 2, 3, 4, 5"]
  fan-out:
    out0 → [print @popup 1]:in0    # double-click to open Max Window
```

```
Example #7 — [message "prepend 7"]  prepend
  fan-out:
    out0 → [message ""]:in0    # click to output contents
```

```
Example #8 — [message "append "so far""]  append
  fan-out:
    out0 → [message ""]:in0    # click to output contents
```

```
Example #9 — [message "set days"]  set contents
  fan-out:
    out0 → [message ""]:in0    # click to output contents
```

```
Example #10 — [message ""some text""]  set contents
  fan-out:
    out0 → [message ""]:in1    # click to output contents
```

```
Example #11 — [message ""]  click to output contents
  fan-in:
    in0 ← [message "set days"]    # set contents
    in0 ← [message "append "so far""]    # append
    in0 ← [message "prepend 7"]    # prepend
    in0 ← [button]    # bang to output
    in0 ← [message "set"]    # clear
    in1 ← [message ""some text""]    # set contents
  fan-out:
    out0 → [print @popup 1]:in0
```

## See also

`append`, `atoi`, `comment`, `itoa`, `jit.cellblock`, `prepend`, `receive`
