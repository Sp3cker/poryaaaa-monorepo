# live.line

_m4l · Live UI Objects_

> Straight line

live.line displays a straight line. It is useful for delimiting zones in your interface.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Messages in |

## Attributes

- `@arrows` (int) — Display Arrow(s)
  Sets the display mode for the arrows.
- `@label` (symbol)
- `@paint` (int)
- `@save` (int)
- `@style` (symbol)

## Help patcher examples

### orientation

> Unlock patcher to see object boxes

```
Example #1 — [live.line]
  (no patch cords)
```

```
Example #2 — [live.line]
  (no patch cords)
```

```
Example #3 — [live.line]  Forced orientation
  (no patch cords)
```

```
Example #4 — [live.line]
  (no patch cords)
```

```
Example #5 — [live.line]
  (no patch cords)
```

```
Example #6 — [live.line]
  (no patch cords)
```

```
Example #7 — [live.line]  Automatic orientation based on object box dimensions
  (no patch cords)
```

```
Example #8 — [live.line]
  (no patch cords)
```

### appearance

```
Example #1 — [live.line]
  fan-in:
    in0 ← [attrui @linecolor]
```

```
Example #2 — [live.line]
  fan-in:
    in0 ← [attrui @linecolor]
```

```
Example #3 — [live.line]
  fan-in:
    in0 ← [attrui @linecolor]
```

```
Example #4 — [live.line]
  fan-in:
    in0 ← [attrui @linecolor]
```

Attributes demonstrated: `@linecolor`

### arrows

```
Example #1 — [live.line]
  (no patch cords)
```

```
Example #2 — [live.line]
  (no patch cords)
```

```
Example #3 — [live.line]
  (no patch cords)
```

```
Example #4 — [live.line]
  (no patch cords)
```

```
Example #5 — [live.line]
  (no patch cords)
```

### basic

```
Example #1 — [live.line]
  (no patch cords)
```

```
Example #2 — [live.line]  as a delimiter in your device
  (no patch cords)
```

```
Example #3 — [live.line]  use live.line
  (no patch cords)
```

## See also

`panel`
