# pcontrol

_max · Patching_

> Open and close subwindows

Serves as a remote control for patcher/subpatcher windows and functions.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Control Message to Send to Patcher Window |
| out0 | Connect to Inlet of Patcher to be Controlled |

## Messages

- `close(patcher: list)` — Close the patcher window
  Closes the patcher window of any subpatches or patcher objects connected to the pcontrol object's outlet.
- `help(filename: symbol)` — Open a help file
  The word help, followed by a symbol, opens a help file in Max's max-help folder with the name of the symbol followed by .help.
- `load(filename: list)` — Load a patcher file
  The word load, followed by the name of a patcher file, opens that file if it can be found in Max's search path. The file name may optionally be followed by up to nine numbers and/or symbols, which will be substituted for the appropriate changeable # arguments (#1 to #9) in the patch being opened.
- `loadunique(patcher: list)` — Load a single patcher instance
  The word loadunique, followed by the name of a patcher file, opens a single instance that file if it can be found in Max's search path. If the file has already been loaded, the previously loaded copy will be activated (i.e. only a single copy of the file may be opened). As with the load message, the file name may optionally be followed by up to nine numbers and/or symbols, which will be substituted for the appropriate changeable # arguments (#1 to #9) in the patch being opened.
- `open(patcher: list)` — Open a patcher window
  Opens the patcher window of any subpatches or patcher objects connected to the pcontrol object's outlet.
- `shroud(filename: list)` — Open a patcher without opening the window
  The word shroud, followed by the name of a patcher file, opens that file but does not show its window. (Use this message with care, since having patchers open but invisible can potentially lead to some disconcerting results.)
- `shroudunique(filename: list)` — Open a patcher without opening the window
  The word shroudunique, followed by the name of a patcher file, opens that file but does not show its window. As with loadunique, if the file has already been loaded, another copy will not be loaded. (Use this message with care, since having patchers open but invisible can potentially lead to some disconcerting results.)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `enable` — seen as: `enable 0`, `enable 0 1`, `enable 1`

## Help patcher examples

### basic

```
Example — [pcontrol]
  fan-in:
    in0 ← [message "enable 0"]    # disable the patcher window for MIDI and Audio
    in0 ← [message "enable 1"]    # close the patcher window
    in0 ← [message "enable 1 1"]    # enable the patcher and its subpatchers for MIDI and Audio / enable the patcher window for MIDI and Audio
    in0 ← [message "close"]    # open the patcher window
    in0 ← [message "open"]
    in0 ← [message "help float"]
    in0 ← [message "load <filename>"]    # load a patcher file
    in0 ← [message "enable 0 1"]    # open a help window / disable the patcher and its subpatchers for MIDI and Audio
  fan-out:
    out0 → [pcontrol_ExamplePatch]:in0
```

## See also

`bpatcher`, `inlet`, `patcher`, `thispatcher`
