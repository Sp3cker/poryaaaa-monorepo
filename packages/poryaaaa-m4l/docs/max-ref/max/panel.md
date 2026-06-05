# panel

_max · U/I_

> Colored background area

The panel object lets you create colored panels for use in creating user interfaces. The panel can be a variety of shapes, including circles, triangles, arrows, and rectangles with optional rounded corners. Shadows and gradients can also be used.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Graphics commands |

## Messages

- `size(width (pixels): int, height (pixels): int)` — Specify the size of the panel
  The word size, followed by two numbers, specifies the width and height, in pixels, of the panel object. The default panel size has a width of 69 and a height of 57.

## GUI behaviors

- `(mouse)` — Drags patcher window
  When drag_window is set to 1 and the patcher is locked, click-dragging on the panel will drag the patcher window.

## Attributes

- `@category` (atom)
- `@invisible` (int)
- `@legacydefault` (atom)
- `@obsolete` (int)
- `@paint` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `shadow` — seen as: `shadow -4`, `shadow 5`

## Help patcher examples

### appearance

> Use the format palette to create gradient panels

```
Example — [panel]
  fan-in:
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
    in0 ← [attrui @shadow]    # Gradient mode
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @bgfillcolor]
```

Attributes demonstrated: `@bgfillcolor`, `@bordercolor`, `@shadow`, `@style`

### misc

```
Example #1 — [panel]  positive shadow values look "raised"
  (no patch cords)
```

```
Example #2 — [panel]  negative shadow values look "indented"
  (no patch cords)
```

```
Example #3 — [panel]
  fan-in:
    in0 ← [message "shadow 5"]
    in0 ← [message "shadow -4"]
```

### shapes

```
Example #1 — [panel]
  (no patch cords)
```

```
Example #2 — [panel]
  (no patch cords)
```

```
Example #3 — [panel]
  (no patch cords)
```

```
Example #4 — [panel]
  (no patch cords)
```

```
Example #5 — [panel]
  (no patch cords)
```

```
Example #6 — [panel]
  (no patch cords)
```

```
Example #7 — [panel]  Arrow:
  (no patch cords)
```

```
Example #8 — [panel]
  (no patch cords)
```

```
Example #9 — [panel]  Triangle:
  (no patch cords)
```

```
Example #10 — [panel]
  (no patch cords)
```

```
Example #11 — [panel]  Circle:
  (no patch cords)
```

### basic

```
Example — [panel]
  fan-in:
    in0 ← [attrui @rounded]    # change roundedness
    in0 ← [attrui @shadow]    # change shadow amount
    in0 ← [attrui @border]    # change size of border
```

Attributes demonstrated: `@border`, `@rounded`, `@shadow`

## See also

`fpic`, `jsui`, `lcd`, `textbutton`, `ubutton`, `live.line`
