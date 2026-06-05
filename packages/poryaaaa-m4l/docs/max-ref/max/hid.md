# hid

_max · Devices_

> Human Interface Device input (modern)

Provides input from human interface peripherals (i.e. Trackpad, Keyboard, and others). The hid object is similar to the legacy hi object but is cross platform with more information and capabilities. This includes providing usage pages, usages, types, and ranges for the information received. (see

 https://www.usb.org/hid

 for more information)

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | message | control messages |
| out0 | list | hid report element info and value |
| out1 | message | device enumeration output in menu format |
| out2 | message | dumpoutlet |

### Port details

**`out0` (hid report element info and value):** A list containing the following information about the HID report element: report_id usage_page usage_page_name usage usage_name value size type io_flags logical_min logical_max phys_min phys_max unit_exp unit unit_name element_index

**`out2` (dumpoutlet):** The begin_report and end_report messages can be used to detect and monitor the beginning and end of a given report.

## Arguments

- **device** (`symbol`) _(optional)_ — Selected input device
  An argument can be used to specify the object for focus on the hid object.

## Messages

- `bang` — Output device events
  bang message will output the current event queue.
- `int(index: int)` — Select an input device
  An incoming int causes the object to focus on the device in the device list with that index.
- `anything(device: list)` — Set the input device
  Sending the name of any device to the hid object will set the object to focus on the specified device.
- `close` — Close the currently open device
- `info` — Print device information to the Max Console
  The info message causes device information to be output to the Max console.
- `menu` — Output a device list in menu format
  The menu message causes a device list to be output from the right outlet in a format fit for a umenu object. On Windows, Onboard and or bluetooth HID devices (keyboards, mouse) are unsupported as available devices.
- `poll(output-time: float)` — Set automatic polling
  The word poll, followed by a number, sets the time in milliseconds between outputs of the event queue. The message poll 0 disables automatic polling.

## Attributes

- `@exclusive` (int) — Open device for exclusive input (Mac OS only)
  On Mac OS, the

 exclusive

 enables to use devices such as mice, keyboards, graphics tablets, etc. exclusively. This is useful when one wishes the input to only be sent to Max and not control the online mouse cursor or trigger system key events, etc. Be careful when using that you will not prevent yourself from using you rmouse or keyboard to disable or close your patch.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `Keyboard` — seen as: `Keyboard`

## Help patcher examples

### basic

> these example subpatchers show how to convert these reports into a packed list of values, or dictionaries with additional information.

```
Example — [hid]
  fan-in:
    in0 ← [p exclusive]    # p exclusive emits: "exclusive $1"
    in0 ← [message "poll 0"]
    in0 ← [button]
    in0 ← [umenu] ← [hid]    # umenu filled by 'menu' message
    in0 ← [message "menu"]    # generate a menu of available devices
    in0 ← [message "info"]
    in0 ← [number]
    in0 ← [message "poll 10"]    # output queue every 10 ms / output event queue
    in0 ← [message "Keyboard"]    # change device with index number / change device with name / don't poll the output queue
  fan-out:
    out0 → [p pack_simple_dictionary]:in0
    out0 → [p pack_full_dictionary]:in0
    out0 → [p pack_tight_values]:in0
    out1 → [umenu]:in0    # umenu filled by 'menu' message
    out1 → [print menu]:in0
    out2 → [route report_begin report_end]:in0
    out2 → [p pack_simple_dictionary]:in1
    out2 → [p pack_full_dictionary]:in1
    out2 → [p pack_tight_values]:in1
```

## See also

`hi`, `gamepad`, `key`, `keyup`
