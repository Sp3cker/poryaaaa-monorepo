# hi

_max · Devices_

> Human Interface device input (legacy)

Provides input from human interface peripherals (i.e. Trackpad, Keyboard, and others).

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | message | (message) control messages |
| out0 | list | (list) element and value, both integers |
| out1 | message | (message) device enumeration output in menu format |

## Arguments

- **device** (`symbol`) _(optional)_ — Selected input device
  An argument can be used to specify the object for focus on the hi object.

## Messages

- `bang` — Output device events
  bang message will output the current event queue.
- `int(index: int)` — Select an input device
  An incoming int causes the object to focus on the device in the device list with that index.
- `anything(device: list)` — Set the input device
  Sending the name of any device to the hi object will set the object to focus on the specified device.
- `clear` — Reset ignore and delta settings
  The message clear will reset all values set using the ignore and delta messages to their default values.
- `delta(element: int)` — Only report changed data
  The word delta, followed by an integer that represents an element of the device will cause the hi object to report an event from the specified element only if it is different then the last value that was reported.
- `ignore(element: int)` — Disable event reporting
  The word ignore, followed by an integer that represents an element of the device, disables event reporting from the specified element.
- `info` — Print device information to the Max Console
  The info message causes device information to be output to the Max console.
- `menu` — Output a device list in menu format
  The menu message causes a device list to be output from the right outlet in a format fit for a umenu object. On Windows, Onboard and or bluetooth HID devices (keyboards, mouse) are unsupported as available devices.
- `poll(output-time: float)` — Set automatic polling
  The word poll, followed by a number, sets the time in milliseconds between outputs of the event queue. The message poll 0 disables automatic polling.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `Keyboard` — seen as: `Keyboard`

## Help patcher examples

### basic

> On Windows, Onboard and or bluetooth HID devices (keyboards, mouse) are unsupported as available devices for the Hi Object.

```
Example — [hi]
  fan-in:
    in0 ← [message "delta 5"]    # don't output zero data from element 5
    in0 ← [message "ignore 10"]    # don't output data from element 10 / don't poll the output queue
    in0 ← [message "poll 0"]
    in0 ← [message "clear"]    # clear ignore and delta lists
    in0 ← [button]    # output event queue
    in0 ← [message "poll 10"]    # output queue every 10 ms
    in0 ← [umenu] ← [hi]    # umenu filled by 'menu' message
    in0 ← [message "menu"]    # generate a menu of available devices / output device info to max window
    in0 ← [message "info"]
    in0 ← [number]    # change device with index number
    in0 ← [message "Keyboard"]    # change device with name
  fan-out:
    out0 → [print hi @popup 1]:in0
    out0 → [message "3 1"]:in1
    out1 → [umenu]:in0    # umenu filled by 'menu' message
```

## See also

`hid`, `gamepad`, `key`, `keyup`
