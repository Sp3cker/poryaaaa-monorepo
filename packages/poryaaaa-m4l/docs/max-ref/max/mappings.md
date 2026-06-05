# mappings

_max_

> Utility object for Mappings

The mappings object allows you to enable and disable keyboard and MIDI mapping from your Max patcher, as well as some other useful utilities.

## Messages

- `key(on/off: int)` — Enable/disable keyboard mapping.
  A non-zero value for on/off will enable Keyboard Mapping mode. A zero value will disable it.
- `midi(on/off: int)` — Enable/disable MIDI mapping.
  A non-zero value for on/off will enable MIDI Mapping mode. A zero value will disable it.
- `open` — Open the Mappings Window.
  Open the Mappings Window for the current patcher.
- `read(filename: symbol)` — Read a Mappings file.
  The message read followed by a filename argument will attempt to read the specified .maxmap file and apply the contained mappings to the current patcher. Without an argument, an Open File dialog will be presented.
- `write(filename: symbol)` — Write a Mappings file.
  The message write followed by a filename argument will attempt to write the current mappings to the specified .maxmap file. Without an argument, a Save File dialog will be presented.

## GUI behaviors

- `(mouse)` — Double-click to open the Mappings Window.
  Double-click on the mappings object to open the Mappings Window for the current patcher.

## Attributes

- `@default` (symbol)
- `@label` (symbol)
- `@save` (int)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example — [mappings]  double-click to open Mappings window
  fan-in:
    in0 ← [message "read mappings-example.maxmap"]
    in0 ← [message "write"]    # save mappings to maxmap file
    in0 ← [message "open"]    # open Mappings window
    in0 ← [message "key $1"]
    in0 ← [message "midi $1"]    # toggle mapping modes (press the 'esc' key to disable)
    in0 ← [message "read"]    # read maxmap file
```

## See also

`midiin`, `notein`, `ctlin`, `bendin`, `xbendin`, `key`
