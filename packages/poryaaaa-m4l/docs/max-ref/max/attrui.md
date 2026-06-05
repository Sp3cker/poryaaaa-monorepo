# attrui

_max · U/I_

> Inspect attributes

Use attrui object to inspect the attribute values of the object(s) it is connected to.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Messages In |
| out0 | Connect to Objects whose Attribute you Want to Explore |

### Port details

**`out0` (Connect to Objects whose Attribute you Want to Explore):** When attrui is connected to an object, its menu reflects the attributes in that object. If connected to multiple objects, the menu contains attributes common to all objects.

## Messages

- `int(input: int)` — Set attribute value
  The currently selected attribute of all connected objects will be set to the int value.
- `float(input: float)` — Set attribute value
  The currently selected attribute of all connected objects will be set to the float value.
- `list(input: any)` — Set attribute value
  The currently selected attribute of all connected objects will be set to the list value.
- `anything(input: any)` — Set attribute value
  The currently selected attribute of all connected objects will be set to the incoming value.

## GUI behaviors

- `(mouse)` — Edit attribute values
  Choose an attribute from the popup menu on the left half of attrui to inspect. Use the controls on the right half to view or change the attribute's value.

## Attributes

- `@invisible` (int)
- `@obsolete` (int)
- `@paint` (int)
- `@renamed` (symbol)
- `@save` (int)
- `@stylealias` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `attrfilter` — seen as: `attrfilter`, `attrfilter triangle bgcolor`

## Help patcher examples

### appearance

```
Example #1 — [attrui @tricolor]
  fan-out:
    out0 → [attrui @blinktime]:in0
```

```
Example #2 — [attrui @style]  Create and apply styles using the format palette or type a style name here
  fan-out:
    out0 → [attrui @blinktime]:in0
```

```
Example #3 — [attrui @textjustification]
  fan-out:
    out0 → [attrui @blinktime]:in0
```

```
Example #4 — [attrui @fontface]
  fan-out:
    out0 → [attrui @blinktime]:in0
```

```
Example #5 — [attrui @fontsize]
  fan-out:
    out0 → [attrui @blinktime]:in0
```

```
Example #6 — [attrui @fontname]
  fan-out:
    out0 → [attrui @blinktime]:in0
```

```
Example #7 — [attrui @textcolor]  text attributes
  fan-out:
    out0 → [attrui @blinktime]:in0
```

```
Example #8 — [attrui @htricolor]
  fan-out:
    out0 → [attrui @blinktime]:in0
```

```
Example #9 — [attrui @bgcolor]
  fan-out:
    out0 → [attrui @blinktime]:in0
```

```
Example #10 — [attrui @orientation]
  fan-out:
    out0 → [attrui @blinktime]:in0
```

```
Example #11 — [attrui @blinktime]
  fan-in:
    in0 ← [attrui @tricolor]
    in0 ← [attrui @htricolor]
    in0 ← [attrui @orientation]
    in0 ← [attrui @textcolor]    # text attributes
    in0 ← [attrui @fontname]
    in0 ← [attrui @fontsize]
    in0 ← [attrui @fontface]
    in0 ← [attrui @textjustification]
    in0 ← [p adjust size] ← [getattr @attr orientation]    # p adjust size emits: "327. 199. 152. 67." | "229. 199. 250. 90." | "224. 252. 199. 46." | "224. 252. 199. 23." | "220. 248. 208. 57.5" | "220. 248. 208. 34.5"
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [getattr @attr orientation]
  fan-out:
    out0 → [button]:in0
```

Attributes demonstrated: `@bgcolor`, `@blinktime`, `@fontface`, `@fontname`, `@fontsize`, `@htricolor`, `@orientation`, `@style`, `@textcolor`, `@textjustification`, `@tricolor`

### advanced

> only display specific attributes

```
Example #1 — [attrui @mousefilter]
  fan-in:
    in0 ← [attrui @lock]    # lock attrui's menu
  fan-out:
    out0 → [number]:in0
```

```
Example #2 — [attrui @lock]  lock attrui's menu
  fan-out:
    out0 → [attrui @mousefilter]:in0
```

```
Example #3 — [attrui @triangle]
  fan-in:
    in0 ← [message "attrfilter triangle bgcolor"]    # display all attributes
    in0 ← [message "attrfilter"]
  fan-out:
    out0 → [number]:in0
```

Attributes demonstrated: `@lock`, `@mousefilter`, `@triangle`

### basic

```
Example #1 — [attrui @bgcolor]  multi-number (float) / Display mode:
  fan-out:
    out0 → [dial]:in0
```

```
Example #2 — [attrui @bgcolor]  click to show all common attributes / click to edit value
  fan-out:
    out0 → [number]:in0
    out0 → [flonum]:in0
    out0 → [slider]:in0
```

```
Example #3 — [attrui @blinkcolor]  click to show all object attributes / click to edit value
  fan-out:
    out0 → [button]:in0    # Multiple objects: / connect to object(s) to inspect attributes
```

```
Example #4 — [attrui @bgcolor]  rgba swatch
  fan-out:
    out0 → [dial]:in0
```

Attributes demonstrated: `@bgcolor`, `@blinkcolor`

## See also

`dynamic_colors`, `getattr`, `pattr`
