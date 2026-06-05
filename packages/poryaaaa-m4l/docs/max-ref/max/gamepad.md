# gamepad

__

> Report gamepad controller events

Tracks and outputs the button, joystick, trigger, and sensor events from all connected gamepad controllers. Send a device control messages for haptic feedback rumble events and device led color with the senddevice message. The gamepad object is a lightweight wrapper for the Simple DirectMedia Layer 2.0 Library's gamepad implementation.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | message | control messages |
| out0 | list | event message |
| out1 | list | device info for event (instance_id device_index controller_name controller_type) |
| out2 | message | dumpoutlet |

### Port details

**`in0` (control messages):** Send a device control messages for haptic feedback rumble events and device led color with the senddevice message.

**`out1` (device info for event (instance_id device_index controller_name controller_type)):** As gamepad controllers generate events, the info for the event is output as a list of the form instance_id device_index controller_name controller_type

## Messages

- `addmapping(mapping_text: symbol)` — Add SDL controller mapping from a symbol
  Add SDL controller mapping from a symbol. You can find example controller mappings from the excellent
  SDL_GameControllerDB
  community database.
- `addmappingfile(filename: symbol)` — Add SDL controller mappings from a file
  Add SDL controller mappings from a file such as gamecontrollerdb.txt from the excellent
  SDL_GameControllerDB
  community database. If you place a file named "gamecontrollerdb.txt" in the Max search path, gamepad will load the mapping file prior initializing SDL with the first gamepad instance created. This can work better for certain controllers than loading the mapping file after creation.
- `senddevice(Device ID: int, Message: symbol, Value: list)` — Send a device control messages for haptic feedback rumble events and device led color
  The word senddevice followed by a number indicating the logical device ID will send the following message on to that device. If the device index is negative, the following message is sent to all devices. Possible messages are led, rumble, and rumbletriggers.
  The led message is followed by red, green and blue values in the range 0-1.
  The rumble message is followed by low high and duration values, where low represents the low frequency rumble amount in the range 0-1, high represents the high frequency rumble amount in the range 0-1, and duration is in milliseconds.
  The rumbletriggers message is followed by left right and duration values, where left represents the left trigger rumble amount in the range 0-1, right represents toe right trigger rumble amount in the range 0-1, and duration is in milliseconds.
- `sendinstance(Instance ID: int, Message: symbol, Value: list)` — Send a device control messages for haptic feedback rumble events and device led color
  The sendinstance message has the same functionality of the senddevice message, but referenced using the unique gamepad instance id rather than the logical device ID.

## Attributes

- `@interval` (float) — Global polling interval (ms)
  The global polling interval sets the rate at which the incoming gamepad controller event queue is being serviced. The default is 5 milliseconds. Changing this attribute will change it for all gamepad controller objects in all patches.
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### advanced

> All gamepad objects are fed by a single global gamepad instance that sends events to the individual gamepad objects in a patcher. Attributes like interval or raw data will apply to this global object, and therefore for all instances in patchers. SDL 2 takes raw joystick data from various drivers and maps it to a gamepad based on its internal mappings. Users can create their own mapping for a given device specifed by GUID and platform. Mappings can be loaded by file with the addmappingfile <filename> message or addmapping <mapping text>. The community sourced Game Controller DB Project has many additional and example mappings. If a file named gamecontrollerdb.txt is in the Max search path it will be loaded on first gamepad instantiation. https://github.com/mdqinc/SDL_GameControllerDB

> Output raw SDL joystick data, rather than gamepad mapped data. This can be helpful for generating a device mapping, or diagnosing gamepad device mapping issues. This applies to all devices and all instances.

```
Example — [gamepad]
  fan-in:
    in0 ← [attrui @rawdata]
    in0 ← [attrui @interval]    # polling interval (ms)
    in0 ← [p more mappings]
  fan-out:
    out0 → [p filter sensors]:in0
    out1 → [unpack 0 0 name type guid]:in0
```

Attributes demonstrated: `@autoscroll`, `@interval`, `@rawdata`

### basic

```
Example — [gamepad]
  fan-in:
    in0 ← [p control_device_state]    # p control_device_state emits: "senddevice 0 rumble 1 1 1000" | "senddevice 0 rumbletriggers 1 1 1000" | "senddevice 0 led 1. 0.5 0.2"
  fan-out:
    out0 → [p output_messages]:in0
    out0 → [jsui]:in0
    out0 → [p event_log]:in1
    out1 → [unpack 0 0 name type guid]:in0
    out1 → [p event_log]:in0
    out2 → [print dumpout]:in0
```

## See also

`hi`, `key`, `keyup`
