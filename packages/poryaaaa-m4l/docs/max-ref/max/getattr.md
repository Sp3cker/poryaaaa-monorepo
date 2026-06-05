# getattr

_max · Control_

> Query object attributes

Provides a user interface to query attribute values from an object. You can also retrieve a list of all available attributes for the attached object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message in |
| out0 | Attribute Value |
| out1 | Connect to An Object |
| out2 | dumpout |

## Arguments

- **attribute** (`symbol`) _(optional)_ — The attribute to access

## Messages

- `bang` — Retrieve the current value
  Queries the connected object and outputs the current value for the selected attribute.
- `getattrlist` — Retrieve all available attributes
  Dumps all of the available attributes out the rightmost outlet. The list of attributes is prefaced by the work "attrlist", and can be used to load a menu or verify input.

## Attributes

- `@basic` (int)
- `@label` (symbol)

## Help patcher examples

### more

```
Example #1 — [getattr]
  fan-in:
    in0 ← [umenu]
    in0 ← [message "getattrlist"]    # get list of all attributes
  fan-out:
    out0 → [message ""]:in1
    out1 → [number]:in0
    out2 → [route attrlist]:in0
```

```
Example #2 — [getattr bgcolor @listen 0]
  fan-in:
    in0 ← [button]    # bang to force output
  fan-out:
    out0 → [swatch]:in0
    out1 → [button]:in0
```

Attributes demonstrated: `@bgcolor`

### basic

```
Example — [getattr bgcolor]
  fan-out:
    out0 → [swatch]:in0
    out1 → [button]:in0
    out2 → [route modified]:in0
```

Attributes demonstrated: `@bgcolor`

## See also

`attrui`
