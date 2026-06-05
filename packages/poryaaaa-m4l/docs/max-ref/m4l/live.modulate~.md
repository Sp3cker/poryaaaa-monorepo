# live.modulate~

_m4l · Live MSP Objects_

> Modulate Ableton Live and Max for Live Parameters

The live.modulate~ object enables you to modulate the value of Live parameters in realtime using signal objects. To understand more about Live's parameters, look up the DeviceParameter Object Class in the Live Object Model.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | value (signal/float) clipped to [-1, 1] |
| in1 | id in |
| out0 | status |

## Messages

- `int(value: int)` — Send a value to the selected Live Parameter
  An integer number value received in the left inlet will be applied to the selected Live parameter (DeviceParameter Object), if any, at the beginning of the next audio buffer, or at the end of a pending ramp (see smoothing).
- `float(value: float)` — Send a decimal value to the selected Live Parameter
  A floating point number value received in the left inlet will be applied to the selected Live parameter (DeviceParameter Object), if any, at the beginning of the next audio buffer, or at the end of a pending ramp (see smoothing).
- `list(target-value: float, delta-time: number)` — Start a ramp
  Start a ramp with a list of two floats, similar to the line~ object. Sending in “1 500” means that the value 1 will be reached in 500 ms, starting at the current value. New ramps will always override the current ramp, so if you want to cut short a ramp, send another value.
- `getid` — Report the mapped object's id
  The mapped object's id is sent from the outlet, preceded by the word id. If there is no mapped object, id 0 will be sent.
- `id(parameter id: int)` — Set the Live object using it's id nn
  In right inlet: Sets the selected Live object. The message has no effect if the id is not a parameter (DeviceParameter Object).
- `signal` — Send signal values to the selected Live Parameter
  Signal input values received in the left inlet will be applied to the selected parameter (DeviceParameter Object), if any, in realtime.

## Attributes

- `@label` (symbol)
- `@save` (int)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `depth` — seen as: `depth $1`

## Help patcher examples

### transport sync

```
Example — [live.modulate~]
  fan-in:
    in0 ← [cycle~] ← [phasor~ 4n @lock 1] ← [live.menu]    # Create a phasor synced the quarter notes on the host transport. / Select a tempo-relative time value
    in1 ← [message "id 8"]
```

### unipolar

> live.modulate~ only functions when used inside of Max for Live devices.

> You might not always want bipolar modulation. Enabling unipolar modulation is simple enough, we just need to scale the modulation source accordingly so that the modulation which is applied is just in one direction from the current value.

```
Example — [live.modulate~]
  fan-in:
    in0 ← [gen~ @t "unipolar modulation"]
    in1 ← [message "id 8"]
```

### multiplicative

> live.modulate~ only functions when used inside of Max for Live devices.

> live.modulate~ adds an offset to the "nominal" value of a parameter, i.e the current value. Using the Live API, we can get the minimum and maximum value for that parameter and patch together a mode in which the modulation source modulates the parameter between the minimum and the current value. This "multiplicative" modulation can be useful for parameters where you don't want to exceed the current value, such as for Volume / Gains Faders.

```
Example — [live.modulate~]
  fan-in:
    in0 ← [gen~ @t "multiplicative modulation"]
    in1 ← [t b l] ← [message "id 6"]
```

### scaling

> live.modulate~ receives a signal between -1 and 1 in the left inlet. The amount of modulation applied depends on the modulation type of the parameter. For bipolar modulation, the input range (-1 to 1) is equal to the range of the parameter (minimum - maximum). This means if you're current value on the parameter, for example a dial, is set to the minimum, a modulation value of 1.0 will result in modulated value at the parameter's maximum. For unipolar modulation

```
Example — [live.modulate~]
  fan-in:
    in0 ← [cycle~ 0.2]
    in1 ← [message ""]    # live.modulate~ will map to a parameter with valid Live id. Sending "id 0" will unmap live.modulate~
```

### depth

> live.modulate~ only functions when used inside of Max for Live devices.

```
Example — [live.modulate~ @depth 0.5]
  fan-in:
    in0 ← [message "depth $1"]
    in0 ← [cycle~ 1]
    in1 ← [message ""]
```

### ramps

```
Example — [live.modulate~ @smoothing 100]
  fan-in:
    in0 ← [message "0 500"]
    in0 ← [message "1 500"]
    in1 ← [message ""]
```

### mapping

> The live.modulate~ object reports if the mapping was successful or not from the first outlet. If the mapping was successful it outputs "mapping 1", if it is unsuccessful it outputs "mapping 0". You can use this to handle instances where someone tries to map to a parameter that is already mapped, for example.

```
Example — [live.modulate~]
  fan-in:
    in0 ← [bpatcher] ← [cycle~ 1]    # live.modulate~ only functions when used inside of Max for Live devices.
    in1 ← [message "id 4"]
  fan-out:
    out0 → [route mapped]:in0
```

### smoothing

```
Example — [live.modulate~ @smoothing 100]  A smoothing value of 0 ms will allow sample accurate control, but can result in performance problems.
  fan-in:
    in0 ← [attrui @smoothing]
    in0 ← [cycle~ 1]    # live.modulate~ only functions when used inside of Max for Live devices.
    in1 ← [message ""]
```

Attributes demonstrated: `@smoothing`

### basic

```
Example — [live.modulate~]  live.modulate~ has an input range of -1 to 1 which maps onto the full range of the modulated parameter
  fan-in:
    in0 ← [cycle~ 1] ← [live.dial] ← [loadmess 1.]
    in1 ← [message "id 1"]
    in1 ← [message "id 0"]    # stop modulation / unmap
```

## See also

`JS API`, `m4l/live_api_overview`, `m4l/live_api`, `Live Object Model`, `live.remote~`, `live.object`, `live.path`, `live.map`, `live.observer`
