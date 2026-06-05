# live.map

_m4l · Live API Objects_

> Simplify the process of selecting Live interface elements for use with the Live API.

The live.map object encapsulates the process of using the mouse to select Live user interface elements to determine their LOM paths and IDs.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang, int, messages in |
| out0 | path |
| out1 | id |
| out2 | name |
| out3 | mapping 1/0 |
| out4 | dumpout |

## Messages

- `bang` — Start mapping
  Begin the process of listening for mouse clicks for Live user interface element selection. Selecting a Live user interface element will automatically stop mapping after outputting the path and ID of the selected element.
- `int(enable: int)` — Start or stop mapping
  1 in the left inlet of the object will begin the process of listening for mouse clicks for Live user interface element selection. Selecting a Live user interface element will automatically stop mapping after outputting the path and ID of the selected element. A 0 will manually stop mapping, whether or not a Live user interface element was selected.
- `cancel` — Cancel mapping
  Stop mapping if it was started.
- `getdefault` — Retrieve the default value of the mapped element.
  Output the default value of the mapped Live user interface element, preceded by the word default, from the rightmost outlet of the object.
- `getrange` — Retrieve the range of the mapped element.
  Output the range of the mapped Live user interface element, preceded by the word range, from the rightmost outlet of the object.
- `unmap` — Clear the mapped element.
  If an element was previously mapped, clear the internal state of the live.map object, as well as any information about default value, range, etc.

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `mapping` — seen as: `mapping $1`

## Help patcher examples

### strict

```
Example — [live.map]
  fan-in:
    in0 ← [message "unmap"]
    in0 ← [message "mapping $1"]
    in0 ← [attrui @strict]    # Turn on strict to prevent mapping to Max device parameters (@strict 1 or 0)
  fan-out:
    out0 → [print path @popup 1]:in0
    out0 → [message ""]:in1
    out1 → [print id @popup 1]:in0
    out1 → [message ""]:in1
    out2 → [print name @popup 1]:in0
    out2 → [message ""]:in1
    out3 → [print mapping @popup 1]:in0
    out3 → [message ""]:in1
```

Attributes demonstrated: `@strict`

### get

```
Example — [live.map]
  fan-in:
    in0 ← [message "unmap"]
    in0 ← [message "mapping $1"]
    in0 ← [message "getdefault"]    # get the parameter's default value
    in0 ← [message "getrange"]    # get the parameter's range values (minimum - maximum)
  fan-out:
    out0 → [print path @popup 1]:in0
    out0 → [message ""]:in1
    out1 → [print id @popup 1]:in0
    out1 → [message ""]:in1
    out2 → [print name @popup 1]:in0
    out2 → [message ""]:in1
    out3 → [print mapping @popup 1]:in0
    out3 → [message ""]:in1
    out4 → [route default range]:in0
```

### cancel

```
Example — [live.map]
  fan-in:
    in0 ← [message "unmap"]    # Clear mapping
    in0 ← [message "cancel"]    # Press esc key to cancel mapping
    in0 ← [message "mapping $1"]    # Toggle mapping 1/0 (on/off)
  fan-out:
    out0 → [print path @popup 1]:in0
    out0 → [message ""]:in1
    out1 → [print id @popup 1]:in0
    out1 → [message ""]:in1
    out2 → [print name @popup 1]:in0
    out2 → [message ""]:in1
    out3 → [print mapping @popup 1]:in0
    out3 → [message ""]:in1
```

### basic

```
Example — [live.map]
  fan-in:
    in0 ← [message "unmap"]    # Clear mapping
    in0 ← [message "mapping $1"]    # Toggle mapping 1/0 (on/off)
    in0 ← [button]    # bang mapping (on/off)
    in0 ← [number]    # int control for mapping (on/off)
  fan-out:
    out0 → [print path @popup 1]:in0
    out0 → [message ""]:in1
    out1 → [print id @popup 1]:in0
    out1 → [message ""]:in1
    out2 → [print name @popup 1]:in0
    out2 → [message ""]:in1
    out3 → [print mapping @popup 1]:in0
    out3 → [message ""]:in1
```

## See also

`bogus`
