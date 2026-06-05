# live.remote~

_m4l · Live MSP Objects_

> Realtime control of parameters in Ableton Live and Max for Live.

The
	live.remote~
 object allows you to remotely control parameters in Ableton Live and Max for Live in realtime.
 To understand more about Live's parameters, look up the DeviceParameter Object Class in the
 Live Object Model
 .

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | value (signal/float) |
| in1 | id
 nn |

### Port details

**`in0` (value (signal/float)):** Sets the value of the parameter object specified by id in the right
 inlet. For the valid range, refer to the min and max properties of the
 target parameter. The value curve is linear to the parameter's GUI
 control in Live.

**`in1` (id
 nn):** Sets object id in the format

 id
 nn

 to select the target parameter (DeviceParameter Object) in Live and Max for Live to control.

 id 0
 means no object, i.e. the remote stops controlling the target parameter. This is also the initial state.

## Messages

- `float(value: float)` — Send a decimal value to the selected Live Parameter
  A floating point number value received in the left inlet will be
  applied to the selected Live parameter (DeviceParameter Object), if any, at the beginning of the next audio buffer, or at the end of a pending ramp (see smoothing).
- `int(value: int)` — Send a value to the selected Live Parameter
  An integer number value received in the left inlet will be applied to
  the selected Live parameter (DeviceParameter Object), if any, at the beginning of the next audio buffer, or at the end of a pending ramp (see smoothing).
- `list(target-value: float, delta-time: number)` — Start a ramp
  Start a ramp with a list of two floats, similar to the line~ object. Sending in “1 500” means that the value 1 will be reached in 500 ms,
  starting at the current value. New ramps will always override the current ramp, so if you want to cut short a ramp, send another value.
- `signal` — Send signal values to the selected Live Parameter
  Signal input values received in the left inlet will be applied to the
  selected parameter (DeviceParameter Object), if any, in realtime.
- `id(parameter id: int)` — Set the Live object using it's id nn
  In right inlet: Sets the selected object. The message has
  no effect if the id is not a parameter (DeviceParameter Object).
- `getid` — Report the mapped object's id
  The mapped object's id is sent from the outlet, preceded by the word id. If there is no mapped object, id 0 will be sent.

## Attributes

- `@label` (symbol)
- `@save` (int)

## Help patcher examples

### mapping

> click the Map button and then click a parameter in Ableton Live to retrieve the Live ID.

> The live.remote~ object reports if the mapping was successful or not from the first outlet. If the mapping was successful it outputs "mapping 1", if it is unsuccessful it outputs "mapping 0". You can use this to handle instances where someone tries to map to a parameter that is already mapped, for example.

```
Example — [live.remote~ @normalized 1]
  fan-in:
    in0 ← [bpatcher] ← [cycle~ 1]    # live.remote~ only functions when used inside of Max for Live devices.
    in1 ← [message "id 11"]
  fan-out:
    out0 → [route mapping]:in0
```

### normalized

> live.remote~ only functions when used inside of Max for Live devices.

```
Example — [live.remote~]
  fan-in:
    in0 ← [attrui @normalized]    # set @normalized to 1 to scale input values to the target parameter range.
    in0 ← [selector~ 3]
    in1 ← [live.path] ← [loadmess goto this_device parameters 2]
```

Attributes demonstrated: `@normalized`

### smoothing

> setting @smoothing to 0 ms will allow sample accurate control, but can result in performance problems

```
Example — [live.remote~]
  fan-in:
    in0 ← [attrui @smoothing]    # set @smoothing to a number above 0. to affect the time value.
    in0 ← [message "0 500"]
    in0 ← [message "1 500"]    # send a ramp message by sending a list "x y" which means reach "value x" in "time y"
    in0 ← [phasor~ 0.2]    # live.remote~ only functions when used inside of Max for Live devices.
    in1 ← [live.path] ← [loadmess path live_set this_device parameters 0]
```

Attributes demonstrated: `@smoothing`

### basic

```
Example — [live.remote~]
  fan-in:
    in0 ← [cycle~ 0.2]    # control a parameter with an LFO
    in1 ← [message "id 0"]
    in1 ← [live.path] ← [loadmess path live_set tracks 0 mixer_device panning]    # set the path to a parameter (eg. panning of the first track)
```

## See also

`JS API`, `m4l/live_api_overview`, `m4l/live_api`, `Live Object Model`, `live.object`, `live.observer`, `live.path`, `live.modulate~`
