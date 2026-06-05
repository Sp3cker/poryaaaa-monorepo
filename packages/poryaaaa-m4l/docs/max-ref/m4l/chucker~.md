# chucker~

_msp · MSP Sampling_

> Segment audio and reorder it on looped playback

chucker~ takes a specified amount of audio data, stores the data in an internal buffer, divides the buffered data into equal sections, and allows the sections to be reordered on playback.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Left Audio Input |
| in1 | Right Audio Input |
| in2 | Audio Sync Input |
| out0 | signal: Out left outlet: The segmented and reordered playback of the left channel of the chucker~ object's buffered contents. |
| out1 | Right Audio Output |
| out2 | Current Step Number |

### Port details

**`in2` (Audio Sync Input):** The right inlet of the object accepts a signal input in the range 0. - 1.0 (typically from the right outlet of a groove~ object) to control the playback of the object's buffered contents.

**`out1` (Right Audio Output):** signal: Out middle outlet: The segmented and reordered playback of the right channel of the chucker~ object's buffered contents.

**`out2` (Current Step Number):** signal: Out right outlet: The current step number in the playback cycle is sent out the right outlet as a signal value.

 Note: This is the step number, not the index of the segment being played back.

## Arguments

- **buffer-size-in-ms** (`int`) — Buffer size
  An optional integer argument can be used to set the number of milliseconds to allocate for the recorded sound. (e.g. an argument of 8000 will allocate enough memory for a stereo output of 8 seconds)

 Note: The actual allocation will be larger than what is specified by the argument, since the chucker~ object supports two buffers and provides for double-buffering.

## Messages

- `directions(direction-specifier: list)` — Set playback direction for steps
  The word directions, followed by a 1 or 2 to indicate left or right channel and a list of integer values whose length is equal to the number of steps, sets the direction of playback for the sections. Playback direction is specified as follows:
  1: forward (the default)
  0: mute
  -1: reverse
- `fademode(mode: int)` — Set the fade method
  Sets the fade method for segment smoothing. Modes include:
  0: Classic (same as Max 5)
  1: Pre-fade (fades use buffered audio, fade executes prior to segment transition)
  2: Post-fade (fades use buffered audio, fade executes after segment transition)
- `freeze(buffer-segment: int)` — Loop the current buffer segment on playback
  The message freeze 1 causes the current buffer segment to loop on playback. Sending the message freeze 0 resumes normal playback.
- `nstep(number-of-steps: int)` — Set the number of buffer segments
  The word nsteps, followed by an integer in the range 1 - 64, sets the number of equal portions into which the chucker~ object's internal buffer is segmented for playback.
- `signal` — Function depends on inlet
  In left inlet: Left channel audio input.
  In middle inlet: Right channel audio input.
  In right inlet: An audio signal in the range 0. - 1.0 provides the audio sync input. This task is typically done using a phasor~ object's output as input.
- `smooth(smooth-amount: float)` — Set a crossfade amount between segments
  The word smooth, followed by a floating point number in the range 0. - 1.0, sets an amount of smoothing (crossfading) between the individual segments being reordered for playback.
- `steps(channel and range: list)` — Set the playback order for the sections
  The word steps, followed by a 1 or 2 to indicate left or right channel and a list of integer values whose range is between 1 and the number of steps and whose length is equal to the number of steps, sets the order of playback for the sections.
  e.g. the message steps 1 4 3 2 1, steps 2 1 2 3 4, when sent to a chucker~ object whose number of steps is set to 4, will play the four left channel segments in reverse order, and the right channel in regular order. order

## Help patcher examples

### basic

> Note the allocation is much larger than specified by the argument, because chucker~ supports two buffers, and provides for double-buffering.

> The fademode option determines how smoothing occurs. Legacy mode provides the least latency, but can have some artifacts around bar transitions. Prefade and postfade modes buffer an entire bar, then forces the smoothing fade to occur either before or after the step transition point.

```
Example — [chucker~ 50000]
  fan-in:
    in0 ← [message "nstep $1"]
    in0 ← [groove~ audio 2]
    in0 ← [message "smooth $1"]
    in0 ← [r to-chucker]
    in1 ← [selector~ 2 2]
    in2 ← [groove~ audio 2]
  fan-out:
    out0 → [*~ 0.2]:in0
    out1 → [*~ 0.2]:in0
    out2 → [number~]:in0
```

## See also

`buffer~`, `groove~`
