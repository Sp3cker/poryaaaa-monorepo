# live.routing

_m4l · Live API Objects_

> Route MIDI and Audio Inputs and Outputs in Ableton Live

Route MIDI and Audio data in Ableton Live using available inputs and outputs. live.routing only supports one midi or audio port at a time per Max for Live device. So, if you use two live.routing objects in one Max for Live device, they will both point to the same port. This is because the Live API currently supports only one port at a time per Max for Live device.

 To route audio data through a Max for Live device, you will need to use the plugin~ and plugout~ objects to recieve audio data from Live in Max. Similarly, to route midi data through a Max for Live device, you will need to use the midiin and midiout objects to recieve midi data from Live in Max. You can then build your routing patch in Max using the live.routing object. To learn more about Ableton Live's I/O, look up the Device I/O Object Class in the Live Object Model.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int, messages in |
| out0 | current type |
| out1 | current channel |
| out2 | available types (list) |
| out3 | available channels (list) |
| out4 | dumpout |

### Port details

**`in0` (int, messages in):** messages, int, attributes

**`out0` (current type):** Outputs the index into the list of available types of the chosen type of i/o. In Ableton Live, this is typically the index of a Track name, if you are routing data from a Live Track.

**`out1` (current channel):** Outputs the index into the list of available channels of the chosen channel on the i/o. In Ableton Live, this is typically the index of one of the "Pre Fx", "Post FX" and "Post Mixer" options, if you are routing data from a Live Track.

**`out2` (available types (list)):** Lists all the available types of I/Os in Live.

**`out3` (available channels (list)):** Lists all the available channels of the selected I/Os in Live.

**`out4` (dumpout):** outputs unsupported data.

## Messages

- `channel(input: int)` — Set the I/O channel by its index
  Set the current Audio or MIDI channel to be routed by the specified index number.
- `type(input: int)` — Set the I/O type by its index
  Set the current Audio or MIDI type to be routed by the specified index number.

## Attributes

- `@index` (int) — Set the Audio or MIDI I/O of the current Max device
  Set the Audio or MIDI I/O of the current Max device by its index number.
- `@port` (symbol) — Port Live Audio and MIDI I/Os
  Port Live Audio and MIDI inputs and outputs to Max to enable routing.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `index` — seen as: `index $1`
- `port` — seen as: `port audio_inputs`, `port audio_outputs`, `port midi_inputs`

## Help patcher examples

### route midi output

```
Example — [live.routing]
  fan-in:
    in0 ← [loadmess port midi_outputs] ← [button]
    in0 ← [message "type $1"]
    in0 ← [message "channel $1"]
  fan-out:
    out0 → [p umenuprep]:in0
    out1 → [p umenuprep]:in0
    out2 → [p umenuprep]:in1
    out3 → [p umenuprep]:in1
    out4 → [print]:in0
```

###  route audio output

```
Example — [live.routing]
  fan-in:
    in0 ← [loadmess port audio_outputs] ← [button]    # port audio_outputs from Live
    in0 ← [message "type $1"]
    in0 ← [message "channel $1"]
  fan-out:
    out0 → [p umenuprep]:in0
    out1 → [p umenuprep]:in0
    out2 → [p umenuprep]:in1
    out3 → [p umenuprep]:in1
    out4 → [print]:in0
```

### route midi input

```
Example — [live.routing]
  fan-in:
    in0 ← [loadmess port midi_inputs] ← [button]    # port midi_inputs from Live
    in0 ← [message "type $1"]
    in0 ← [message "channel $1"]
  fan-out:
    out0 → [p umenuprep]:in0
    out1 → [p umenuprep]:in0
    out2 → [p umenuprep]:in1
    out3 → [p umenuprep]:in1
    out4 → [print]:in0
```

### route audio input

```
Example — [live.routing]
  fan-in:
    in0 ← [loadmess port audio_inputs] ← [button]
    in0 ← [message "type $1"]
    in0 ← [message "channel $1"]
  fan-out:
    out0 → [p umenuprep]:in0
    out1 → [p umenuprep]:in0
    out2 → [p umenuprep]:in1
    out3 → [p umenuprep]:in1
    out4 → [print]:in0
```

### basic

```
Example — [live.routing]
  fan-in:
    in0 ← [message "port midi_outputs"]
    in0 ← [message "port audio_inputs"]
    in0 ← [message "channel $1"]
    in0 ← [message "index $1"]
    in0 ← [message "type $1"]
    in0 ← [message "port midi_inputs"]
    in0 ← [message "port audio_outputs"]
  fan-out:
    out0 → [print type]:in0
    out1 → [print channel]:in0
    out2 → [print "list types"]:in0
    out3 → [print "list channels"]:in0
    out4 → [print dumpout]:in0
```

## See also

`JS API`, `m4l/live_api_overview`, `m4l/live_api`, `Live Object Model`, `live.object`, `live.observer`, `live.path`, `live.banks`
