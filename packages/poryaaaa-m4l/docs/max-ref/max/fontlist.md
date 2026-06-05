# fontlist

_max · Files, System_

> List system fonts

Outputs a list of system fonts and, optionally, their system identification numbers. Optionally filters the list by font-family.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang to output font list |
| out0 | connect to an umenu object |

## Arguments

- **font-type** (`symbol`) _(optional)_ — Font types to display
  Font types (see above) may be used as arguments to specify font types the fontlist object will recognize.

## Messages

- `bang` — Output available fonts
  Sends the names of all currently installed fonts out the fontlist object's outlet as a series of messages. The messages are formatted for use by the umenu menu display objects. The list begins with a single line containing the message clear, followed by single line messages in the form append font-name.

## Help patcher examples

### basic

```
Example — [fontlist]
  fan-in:
    in0 ← [button]    # generate a list of installed and active system fonts
  fan-out:
    out0 → [umenu]:in0
```

Attributes demonstrated: `@fontsize`

## See also

`umenu`
