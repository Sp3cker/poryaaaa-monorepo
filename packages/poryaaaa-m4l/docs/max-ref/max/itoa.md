# itoa

_max · Lists_

> Convert character codes to symbol

Convert a stream or list of up to 256 integer character codes into a symbol.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | int, list | Integer list converted to symbol, bang outputs current symbol |
| in1 | int, list | Integer list converted and concatenated to current symbol |
| in2 | int, list | Integer list converted and sets contents of symbol |
| out0 | — | Symbol output |

## Messages

- `bang` — Send currently stored values as a symbol
  In left inlet: a bang message can be used to trigger the output of the currently stored string as a symbol.
- `int(input: int)` — Function depends on inlet
  In left inlet: The integer is interpreted as a character code which is stored internally and sent out the outlet as a symbol.
  In middle inlet: The integer is interpreted as a character code which is appended to the internally stored character string. No output is triggered.
  In right inlet: The integer is interpreted as a character code which is stored internally, replacing the previously stored character string, but not output.
- `float(input: float)` — Function depends on inlet
  Converted to int.
- `list(input: list)` — Function depends on inlet
  In left inlet: Each value in list of integers sent to the left inlet is interpreted as a character and stored internally as a character string, replacing the previously stored character string, and output as a symbol.
  In middle inlet: A list of integers sent to the middle inlet will be converted to characters and appended to the current internally-stored character string, without causing output.
  In right inlet: A list of integers sent to the right inlet will be converted to characters and stored internally as a character string, replacing the previously stored character string, without triggering output .
- `clear(input: list)` — Clear stored values
  In left inlet: The clear message is used to clear the contents of the internally-stored string of characters.

## Attributes

- `@basic` (int)
- `@default` (int)
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example #1 — [itoa @utf8 1]  utf8 mode expects utf8 character sequences
  fan-in:
    in0 ← [message "71 195 182 100 101 108"]    # compare with utf8 mode
  fan-out:
    out0 → [message ""]:in1
```

```
Example #2 — [itoa]
  fan-in:
    in0 ← [message "71 114 111 111 118 121 33"]    # set symbol and output
    in0 ← [message "clear"]    # clear stored symbol / output previous symbol
    in0 ← [button]
    in0 ← [message "32"]    # space
    in0 ← [message "65 66 67"]
    in0 ← [number]
    in1 ← [message "112 101 97 99 101"]    # append to symbol
    in1 ← [number]
    in1 ← [message "68 69 70"]
    in2 ← [message "76 79 86 69"]    # set symbol
    in2 ← [number]
    in2 ← [message "71 246 100 101 108"]
  fan-out:
    out0 → [message ""]:in1
```

## See also

`atoi`, `key`, `keyup`, `message`, `regexp`, `spell`, `sprintf`
