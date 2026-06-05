# console

_max · System_

> Console Output in Patcher

Mirror and filter messages to the Max window in your patcher.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | Change Filter Attributes |
| out0 | — | Object Name |
| out1 | — | Message |
| out2 | int | Message Type |

## Messages

- `clear` — Clear the Max Console
  Sending the clear message to any console object will clear the entire contents of the Max console window.
- `write([filepath: symbol])` — Save Console Contents to Disk
  Sending the message write to any console object will write the entire contents of the Max console to a text file, ignoring any filters. An optional argument allows you to specify the filepath. If no filepath is provided, a file browser window will open allowing you to specify the write location.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `textfilter` — seen as: `textfilter 7`, `textfilter seven`

## Help patcher examples

### basic

```
Example — [console]
  fan-in:
    in0 ← [message "textfilter seven"]
    in0 ← [message "textfilter 7"]
  fan-out:
    out0 → [message "prince_says"]:in1
    out1 → [button]:in0
    out1 → [message "one day all seven will die"]:in1
    out2 → [message "0"]:in1
```

## See also

`max_console`, `preferences_and_settings?panchor=console`, `print`
