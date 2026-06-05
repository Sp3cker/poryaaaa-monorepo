# mousestate

_max · Interaction_

> Report the mouse information

Provides button status and cursor position information about the mouse/cursor when the cursor is positioned within a Max patcher window. The mouse buttons are sampled every 50ms, while the mouse position is sampled every input bang.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang Triggers Mouse Location Reporte |
| out0 | 1 if Button Down, 0 if Up |
| out1 | Mouse Horizontal Location |
| out2 | Mouse Vertical Location |
| out3 | Mouse Delta Horizontal |
| out4 | Mouse Delta Vertical |
| out5 | 1 if Middle Button Down, 0 if Up |
| out6 | 1 if Right Button Down, 0 if Up |
| out7 | Mousewheel Delta Horizontal |
| out8 | Mousewheel Delta Vertical |
| out9 | Mousewheel Flags (smooth, inertial |

## Messages

- `bang` — Output mouse information
  Sends out the current horizontal and vertical coordinates of the location of the mouse, as well as the change in location since the last output.
- `mode(input: int)` — Set coordinate reference mode
  The word mode, followed by a long value specifices the type of reference to use for the mouse coordinates from the second and third outlets. A value of 0 specifies to use screen-relative coordinates where 0,0 is the top left corner of the primary display. A value of 1 specifies patcher-relative coordinates where 0,0 is the top left corner of the content area of the mousestate object's patcher. A value of 2 specifies front-most patcher relative coordinates where 0,0 is the top left corner of the content area of the top patcher window.
- `nopoll` — Turn off mouse polling
  Undoes a poll message, reverting mousestate to its normal condition of waiting for a bang before reporting.
- `poll` — Set output on mouse movement
  Causes mousestate to send out the mouse location, and the change in mouse location, whenever the mouse is moved, as well as when a bang is received.
- `reset` — Reset the location origin
  Resets the 0,0 point to its default setting, in the upper left corner of the screen.
- `zero` — Set the location origin
  Resets the point mousestate considers as the 0,0 point from which to measure the mouse location. The current location of the mouse is considered the new 0,0 point.

## GUI behaviors

- `(mouse)` — TEXT_HERE

## Help patcher examples

### basic

```
Example — [mousestate]
  fan-in:
    in0 ← [message "poll"]
    in0 ← [message "zero"]
    in0 ← [message "reset"]    # set the 0,0 point from which to measure the mouse location
    in0 ← [message "mode 2"]    # mouse position relative to frontmost patcher / mouse position relative to mousestate's patcher
    in0 ← [message "mode 1"]    # mouse position relative to screen
    in0 ← [message "mode 0"]    # resets to 0,0 point to its default (upper left corner)
  fan-out:
    out0 → [toggle]:in0    # Left Button
    out1 → [number]:in0    # Hor. Position
    out2 → [number]:in0    # Ver. Position
    out3 → [number]:in0    # Hor. Delta
    out4 → [number]:in0    # Ver. Delta
    out5 → [toggle]:in0    # Middle Button
    out6 → [toggle]:in0    # Right Button
    out7 → [flonum]:in0    # Wheel DeltaX
    out8 → [flonum]:in0    # Wheel DeltaY
    out9 → [message "0 0"]:in1    # Wheel Flags (smooth, inertial)
```

## See also

`modifiers`, `mousefilter`
