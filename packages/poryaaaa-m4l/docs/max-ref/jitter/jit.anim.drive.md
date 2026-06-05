# jit.anim.drive

_jit · Jitter Animation_

> Animate a 3D transform

Animates relative transforms over time with easing. The jit.anim.drive object works in conjunction with jit.anim.node and OpenGL objects to perform this function. Double clicking the object will open a ui mapping dictionary, allowing certain user interface actions to be mapped to animation messages.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages in |
| out0 | messages to anim.node objects |
| out1 | dumpout |

## Messages

- `bang` — Output messages if animating
  Causes the animation messages to be output, if currently animating.
- `anim_reset` — Disable all animations
- `dictionary(name: symbol)` — Dictionary input to set the ui mapping
  The incoming dictionary is cloned and used to set the ui mapping.
- `grow(x: float, y: float, z: float)` — Scale along axis
  Scale the attached object along each axis by the amount provided. The axis arguments are multiplied by speed.
- `move(x: float, y: float, z: float)` — Move along axis
  Move the attached object along the axis provided, relative to the space indicated by the objects's movemode attribute. The axis arguments are multiplied by speed.
- `moveto(x: float, y: float, z: float, duration: float)` — Move to a position over a specified time
  Move the attached object to the specified position over the specified length of time (in seconds). Easing in and out is applied to the movement based on the easefunc attribute.
- `rotateto(x: float, y: float, z: float, w: float, duration: float)` — Rotate over a specified time
  Rotate the attached object to the specified quaternion orientation over the specified length of time (in seconds). Easing in and out is applied to the rotation based on the easefunc attribute.
- `scaleto(x: float, y: float, z: float, duration: float)` — Scale over a specified time
  Scale the attached object to the specified scale value over the specified length of time (in seconds). Easing in and out is applied to the scaling based on the easefunc attribute.
- `springto(x: float, y: float, z: float)` — Move to a position using mass-spring simulation
  Move the attached object to the specified position using a mass-spring simulation.
- `turn(x: float, y: float, z: float)` — Rotate along axis
  Rotate the attached object along each axis by the amount provided. The axis arguments are multiplied by speed.
- `ui_dict` — See the dictionary listing
- `ui_key(key: symbol)` — Send the specified key
  Send the specified key. If the key is in the ui mapping dictionary, the corresponding animation will be enabled.
- `ui_keyup(key: symbol)` — Send the specified keyup
  Send the specified keyup. If the key is in the ui mapping dictionary, the corresponding animation will be disabled.
- `ui_mouse(screen-x: int, screen-y: int, mouse-down: int)` — Send the mouse event
  Send the mouse event. If the event is in the ui mapping dictionary, the corresponding animation will be enabled.
- `update_drive` — See the bang listing

## GUI behaviors

- `(mouse)` — Mouse events

## Attributes

- `@label` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — dumpout

### ui

> this value will be stored as an application preference affecting all jit.anim.drive objects

```
Example — [jit.anim.drive]  double click to open ui-map editor
  fan-in:
    in0 ← [attrui @ui_dict_layout]    # set the default keyboard layout for the ui-map
    in0 ← [attrui @easefunc]
    in0 ← [attrui @ease]
    in0 ← [attrui @speed]    # controlling a gl.camera with anim.drive
    in0 ← [attrui @ui_listen]    # disable the gl.handle to interact with the window using the mouse / enable ui_listen, focus on jit.window, and use keyboard and mouse
    in0 ← [message "anim_reset"]
  fan-out:
    out0 → [jit.gl.camera drive-ctx]:in0
```

Attributes demonstrated: `@ease`, `@easefunc`, `@speed`, `@ui_dict_layout`, `@ui_listen`

### basic

```
Example — [jit.anim.drive]
  fan-in:
    in0 ← [attrui @easefunc]
    in0 ← [attrui @ease]
    in0 ← [attrui @speed]
    in0 ← [message "move 0 0 1"]
    in0 ← [message "turn 0 1 0"]
    in0 ← [message "grow 1 1 1"]
    in0 ← [r todrive]
    in0 ← [message "anim_reset"]
  fan-out:
    out0 → [jit.gl.gridshape @shape torus @scale 0.2 0.2 0.2]:in0
```

Attributes demonstrated: `@animmode`, `@ease`, `@easefunc`, `@enable`, `@speed`

## See also

`jit.anim.node`, `jit.anim.path`, `jit.gl.camera`
