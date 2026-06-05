## **Live Object Model**

Version 9.1.4-rev.0

cycling74.com
## **Contents**

## **LOM - The Live Object Model**

_Objects which comprise the Live API described by their structure, properties and functions._ The Live Object Model lists a number of Live object classes with their properties and functions, as well as their parent-child relations through which a hierarchy is formed. Please refer to the Live API overview chapterfor definitions of the basic Live API terms and a list of the Max objects used to access it.

_This document refers to Ableton Live version 12.3.5_

## **API Objects**

|Item|Description|
|---|---|
|Application|This class represents the Live application. It is reachable by the root path<br>live_app ...|
|Application.View|This class represents the aspects of the Live application related to viewing<br>the application....|
|Chain|This class represents a group device chain in Live.|
|ChainMixerDevice|This class represents a chain's mixer device in Live.|
|Clip|This class represents a clip in Live. It can be either an audio clip or a MIDI<br>clip in the Arr...|
|Clip.View|Representing the view aspects of a Clip.|
|ControlSurface|A ControlSurface can be reached either directly by the root path<br>control_surfaces N or by g...|
|CuePoint|Represents a locator in the Arrangement View.|
|Device|This class represents a MIDI or audio device in Live.|
|Device.View|Representing the view aspects of a Device.|
|DeviceIO|This class represents an input or output bus of a Live device.|
|DeviceParameter|This class represents an (automatable) parameter within a MIDI or audio<br>device. To modify a de...|
|Groove|This class represents a groove in Live. Available since Live 11.0. ...|
|GroovePool|This class represents the groove pool in Live. It provides access to the<br>current set's list of groov...|
|MaxDevice|This class represents a Max for Live device in Live. A MaxDevice is a<br>type of Device...|
|MixerDevice|This class represents a mixer device in Live. It provides access to<br>volume, panning and other ...|
|RackDevice|This class represents a Live Rack Device. A RackDevice is a type of<br>Device, meaning th...|
|RackDevice.View|Represents the view aspects of a Rack Device. A RackDevice.View is<br>a type of Device.Vi...|
|Song|This class represents a Live Set. The current Live Set is reachable by<br>the root path li...|
|Song.View|This class represents the view aspects of a Live document: the<br>Session and Arrangement Views....|
|TakeLane|This class represents a take lane in Live. Tracks in Live can have take lanes<br>in Arrangement View, w...|
|this_device|This root path represents the device containing the live.path object to<br>which the ...|
|Track|This class represents a track in Live. It can either be an audio track, a MIDI<br>track, a return...|
|Track.View|Representing the view aspects of a track.|

## **Application.View**

This class represents the aspects of the Live application related to viewing the application.

## **Canonical Path**

```
live_app view
```

## **Properties**

## **browse_mode** bool

observe read-only

1 = Hot-Swap Mode is active for any target.

## **focused_document_view** unicode

## observe read-only

The name of the currently visible view in the focused Live window ('Session' or 'Arranger').

## **Functions**

## **available_main_views**

Returns: `view names` [list of symbols].

This is a constant list of view names to be used as an argument when calling other functions: `Browser Arranger Session Detail Detail/Clip Detail/DeviceChain` .

## **focus_view**

Parameter: `view_name`

Shows named view and focuses on it. You can also pass an empty view_name “ ", which refers to the Arrangement or Session View (whichever is visible in the main window).

## **hide_view**

Parameter: `view_name`

Hides the named view. You can also pass an empty view_name “ ", which refers to the Arrangement or Session View (whichever is visible in the main window).

## **is_view_visible**

Parameter: `view_name`

Returns: [bool] Whether the specified view is currently visible.

## **scroll_view**

Parameters: `direction view_name modifier_pressed`

`direction` [int] is 0 = up, 1 = down, 2 = left, 3 = right

`modifier_pressed` [bool] If view_name is "Arranger" and modifier_pressed is 1 and direction is

left or right, then the size of the selected time region is modified, otherwise the position of the playback cursor is moved.

Not all views are scrollable, and not in all directions. Currently, only the `Arranger` , `Browser` ,

`Session` , and `Detail/DeviceChain` views can be scrolled.

You can also pass an empty view_name `" "` , which refers to the Arrangement or Session View (whichever view is visible).

## **show_view**

Parameter: `view_name`

## **toggle_browse**

Displays the device chain and the browser and activates Hot-Swap Mode for the selected device. Calling this function again deactivates Hot-Swap Mode.

## **zoom_view**

Parameter: `direction view_name modifier_pressed`

`direction` [int] - 0 = up, 1 = down, 2 = left, 3 = right

`modifier_pressed` [bool] If `view_name` is 'Arrangement', `modifier_pressed` is 1, and `direction` is left or right, then the size of the selected time region is modified, otherwise the

position of the playback cursor is moved. If `view_name` is Arrangement and `modifier_pressed` is 1 and `direction` is up or down, then only the height of the highlighted track is changed, otherwise the height of all tracks is changed.

Only the Arrangement and Session Views can be zoomed. For Session View, the behaviour of zoom_view is identical to scroll_view. You can also pass an empty view_name “ ", which refers to the Arrangement or Session View (whichever is visible in the main window).

## **Application**

This class represents the Live application. It is reachable by the root path `live_app` .

## **Canonical Path**

```
live_app
```

## **Children**

read-only

**view** Application.View observe read-only **control_surfaces** list of ControlSurface

A list of the control surfaces currently selected in Live's Preferences.

If None is selected in any of the slots or the script is inactive (e.g. when Push2 is selected, but no Push is connected), id 0 will be returned at those indices.

## **Properties**

read-only **current_dialog_button_count** int

The number of buttons in the current message box. **current_dialog_message** symbol

read-only

The text of the current message box (empty if no message box is currently shown).

## **open_dialog_count** int

observe read-only

The number of dialog boxes shown.

## **average_process_usage** float

observe read-only

Reports Live's average CPU load.

Note that Live's CPU meter shows the audio processing load but not Live's overall CPU usage.

observe read-only **peak_process_usage** float

Reports Live's peak CPU load.

Note that Live's CPU meter shows the audio processing load but not Live's overall CPU usage.

## **Functions**

## **get_bugfix_version**

Returns: the 2 in Live 9.1.2.

## **get_document**

Returns: the current Live Set.

## **get_major_version**

Returns: the 9 in Live 9.1.2.

## **get_minor_version**

Returns: the 1 in Live 9.1.2.

## **get_version_string**

Returns: the text 9.1.2 in Live 9.1.2.

## **press_current_dialog_button**

Parameter: `index`

Press the button with the given index in the current dialog box.

## **Chain**

This class represents a group device chain in Live.

## **Canonical Paths**

```
live_set tracks N devices M chains L
```

```
live_set tracks N devices M return_chains L
```

```
live_set tracks N devices M chains L devices K chains P ...
```

```
live_set tracks N devices M return_chains L devices K chains P ...
```

## **Children**

## **devices** Device

observe read-only

**mixer_device** ChainMixerDevice

read-only

## **Properties**

## **color** int

observe

The RGB value of the chain's color in the form `0x00rrggbb` or (2^16 * red) + (2^8) * green + blue, where red, green and blue are values from 0 (dark) to 255 (light).

When setting the RGB value, the nearest color from the color chooser is taken.

## **color_index** long

observe

The color index of the chain.

## **is_auto_colored** bool

observe

1 = the chain will always have the color of the containing track or chain.

read-only **has_audio_input** bool read-only **has_audio_output** bool read-only **has_midi_input** bool read-only **has_midi_output** bool observe **mute** bool 1 = muted (Chain Activator off) observe read-only **muted_via_solo** bool 1 = muted due to another chain being soloed. observe **name** unicode **solo** bool observe

1 = soloed (Solo switch on) does not automatically turn Solo off in other chains.

## **Functions**

## **delete_device**

Parameter: `index` [int] Delete the device at the given index.

## **insert_device**

Parameters: `device_name` [symbol] `target_index` [int] (optional)

Attempts to insert the device specified by `device_name` at the given index in the chain. If no index is provided, attempts to insert the device at the end. Throws an error if insertion is not possible. `device_name` is the name as it appears in the UI of Live.

Not all indices are valid. As can be expected, indices outside of the range defined by the current length of the device chain are invalid, but there are other limitations: for example, a MIDI effect can't be inserted after an instrument. The rule of thumb is that if an index would be invalid when inserting using the mouse, it's invalid here.

At the moment, only native Live devices can be inserted. Max for Live devices and plug-in are not supported.

_Available since Live 12.3._

## **ChainMixerDevice**

This class represents a chain's mixer device in Live.

## **Canonical Paths**

```
live_set tracks N devices M chains L mixer_device
```

```
live_set tracks N devices M return_chains L mixer_device
```

## **Children**

**sends** list of DeviceParameter

observe read-only

[in Audio Effect Racks and Instrument Racks only] For Drum Racks, otherwise empty.

read-only **chain_activator** DeviceParameter read-only **panning** DeviceParameter [in Audio Effect Racks and Instrument Racks only] **volume** DeviceParameter read-only [in Audio Effect Racks and Instrument Racks only]

## **Clip.View**

Representing the view aspects of a Clip.

## **Canonical Path**

```
live_set tracks N clip_slots M clip view
```

## **Properties**

## **grid_is_triplet** bool

Get/set whether the clip is displayed with a triplet grid.

## **grid_quantization** int

Get/set the grid quantization.

## **Functions**

## **hide_envelope**

Hide the Envelopes box.

## **select_envelope_parameter**

Parameter: [DeviceParameter] Select the specified device parameter in the Envelopes box.

## **show_envelope**

Show the Envelopes box.

## **show_loop**

If the clip is visible in Live's Detail View, this function will make the current loop visible there.

## **Clip**

This class represents a clip in Live. It can be either an audio clip or a MIDI clip in the Arrangement or Session View, depending on the track / slot it lives in.

## **Canonical Paths**

```
live_set tracks N clip_slots M clip
```

```
live_set tracks N arrangement_clips M
```

## **Children**

**view** Clip.View

read-only

## **Properties**

## **available_warp_modes** list

read-only

Returns the list of indexes of the Warp Modes available for the clip. Only valid for audio clips.

## **color** int

observe

The RGB value of the clip's color in the form `0x00rrggbb` or (2^16 * red) + (2^8) * green + blue, where red, green and blue are values from 0 (dark) to 255 (light).

When setting the RGB value, the nearest color from the clip color chooser is taken.

## **color_index** int

observe

The clip's color index.

## **end_marker** float

observe

The end marker of the clip in beats, independent of the loop state. Cannot be set before the start marker.

## **end_time** float

observe read-only

The end time of the clip. For Session View clips, if Loop is on, this is the Loop End, otherwise it's the End Marker. For Arrangement View clips, this is always the position of the clip's rightmost edge in the Arrangement.

## **gain** float

observe

The gain of the clip (range is 0.0 to 1.0). Only valid for audio clips.

## **gain_display_string** symbol

read-only

Get the gain display value of the clip as a string (e.g. "1.3 dB"). Can only be called on audio clips.

|**file_path**symbol||read-only|
|---|---|---|
|Get the location of the audio file represented by the clip. Only available for audio clips.|||
|**groove**Groove||observe|
|Get/set/observe access to the groove associated with this clip.|||
|_Available since Live 11.0._|||
|**has_envelopes**bool|observe|read-only|
|Get/observe whether the clip has any automation.|||
|**has_groove**bool||read-only|
|Returns true if a groove is associated with this clip.|||
|_Available since Live 11.0._|||
|**is_session_clip**bool||read-only|
|1 = The clip is a Session clip.|||
|A clip can be either an Arrangement or a Session clip.|||
|**is_arrangement_clip**bool||read-only|
|1 = The clip is an Arrangement clip.|||
|A clip can be either an Arrangement or a Session clip.|||
|**is_take_lane_clip**bool||read-only|

1 = The clip is a Take Lane clip. Returns true if the clip is on a Take Lane. Take Lane clips are also Arrangement clips.

read-only
is_audio_clip  bool
0 = MIDI clip, 1 = audio clip
read-only
is_midi_clip  bool
The opposite of  is_audio_clip  .
observe read-only
is_overdubbing  bool
1 = clip is overdubbing.
is_playing  bool
1 = clip is playing or recording.
observe read-only
is_recording  bool
1 = clip is recording.
read-only
is_triggered  bool
1 = Clip Launch button is blinking.
observe
launch_mode  int

The Launch Mode of the Clip as an integer index. Available Launch Modes are: 0 = Trigger (default)

1 = Gate

2 = Toggle 3 = Repeat

_Available since Live 11.0._

## **launch_quantization** int

observe

The Launch Quantization of the Clip as an integer index. Available Launch Quantization values are: 0 = Global (default)

1 = None 2 = 8 Bars 3 = 4 Bars 4 = 2 Bars 5 = 1 Bar 6 = 1/2 7 = 1/2T 8 = 1/4 9 = 1/4T 10 = 1/8 11 = 1/8T 12 = 1/16 13 = 1/16T 14 = 1/32

_Available since Live 11.0._

## **legato** bool

observe

1 = Legato Mode switch in the Clip's Launch settings is on.

_Available since Live 11.0._

**length** float

read-only

For looped clips: loop length in beats. Otherwise it's the distance in beats from start to end marker. Makes no sense for unwarped audio clips.

## **loop_end** float

observe

For looped clips: loop end. For unlooped clips: clip end.

## **loop_jump** bang

observe

Bangs when the clip play position is crossing the loop start marker (possibly projected into the loop).

## **loop_start** float

observe

For looped clips: loop start. For unlooped clips: clip start.

loop_start and loop_end are in absolute clip beat time if clip is MIDI or warped. The 1.1.1 position has beat time 0. If the clip is unwarped audio, they are given in seconds, 0 is the time of the first sample in the audio material.

## **looping** bool

observe

1 = clip is looped. Unwarped audio cannot be looped.

## **muted** bool

observe

1 = muted (i.e. the Clip Activator button of the clip is off).

## **name** symbol

observe

## **notes** bang

## observe

Observer sends bang when the list of notes changes. Available for MIDI clips only.

## **warp_markers** dict/bang

observe read-only

Observing this property outputs a bang when the Warp Markers change.

Getting this property returns the Warp Markers in a dict as pairs of sample times and beat times: `sample_time` : [float] the position in seconds in the audio sample file.

`beat_time` : [float] the beat this sample position corresponds to.

To calculate the position in the sample file that corresponds to any given Clip time in beats, Live goes through these steps:\

Find the Warp Marker with a `beat_time` below the given Clip time and the one above it.

- Get the ratio of the Clip time in beats between the beat times of these two markers.

Get the `sample_time` for each of these two markers.

Interpolate between these two sample times with the same ratio to get the file sample position in seconds.

The last Warp Marker in the dict is not visible in the Live interface. This hidden marker is used to calculate the BPM of the last segment.

Available for audio clips only.

_Getting is available since Live 11.0._

## **pitch_coarse** int

observe

Pitch shift in semitones ("Transpose"), -48 ... 48. Available for audio clips only.

## **pitch_fine** float

observe

Extra pitch shift in cents ("Detune"), -50 ... 49. Available for audio clips only.

## **playing_position** float

observe read-only

Current playing position of the clip.

For MIDI and warped audio clips, the value is given in beats of absolute clip time. The clip's beat time of 0 is where 1 is shown in the bar/beat/16th time scale at the top of the clip view.

For unwarped audio clips, the position is given in seconds, according to the time scale shown at the bottom of the clip view.

Stopped clips have a playing position of 0.

## **playing_status** bang

observe

Observer sends bang when playing/trigger status changes.

## **position** float

observe read-only

Get and set the clip's loop position. The value will always equal loop_start, however setting this property, unlike setting loop_start, preserves the loop length.

## **ram_mode** bool

observe

1 = an audio clip’s RAM switch is enabled.

## **sample_length** int

read-only

Length of the Clip's sample, in samples.

## **sample_rate** float

read-only

Get the Clip's sample rate.

## **signature_denominator** int

observe

## **signature_numerator** int

observe

## **start_marker** float

observe

The start marker of the clip in beats, independent of the loop state. Cannot be set behind the end marker.

## **start_time** float

observe read-only

The start time of the clip, relative to the global song time. The value is in beats.

For Arrangement View clips, this is the offset within the arrangement. For Session View clips, this is the time the clip was started. Note that what is reported is the start_time of the currently playing clip on the track, regardless of which clip.

When a Session View clip's playback position was offset by clicking in its time ruler in the Clip Detail View or moving its start marker, its start_time may be negative. This allows using the start_time as an offset when calculating the clip's current playback position based on the global song time.

## **velocity_amount** float

observe

How much the velocity of the note that triggers the clip affects its volume, 0 = no effect, 1 = full effect.

_Available since Live 11.0._

## **warp_mode** int

observe

The Warp Mode of the clip as an integer index. Available Warp Modes are:

0 = Beats Mode

1 = Tones Mode

2 = Texture Mode

3 = Re-Pitch Mode

4 = Complex Mode

5 = REX Mode

6 = Complex Pro Mode

Available for audio clips only.

## **warping** bool

observe

1 = Warp switch is on.

Available for audio clips only.

Technical note: Internally, Live will defer the setting of this property. This has the consequence that if you are sequencing API calls from a single event, the actual order of operations may differ from what you'd intuitively expect. Most of the time this should be transparent to you, but if you run into issues, please report them.

## **will_record_on_start** bool

read-only

1 for MIDI clips which are in triggered state, with the track armed and MIDI Arrangement Overdub on.

## **Functions**

## **add_new_notes**

Parameter:

```
dictionary
```

Key: `"notes"` [list of note specification dictionaries]

Note specification dictionaries have the following keys:

`pitch` : [int] the MIDI note number, 0...127, 60 is C3.

`start_time` : [float] the note start time in beats of absolute clip time.

`duration` : [float] the note length in beats.

`velocity (optional)` : [float] the note velocity, 0 ... 127 _(100 by default)_ .

`mute (optional)` : [bool] 1 = the note is deactivated _(0 by default)_ .

`probability (optional)` : [float] the chance that the note will be played:

1.0 = the note is always played

0.0 = the note is never played

## _(1.0 by default)_ .

`velocity_deviation (optional)` : [float] the range of velocity values at which the note can be played:

0.0 = no deviation; the note will always play at the velocity specified by the _velocity_ property -127.0 to 127.0 = the note will be assigned a velocity value between _velocity_ and _velocity + velocity_deviation_ , inclusive; if the resulting range exceeds the limits of MIDI velocity (0 to 127), then it will be clamped within those limits

## _(0.0 by default)_ .

`release_velocity (optional)` : [float] the note release velocity _(64 by default)_ .

Returns a list of note IDs of the added notes.

For MIDI clips only.

_Available since Live 11.0._

## **add_warp_marker**

Only available for warped Audio Clips. Adds the specified warp marker, if possible.

The warp marker is specified as a dict which can have a `beat_time` and a `sample_time` key, both associated with float values.

The `sample_time` key may be omitted; in this case, Live will calculate the appropriate sample time to create a warp marker at the specified beat time without changing the Clip's playback timing, similar to what would happen if you were to double-click in the upper half of the Sample Display in Clip View.

If `sample_time` is specified, certain limitations must be taken into account: \

- The sample time must lie within the range _[0, s]_ , where _s_ is the sample's length. The

- `sample_length` Clip property helps with this.

The sample time must lie between the left and right adjacents markers' respective sample times (this is a logical constraint).

Within these constraints, there are limitations on the resulting segments' BPM. The allowed BPM range is _[5, 999]_ .

## **apply_note_modifications**

Parameter:

```
dictionary
```

Key: `"notes"` [list of note dictionaries] as returned from `get_notes_extended` .

The list of note dictionaries passed to the function can be a subset of notes in the clip, but will be ignored if it contains any notes that are not present in the clip.

For MIDI clips only.

_Available since Live 11.0. Replaces modifying notes with remove_notes followed by set_notes._

## **clear_all_envelopes**

Removes all automation in the clip.

## **clear_envelope**

Parameter:

## `device_parameter` [id]

Removes the automation of the clip for the given parameter.

## **crop**

Crops the clip: if the clip is looped, the region outside the loop is removed; if it isn't, the region outside the start and end markers.

## **deselect_all_notes**

Call this before replace_selected_notes if you just want to add some notes. Output:

```
deselect_all_notes id 0
```

For MIDI clips only.

## **duplicate_loop**

Makes the loop two times longer by moving loop_end to the right, and duplicates both the notes and the envelopes. If the clip is not looped, the clip start/end range is duplicated. Available for MIDI clips only.

## **duplicate_notes_by_id**

Parameter:

`list` of note IDs.

Or `dictionary`

Keys:

`note_ids` [list of note IDs] as returned from `get_notes_extended`

`destination_time (optional)` [float/int] `transposition_amount (optional)` [int]

Duplicates all notes matching the given note IDs.

Provided note IDs must be associated with existing notes in the clip. Existing notes can be queried with `get_notes_extended` .

The selection of notes will be duplicated to _destination_time_ , if provided. Otherwise the new notes will be inserted after the last selected note. This behavior can be observed when duplicating notes in the Live GUI.

If the _transposition_amount_ is specified, the duplicated notes will be transposed by the number of semitones.

Available for MIDI clips only.

_Available since Live 11.1.2_

## **duplicate_region**

Parameter:

`region_start` [float/int] `region_length` [float/int] `destination_time` [float/int] `pitch (optional)` [int] `transposition_amount (optional)` [int]

Duplicate the notes in the specified region to the _destination_time_ . Only notes of the specified pitch are duplicated or all if _pitch_ is -1. If the _transposition_amount_ is not 0, the notes in the region will be transposed by the _transpose_amount_ of semitones. Available for MIDI clips only.

## **fire**

Same effect as pressing the Clip Launch button.

## **get_all_notes_extended**

Parameter:

`dict (optional)` [dict]

(See below for a discussion of this argument).

Returns a dictionary of all of the notes in the clip, regardless of where they are positioned with respect to the start/end markers and the loop start/loop end, as a list of note dictionaries. Each note dictionary consists of the following key-value pairs:

`note_id` : [int] the unique note identifier. `pitch` : [int] the MIDI note number, 0...127, 60 is C3.

`start_time` : [float] the note start time in beats of absolute clip time. `duration` : [float] the note length in beats. `velocity` : [float] the note velocity, 0 ... 127. `mute` : [bool] 1 = the note is deactivated. `probability` : [float] the chance that the note will be played:

1.0 = the note is always played;

- 0.0 = the note is never played.

`velocity_deviation` : [float] the range of velocity values at which the note can be played: 0.0 = no deviation; the note will always play at the velocity specified by the _velocity_ property -127.0 to 127.0 = the note will be assigned a velocity value between _velocity_ and _velocity + velocity_deviation_ , inclusive; if the resulting range exceeds the limits of MIDI velocity (0 to 127), then it will be clamped within those limits.

`release_velocity` : [float] the note release velocity.

It is possible to optionally provide a single [dict] argument to this function, containing a single keyvalue pair: the key is "return" and the associated value is a list of the note properties as listed above in the discussion of the returned note dictionaries, e.g. ["note_id", "pitch", "velocity"]. The effect of this will be that the returned note dictionaries will only contain the key-value pairs for the specified properties, which can be useful to improve patch performance when processing large notes dictionaries.

For MIDI clips only.

_Available since Live 11.1_

## **get_notes_by_id**

Parameter: `list` of note IDs.

Provided note IDs must be associated with existing notes in the clip. Existing notes can be queried with `get_notes_extended` .

Returns a dictionary of notes associated with the provided IDs, as a list of note dictionaries. Each note dictionary consists of the following key-value pairs:

`note_id` : [int] the unique note identifier. `pitch` : [int] the MIDI note number, 0...127, 60 is C3. `start_time` : [float] the note start time in beats of absolute clip time. `duration` : [float] the note length in beats. `velocity` : [float] the note velocity, 0 ... 127. `mute` : [bool] 1 = the note is deactivated. `probability` : [float] the chance that the note will be played: 1.0 = the note is always played; 0.0 = the note is never played. `velocity_deviation` : [float] the range of velocity values at which the note can be played:

0.0 = no deviation; the note will always play at the velocity specified by the _velocity_ property -127.0 to 127.0 = the note will be assigned a velocity value between _velocity_ and _velocity + velocity_deviation_ , inclusive; if the resulting range exceeds the limits of MIDI velocity (0 to 127), then it will be clamped within those limits.

`release_velocity` : [float] the note release velocity.

It is possible to optionally provide the argument to this function in the form of a dictionary instead. The dictionary must include the "note_ids" key associated with a list of [int]s, which are the ID values you would like to pass to the function.

If you use this method, you can optionally provide an additional key-value pair: the key is "return" and the associated value is a list of the note properties as listed above in the discussion of the returned note dictionaries, e.g. ["note_id", "pitch", "velocity"]. The effect of this will be that the returned note dictionaries will only contain the key-value pairs for the specified properties, which can be useful to improve patch performance when processing large notes dictionaries.

For MIDI clips only.

_Available since Live 11.0._

## **get_notes_extended**

Parameters:

`from_pitch` [int] `pitch_span` [int] `from_time` [float] `time_span` [float]

`from_time` and `time_span` are given in beats.

Returns a dictionary of notes that have their start times in the given area, as a list of note dictionaries. Each note dictionary consists of the following key-value pairs: `note_id` : [int] the unique note identifier. `pitch` : [int] the MIDI note number, 0...127, 60 is C3. `start_time` : [float] the note start time in beats of absolute clip time. `duration` : [float] the note length in beats. `velocity` : [float] the note velocity, 0 ... 127. `mute` : [bool] 1 = the note is deactivated.

`probability` : [float] the chance that the note will be played:

1.0 = the note is always played;

0.0 = the note is never played.

`velocity_deviation` : [float] the range of velocity values at which the note can be played: 0.0 = no deviation; the note will always play at the velocity specified by the _velocity_ property -127.0 to 127.0 = the note will be assigned a velocity value between _velocity_ and _velocity + velocity_deviation_ , inclusive; if the resulting range exceeds the limits of MIDI velocity (0 to 127), then it will be clamped within those limits.

`release_velocity` : [float] the note release velocity.

It is possible to optionally provide the arguments to this function in the form of a single dictionary instead. The dictionary must include all of the parameter names given above as its keys; the associated values are the parameter values you wish to pass to the function.

If you use this method, you can optionally provide an additional key-value pair: the key is "return" and the associated value is a list of the note properties as listed above in the discussion of the returned note dictionaries, e.g. ["note_id", "pitch", "velocity"]. The effect of this will be that the returned note dictionaries will only contain the key-value pairs for the specified properties, which can be useful to improve patch performance when processing large notes dictionaries.

For MIDI clips only.

_Available since Live 11.0. Replaces get_notes._

## **get_selected_notes_extended**

Parameter:

`dict (optional)` [dict]

(See below for a discussion of this argument).

Returns a dictionary of the selected notes in the clip, as a list of note dictionaries. Each note dictionary consists of the following key-value pairs: `note_id` : [int] the unique note identifier. `pitch` : [int] the MIDI note number, 0...127, 60 is C3. `start_time` : [float] the note start time in beats of absolute clip time. `duration` : [float] the note length in beats. `velocity` : [float] the note velocity, 0 ... 127. `mute` : [bool] 1 = the note is deactivated.

`probability` : [float] the chance that the note will be played:

1.0 = the note is always played;

## 0.0 = the note is never played.

`velocity_deviation` : [float] the range of velocity values at which the note can be played: 0.0 = no deviation; the note will always play at the velocity specified by the _velocity_ property -127.0 to 127.0 = the note will be assigned a velocity value between _velocity_ and _velocity + velocity_deviation_ , inclusive; if the resulting range exceeds the limits of MIDI velocity (0 to 127), then it will be clamped within those limits.

`release_velocity` : [float] the note release velocity.

It is possible to optionally provide a single [dict] argument to this function, containing a single keyvalue pair: the key is "return" and the associated value is a list of the note properties as listed above in the discussion of the returned note dictionaries, e.g. ["note_id", "pitch", "velocity"]. The effect of this will be that the returned note dictionaries will only contain the key-value pairs for the specified properties, which can be useful to improve patch performance when processing large notes dictionaries.

For MIDI clips only.

_Available since Live 11.0. Replaces get_selected_notes._

## **move_playing_pos**

## Parameter: `beats`

`beats` [float] relative jump distance in beats. Negative beats jump backwards.

Jumps by given amount, unquantized.

Unwarped audio clips, recording audio clips and recording non-overdub MIDI clips cannot jump.

## **move_warp_marker**

## Parameters: `beat_time` [float]

## `beat_time_distance` [float]

Moves the warp marker specified by _beat_time_ the specified beat time distance.

## **quantize**

Parameter:

`quantization_grid` [int] `amount` [float]

Quantizes all notes in the clip to the quantization_grid taking the song's swing_amount into account.

## **quantize_pitch**

Parameter:

`pitch` [int] `quantization_grid` [int] `amount` [float]

Same as _quantize_ , but only for notes in the given pitch.

## **remove_notes_by_id**

Parameter:

`list` of note IDs.

Deletes all notes associated with the provided IDs.

Provided note IDs must be associated with existing notes in the clip. Existing notes can be queried with `get_notes_extended` .

_Available since Live 11.0._

## **remove_notes_extended**

Parameter:

`from_pitch` [int] `pitch_span` [int] `from_time` [float] `time_span` [float]

Deletes all notes that start in the given area. `from_time` and `time_span` are given in beats.

_Available since Live 11.0. Replaces remove_notes._

## **remove_warp_marker**

Parameter: `beat_time` [float]

Removes the warp marker at the given beat time.

## **scrub**

Parameter: `beat_time` [float]

Scrub the clip to a time, specified in beats. This behaves exactly like scrubbing with the mouse; the scrub will respect Global Quantization, starting and looping in time with the transport. The scrub will continue until stop_scrub() is called.

## **select_all_notes**

Use this function to process all notes of a clip, independent of the current selection.

Output:

```
select_all_notes id 0
```

For MIDI clips only.

## **select_notes_by_id**

Parameter:

`list` of note IDs.

Selects all notes associated with the provided IDs.

Note that this function will _not_ print a warning or error if the list contains nonexistent IDs.

_Available since Live 11.0.6_

## **set_fire_button_state**

Parameter: `state` [bool]

If the state is set to 1, Live simulates pressing the clip start button until the state is set to 0, or until the clip is otherwise stopped.

## **stop**

Same effect as pressing the stop button of the track, but only if this clip is actually playing or recording. If this clip is triggered or if another clip in this track is playing, it has no effect.

## **stop_scrub**

Stops an active scrub on a clip.

## **ControlSurface**

A ControlSurface can be reached either directly by the root path `control_surfaces N` or by getting a list of active control surface IDs, via calling _get control_surfaces_ on an Application object. The latter list is in the same order in which control surfaces appear in Live's Link/MIDI Preferences. Note the same order is not guaranteed when getting a control surface via the `control_surfaces N` path.

A control surface can be thought of as a software layer between the Live API and, in this case, Max for Live. Individiual controls on the surface are represented by objects that can be grabbed and released via Max for Live, to obtain and give back exclusive control (see _grab_control_ and _release_control_ ). In this way, parts of the hardware can be controlled via Max for Live while other parts can retain their default functionality.

Additionally, Live offers a special `MaxForLive` control surface that has a _register_midi_control_ function. Using this, Max for Live developers can set up entirely custom control surfaces by adding and grabbing arbitrary controls.

## **Canonical Path**

```
control_surfaces N
```

## **Properties**

## **pad_layout** symbol

observe read-only

The active pad layout.

On Push 2 and 3, the layout can be changed with the Note and Session buttons and depends on the loaded instrument. Layout variants can be selected by pressing the Layout button.

Available layouts are:\

- Melodic mode - the device chain is empty or an Instrument is loaded

- `note.melodic.64_notes` - Melodic: 64 Notes

- `note.melodic.64_notes_and_macro_variations` - Melodic: 64 Notes + Macro Variations `note.melodic.sequencer` - Melodic: Sequencer

- `note.melodic.sequencer_and_32_notes` - Melodic: Sequencer + 32 Notes

- Drums mode - a Drum Rack is loaded

- `note.drums.macro_variations` - Drums: Macro Variations

- `note.drums.64_pads` - Drums: 64 Pads

- `note.drums.loop_selector` - Drums: Loop Selector

- `note.drums.16_velocities` - Drums: 16 Velocities

- `note.drums.16_pitches` - Drums: 16 Pitches

- Session mode - the Session button was pressed `session` - Session is active

## **Functions**

## **get_control**

Parameter: `name`

Returns the control with the given name.

## **get_control_names**

Returns the list of all control names.

## **grab_control**

Parameter: `control`

Take ownership of the _control_ . This releases all standard functionality of the control, so that it can be used exclusively via Max for Live.

## **grab_midi**

Forward MIDI messages received by the control surface script from the control surface to Max for Live.

Note: the control surface script will only receive those channel messages from Live's engine that it explicitly requests. For example, a script might use a specific note message to toggle transport in Live; it will thus request that this note message be forwarded to it from Live.

Messages used for purely real-time purposes, on the other hand, will often bypass the script and instead just be sent to Live's tracks; this is true, for example, of Push's pads in Note (but not Session) mode. Accordingly, the API object will not output these real-time pad messages; to work with track messages, use objects such as `midiin` .

## **register_midi_control**

Parameters:

`name` [symbol] `status` [int] `number` [int]

( _MaxForLive_ control surface only) Register a MIDI control defined by _status_ and _number_ . Supported

status codes are _144_ (note on), _176_ (continuous control) and _224_ (pitchbend). Returns the LOM ID associated with the control.

Once a control is registered and grabbed via _grab_control_ , Live will forward associated MIDI messages that it receives to Max for Live. Max for Live can send values to the control (e.g. to light an LED) by calling _send_value_ on the control object.

## **release_control**

Parameter: `control`

Re-establishes the standard functionality for the control.

## **release_midi**

Stop forwarding MIDI messages received from the control surface to Max for Live.

## **send_midi**

Parameter: `midi_message` [list of int] Send _midi_message_ to the control surface.

## **send_receive_sysex**

Parameters:

`sysex_message` [list of int] `timeout` [symbol, int]

Send _sysex_message_ to the control surface and await a response.

If the message is followed by the word _timeout_ and a float, this sets the response timeout accordingly. The default timeout value is 0.2.

If the response times out and MIDI has not been grabbed via _grab_midi_ , it's not forwarded to Max for Live. If MIDI has been grabbed via Max for Live, received messages are always forwarded, but the timeout is still reported.

## **CuePoint**

Represents a locator in the Arrangement View.

## **Canonical Path**

```
live_set cue_points N
```

## **Properties**

**name** symbol

observe

**time** float

observe read-only

Arrangement position of the marker in beats.

## **Functions**

## **jump**

Set current Arrangement playback position to marker, quantized if song is playing.

## **Device.View**

Representing the view aspects of a Device.

## **Canonical Paths**

```
live_set tracks N devices M view
```

```
live_set tracks N devices M chains L devices K view
```

```
live_set tracks N devices M return_chains L devices K view
```

## **Properties**

**is_collapsed** bool

observe

- 1 = the device is shown collapsed in the device chain.

## **Device**

This class represents a MIDI or audio device in Live.

## **Canonical Paths**

```
live_set tracks N devices M
```

```
live_set tracks N devices M chains L devices K
```

```
live_set tracks N devices M return_chains L devices K
```

## **Children**

**parameters** list of DeviceParameter

observe read-only

Only automatable parameters are accessible. See DeviceParameter to learn how to modify them.

**view** Device.View

read-only

## **Properties**

**can_have_chains** bool

read-only

0 for a single device 1 for a device Rack

## **can_have_drum_pads** bool

read-only

1 for Drum Racks

## **class_display_name** symbol

read-only

Get the original name of the device (e.g. `Operator` , `Auto Filter` ).

## **class_name** symbol

read-only

Live device type such as `MidiChord` , `Operator` , `Limiter` , `MxDeviceAudioEffect` , or `PluginDevice` .

## **is_active** bool

observe read-only

0 = either the device itself or its enclosing Rack device is off.

## **name** symbol

observe

This is the string shown in the title bar of the device.

## **type** int

read-only

The type of the device. Possible types are: 0 = undefined, 1 = instrument, 2 = audio_effect, 4 = midi_effect.

## **latency_in_samples** int

Device latency in samples.

## **latency_in_ms** float

Device latency in milliseconds.

## **can_compare_ab** bool

observe read-only observe read-only read-only

1 for devices that support the AB Compare feature. 0 otherwise.

_Available since Live 12.3._

## **is_using_compare_preset_b** bool

observe

1 if the device has compare preset B loaded. 0 otherwise. (Only relevant if _can_compare_ab_ , otherwise errors.)

_Available since Live 12.3._

## **Functions**

## **store_chosen_bank**

Parameters:

`script_index` [int] `bank_index` [int]

(This is related to hardware control surfaces and is usually not relevant.)

## **save_preset_to_compare_ab_slot**

Save the device state to the other compare AB slot. (Only relevant if _can_compare_ab_ , otherwise errors.)

_Available since Live 12.3._

## **DeviceIO**

This class represents an input or output bus of a Live device.

## **Properties**

## **available_routing_channels** dictionary

observe read-only

The available channels for this input/output bus. The channels are represented as a _dictionary_ with the following key:

`available_routing_channels` [list]

The list contains _dictionaries_ as described in _routing_channel_ .

## **available_routing_types** dictionary

observe read-only

The available types for this input/output bus. The types are represented as a _dictionary_ with the following key:

`available_routing_types` [list]

The list contains _dictionaries_ as described in _routing_type_ .

## **default_external_routing_channel_is_none** bool

- 1 = the default routing channel for External routing types is none.

_Available since Live 11.0._

## **routing_channel** dictionary

observe

The current routing channel for this input/output bus. It is represented as a _dictionary_ with the following keys:

`display_name` [symbol] `identifier` [symbol]

Can be set to any of the values found in _available_routing_channels._

## **routing_type** dictionary

observe

The current routing type for this input/output bus. It is represented as a _dictionary_ with the following keys:

`display_name` [symbol] `identifier` [symbol]

Can be set to any of the values found in _available_routing_types._

## **DeviceParameter**

This class represents an (automatable) parameter within a MIDI or audio device. To modify a device parameter, set its `value` property or send its object ID to live.remote~.

## **Canonical Path**

```
live_set tracks N devices M parameters L
```

## **Properties**

## **automation_state** int

observe read-only

Get the automation state of the parameter.

0 = no automation.

- 1 = automation active.

- 2 = automation overridden.

## **default_value** float

read-only

Get the default value for this parameter.

Only available for parameters that aren't quantized (see _is_quantized_ ).

## **is_enabled** bool

read-only

1 = the parameter value can be modified directly by the user, by sending `set` to a live.object, by automation or by an assigned MIDI message or keystroke.

Parameters can be disabled because they are macro-controlled, or they are controlled by a liveremote~ object, or because Live thinks that they should not be moved.

## **is_quantized** bool

read-only

1 for booleans and enums

0 for int/float parameters

Although parameters like MidiPitch.Pitch appear quantized to the user, they actually have an is_quantized value of 0.

## **max** float

read-only

Largest allowed value.

**min** float read-only Lowest allowed value. read-only **name** symbol The short parameter name as shown in the (closed) automation chooser. **original_name** symbol read-only The name of a Macro parameter before its assignment.

observe read-only **state** int

The active state of the parameter. 0 = the parameter is active and can be changed. 1 = the parameter can be changed but isn't active, so changes won't have an audible effect. 2 = the parameter cannot be changed.

## **value** float

observe

The internal value between min and max. Use display_value for the value as visible in the GUI.

observe **display_value** float The value as visible in the GUI. **value_items** StringVector read-only

read-only

Get a list of the possible values for this parameter. Only available for parameters that are quantized (see _is_quantized_ ).

## **Functions**

## **re_enable_automation**

Re-enable automation for this parameter.

## **str_for_value**

Parameter: `value` [float] Returns: [symbol] String representation of the specified value.

## **__str__**

Returns: [symbol] String representation of the current parameter value.

## **Groove**

This class represents a groove in Live.

## _Available since Live 11.0._

All grooves are stored in Live's groove pool.

## **Canonical Paths**

```
live_set groove_pool grooves N
```

```
live_set tracks N clip_slots M clip groove
```

## **Children**

## **base** int

Get/set the groove's base grid (index based setter). 0 = 1/4 1 = 1/8 2 = 1/8T 3 = 1/16 4 = 1/16T 5 = 1/32 observe **name** symbol Get/set/observe the name of the groove. observe **quantization_amount** float Get/set/observe the groove's quantization amount. observe **random_amount** float Get/set/observe the groove's random amount. observe **timing_amount** float Get/set/observe the groove's timing amount. observe **velocity_amount** float Get/set/observe the groove's velocity amount.

## **GroovePool**

This class represents the groove pool in Live. It provides access to the current set's list of grooves.

## **Canonical Path**

```
live_set groove_pool
```

## **Children**

**grooves** list of Groove

observe read-only

List of grooves in the groove pool from top to bottom, can be accessed via index.

## **MaxDevice**

This class represents a Max for Live device in Live.

A MaxDevice is a type of Device, meaning that it has all the children, properties and functions that a Device has. Listed below are the members unique to MaxDevice.

## **Properties**

## **audio_inputs** list of DeviceIO

observe read-only

List of the audio inputs that the MaxDevice offers.

## **audio_outputs** list of DeviceIO

observe read-only

List of the audio outputs that the MaxDevice offers.

**midi_inputs** list of DeviceIO

observe read-only

List of the midi inputs that the MaxDevice offers.

_Available since Live 11.0._

## **midi_outputs** list of DeviceIO

observe read-only

List of the midi outputs that the MaxDevice offers.

_Available since Live 11.0._

## **Functions**

## **get_bank_count**

Returns: [int] the number of parameter banks.

## **get_bank_name**

Parameters: `bank_index` [int]

Returns: [list of symbols] The name of the parameter bank specified by bank_index.

## **get_bank_parameters**

Parameters: `bank_index` [int]

Returns: [list of ints] The indices of the parameters contained in the bank specified by bank_index. Empty slots are marked as -1. Bank index -1 refers to the "Best of" bank.

## **MixerDevice**

This class represents a mixer device in Live. It provides access to volume, panning and other DeviceParameter objects. See DeviceParameter to learn how to modify them.

## **Canonical Path**

```
live_set tracks N mixer_device
```

## **Children**

**sends** list of DeviceParameter

observe read-only

observe **crossfade_assign** int 0 = A, 1 = none, 2 = B [not in master track] observe **panning_mode** int

Access to the Track mixer's pan mode: 0 = Stereo, 1 = Split Stereo.

## **RackDevice.View**

Represents the view aspects of a Rack Device. A RackDevice.View is a type of Device.View, meaning that it has all the properties that a Device.View has. Listed below are the members unique to RackDevice.View.

## **Children**

observe **selected_drum_pad** DrumPad Currently selected Drum Rack pad. Only available for Drum Racks. observe **selected_chain** Chain Currently selected chain.

## **Properties**

**drum_pads_scroll_position** int

observe

Lowest row of pads visible, range: 0 - 28. Only available for Drum Racks.

## **is_showing_chain_devices** bool

observe

1 = the devices in the currently selected chain are visible.

## **RackDevice**

This class represents a Live Rack Device.

A RackDevice is a type of Device, meaning that it has all the children, properties and functions that a Device has. Listed below are members unique to RackDevice.

## **Children**

**chain_selector** DeviceParameter

read-only

Convenience accessor for the Rack's chain selector.

**chains** list of Chain

observe read-only

The Rack's chains.

**drum_pads** list of DrumPad

observe read-only

All 128 Drum Pads for the topmost Drum Rack. Inner Drum Racks return a list of 0 entries.

**return_chains** list of Chain

observe read-only

The Rack's return chains.

**visible_drum_pads** list of DrumPad

observe read-only

All 16 visible DrumPads for the topmost Drum Rack. Inner Drum Racks return a list of 0 entries.

## **Properties**

## **can_show_chains** bool

read-only

1 = The Rack contains an instrument device that is capable of showing its chains in Session View.

**has_drum_pads** bool

observe read-only

1 = the device is a Drum Rack with pads. A nested Drum Rack is a Drum Rack without pads. Only available for Drum Racks.

## **has_macro_mappings** bool

observe read-only

1 = any of a Rack's Macros are mapped to a parameter.

## **is_showing_chains** bool

observe

1 = The Rack contains an instrument device that is showing its chains in Session View.

## **variation_count** int

observe read-only

The number of currently stored macro variations.

_Available since Live 11.0._

## **selected_variation_index** int

Get/set the currently selected variation.

_Available since Live 11.0._

## **visible_macro_count** int

observe read-only

The number of currently visible macros.

## **Functions**

**copy_pad**

Parameters:

`source_index` [int] `destination_index` [int]

Copies all content of a Drum Rack pad from a source pad to a destination pad. The source_index and destination_index refer to pad indices inside a Drum Rack.

## **add_macro**

Increases the number of visible macro controls.

_Available since Live 11.0._

## **insert_chain**

Parameters: `index` [int] (optional)

Attempts to insert a new chain at the given index, or at the end of the chain list if no index is provided. Throws an error if insertion is not possible.

Side note: A chain inserted into a Drum Rack will have an initial MIDI In Note setting of "All Notes" (see `DrumChain.in_note` ). You likely want the chain to be triggered when a specific pad is played; the way to achieve this is to set the `in_note` to the note value that corresponds to the pad.

_Available since Live 12.3._

## **remove_macro**

Decreases the number of visible macro controls.

_Available since Live 11.0._

## **randomize_macros**

Randomizes the values of eligible macro controls.

_Available since Live 11.0._

## **store_variation**

Stores a new variation of the values of all currently mapped macros.

_Available since Live 11.0._

## **recall_selected_variation**

Recalls the currently selected macro variation.

_Available since Live 11.0._

## **recall_last_used_variation**

Recalls the macro variation that was recalled most recently.

_Available since Live 11.0._

## **delete_selected_variation**

Deletes the currently selected macro variation. Does nothing if there is no selected variation.

_Available since Live 11.0._

## **Song.View**

This class represents the view aspects of a Live document: the Session and Arrangement Views.

## **Canonical Path**

```
live_set view
```

## **Children**

## **detail_clip** Clip

observe

The clip currently displayed in the Live application's Detail View.

## **highlighted_clip_slot** ClipSlot

The slot highlighted in the Session View.

## **selected_chain** Chain

observe

The highlighted chain, or "id 0"

## **selected_parameter** DeviceParameter

observe read-only

The selected parameter, or "id 0"

**selected_scene** Scene

observe

**selected_track** Track

observe

## **Properties**

## **draw_mode** bool

observe

Reflects the state of the envelope/automation Draw Mode Switch in the transport bar, as toggled with Cmd/Ctrl-B.

0 = breakpoint editing (shows arrow), 1 = drawing (shows pencil)

## **follow_song** bool

observe

Reflects the state of the Follow switch in the transport bar as toggled with Cmd/Ctrl-F. 0 = don't follow playback position, 1 = follow playback position

## **Functions**

## **select_device**

Parameter: `id NN`

Selects the given device object in its track.

You may obtain the id using a live.path or by using `get devices` on a track, for example. The track containing the device will not be shown automatically, and the device gets the appointed device (blue hand) only if its track is selected.

## **Song**

This class represents a Live Set. The current Live Set is reachable by the root path `live_set` .

## **Canonical Path**

```
live_set
```

## **Children**

**cue_points** list of CuePoint

observe read-only

Cue points are the markers in the Arrangement to which you can jump.

observe read-only **return_tracks** list of Track observe read-only **scenes** list of Scene **tracks** list of Track observe read-only observe read-only

**visible_tracks** list of Track

A track is visible if it's not part of a folded group. If a track is scrolled out of view it's still considered visible.

read-only **master_track** Track

**view** Song.View

read-only

read-only **groove_pool** GroovePool

Live's groove pool.

_Available since Live 11.0._

**tuning_system** TuningSystem

observe read-only

Live's currently active tuning system.

## **Properties**

**appointed_device** Device

observe read-only

The appointed device is the one used by a control surface unless the control surface itself chooses which device to use. It is marked by a blue hand.

## **arrangement_overdub** bool

observe

Get/set the state of the MIDI Arrangement Overdub button.

## **back_to_arranger** bool

observe

Get/set/observe the current state of the Back to Arrangement button located in Live's transport bar (1 = highlighted). This button is used to indicate that the current state of the playback differs from what is stored in the Arrangement.

Setting this property to 0 will make Live go back to playing the content of the arrangement.

## **can_capture_midi** bool

observe read-only

1 = Recently played MIDI material exists that can be captured into a Live Track. See _capture_midi_ .

## **can_jump_to_next_cue** bool

observe read-only

0 = there is no cue point to the right of the current one, or none at all.

## **can_jump_to_prev_cue** bool

observe read-only

0 = there is no cue point to the left of the current one, or none at all.

## **can_redo** bool

read-only

1 = there is something in the history to redo.

## **can_undo** bool

read-only

1 = there is something in the history to undo.

## **clip_trigger_quantization** int

observe

Reflects the quantization setting in the transport bar. 0 = None 1 = 8 Bars 2 = 4 Bars 3 = 2 Bars 4 = 1 Bar 5 = 1/2 6 = 1/2T 7 = 1/4 8 = 1/4T 9 = 1/8 10 = 1/8T 11 = 1/16 12 = 1/16T 13 = 1/32

## **count_in_duration** int

observe read-only

The duration of the Metronome's Count-In setting as an index, mapped as follows: 0 = None 1 = 1 Bar 2 = 2 Bars 3 = 4 Bars

## **current_song_time** float

observe

The playing position in the Live Set, in beats.

read-only **exclusive_arm** bool Current status of the exclusive Arm option set in the Live preferences. read-only **exclusive_solo** bool Current status of the exclusive Solo option set in the Live preferences. **file_path** symbol read-only

The path to the current Live Set, in OS-native format. If the Live Set hasn't been saved, the path is empty.

**groove_amount** float The groove amount from the current set's groove pool (0. - 1.0).

observe

## **is_ableton_link_enabled** bool

observe

Enable/disable Ableton Link. The Link toggle in the Live's transport bar must be visible to enable Link.

## **is_ableton_link_start_stop_sync_enabled** bool

observe

Enable/disable Ableton Link Start Stop Sync.

## **is_counting_in** bool

observe read-only

1 = the Metronome is currently counting in.

observe

**is_playing** bool Get/set if Live's transport is running. read-only **last_event_time** float

The beat time of the last event (i.e. automation breakpoint, clip end, cue point, loop end) in the Arrangement. observe **loop** bool Get/set the enabled state of the Arrangement loop. observe **loop_length** float Arrangement loop length in beats. observe **loop_start** float Arrangement loop start in beats. observe **metronome** bool Get/set the enabled state of the metronome. observe **midi_recording_quantization** int Get/set the current Record Quantization value. 0 = None 1 = 1/4

2 = 1/8 3 = 1/8T 4 = 1/8 + 1/8T 5 = 1/16 6 = 1/16T 7 = 1/16 + 1/16T 8 = 1/32 read-only **name** symbol The name of the current Live Set. If the Live Set hasn't been saved, the name is empty. observe **nudge_down** bool 1 = the Tempo Nudge Down button in the transport bar is currently pressed. observe **nudge_up** bool 1 = the Tempo Nudge Up button in the transport bar is currently pressed. observe **tempo_follower_enabled** bool 1 = the Tempo Follower controls the tempo. The Tempo Follower Toggle must be made visible in the preferences for this property to be effective. observe **overdub** bool 1 = MIDI Arrangement Overdub is enabled in the transport. observe **punch_in** bool 1 = the Punch-In button is enabled in the transport.

## **punch_out** bool

observe

1 = the Punch-Out button is enabled in the transport.

## **re_enable_automation_enabled** bool

observe read-only

1 = the Re-Enable Automation button is on.

## **record_mode** bool

observe

1 = the Arrangement Record button is on.

## **root_note** int

observe

The root note of the scale currently selected in Live. The root note can be a number between 0 and 11, where 0 = C and 11 = B.

## **scale_intervals** list

observe read-only

A list of integers representing the intervals in Live's current scale (see _scale_name_ and _scale_mode_ ). An interval is expressed as the difference between the scale degree at the list index and the first scale degree.

## **scale_mode** bool

observe

Access to the Scale Mode setting in Live.

When on, key tracks that belong to the currently selected scale are highlighted in Live's MIDI Note Editor, and pitch-based parameters in MIDI Tools and Devices can be edited in scale degrees rather than semitones.

See also _root_note_ , _scale_name_ , and _scale_intervals_ .

observe
scale_name  unicode
The name of the scale selected in Live, as displayed in the Current Scale Name chooser.
read-only
select_on_launch  bool
1 = the "Select on Launch" option is set in Live's preferences.
observe
session_automation_record  bool
The state of the Automation Arm button.
observe
session_record  bool
The state of the Session Overdub button.
observe read-only
session_record_status  int
Reflects the state of the Session Record button.
observe
signature_denominator  int
observe
signature_numerator  int
observe read-only
song_length  float
A little more than  last_event_time  , in beats.
observe
start_time  float

The position in the Live Set where playing will start, in beats.

## **swing_amount** float

observe

Range: 0.0 - 1.0; affects MIDI Recording Quantization and all direct calls to `Clip.quantize` .

## **tempo** float

observe

Current tempo of the Live Set in BPM, 20.0 ... 999.0. The tempo may be automated, so it can change depending on the current song time.

## **Functions**

## **capture_and_insert_scene**

Capture the currently playing clips and insert them as a new scene below the selected scene.

## **capture_midi**

Parameter: `destination` [int]

0 = auto, 1 = session, 2 = arrangement

Capture recently played MIDI material from audible tracks into a Live Clip. If _destinaton_ is not set or it is set to _auto_ , the Clip is inserted into the view currently visible in the focused Live window. Otherwise, it is inserted into the specified view.

## **continue_playing**

From the current playback position.

## **create_audio_track**

## Parameter: `index`

Index determines where the track is added, it is only valid between 0 and len(song.tracks). Using an index of -1 will add the new track at the end of the list.

## **create_midi_track**

## Parameter: `index`

Index determines where the track is added, it is only valid between 0 and len(song.tracks). Using an index of -1 will add the new track at the end of the list.

## **create_return_track**

Adds a new return track at the end.

## **create_scene**

Parameter: `index`

Returns: The new scene

Index determines where the scene is added. It is only valid between 0 and len(song.scenes). Using an index of -1 will add the new scene at the end of the list.

## **delete_scene**

Parameter: `index`

Delete the scene at the given index.

## **delete_track**

Parameter: `index`

Delete the track at the given index.

## **delete_return_track**

Parameter: `index`

Delete the return track at the given index.

## **duplicate_scene**

Parameter: `index`

Index determines which scene to duplicate.

## **duplicate_track**

Parameter: `index`

Index determines which track to duplicate.

## **find_device_position**

Parameter:

`device` [live object] `target` [live object] `target position` [int] Returns:

[int] The position in the target's chain where the device can be inserted that is the closest possible to the target position.

## **force_link_beat_time**

Force the Link timeline to jump to Live's current beat time.

## **get_beats_loop_length**

Returns: `bars.beats.sixteenths.ticks` [symbol]

The Arrangement loop length.

## **get_beats_loop_start**

Returns: `bars.beats.sixteenths.ticks` [symbol]

The Arrangement loop start.

## **get_current_beats_song_time**

Returns: `bars.beats.sixteenths.ticks` [symbol]

The current Arrangement playback position.

## **get_current_smpte_song_time**

Parameter: `format`

`format` [int] is the time code type to be returned

0 = the frame position shows the milliseconds

1 = Smpte24 2 = Smpte25 3 = Smpte30 4 = Smpte30Drop 5 = Smpte29 Returns: _hours:min:sec_

[symbol]

The current Arrangement playback position.

## **is_cue_point_selected**

Returns: bool 1 = the current Arrangement playback position is at a cue point

## **jump_by**

Parameter: `beats`

`beats` [float] is the amount to jump relatively to the current position

## **jump_to_next_cue**

Jump to the right, if possible.

## **jump_to_prev_cue**

Jump to the left, if possible.

## **move_device**

Parameter: `device` [live object] `target` [live object] `target position` [int]

Returns: [int] The position in the target's chain where the device was inserted. Move the device to the specified position in the target chain. If the device cannot be moved to the specified position, the nearest possible position is chosen.

## **play_selection**

Do nothing if no selection is set in Arrangement, or play the current selection.

## **re_enable_automation**

Trigger 'Re-Enable Automation', re-activating automation in all running Session clips.

## **redo**

Causes the Live application to redo the last operation.

## **scrub_by**

Parameter: `beats`

`beats` [float] the amount to scrub relative to the current Arrangement playback position Same as `jump_by` , at the moment.

## **set_or_delete_cue**

Toggle cue point at current Arrangement playback position.

## **start_playing**

Start playback from the insert marker.

## **stop_all_clips**

Parameter (optional): `quantized`

Calling the function with 0 will stop all clips immediately, independent of the launch quantization. The default is '1'.

## **stop_playing**

Stop the playback.

## **tap_tempo**

Same as pressing the Tap Tempo button in the transport bar. The new tempo is calculated based on the time between subsequent calls of this function.

## **trigger_session_record**

## Parameter: `record_length (optional)`

Starts recording in either the selected slot or the next empty slot, if the track is armed. If _record_length_ is provided, the slot will record for the given length in beats. If triggered while recording, recording will stop and clip playback will start.

## **undo**

Causes the Live application to undo the last operation.

## **TakeLane**

This class represents a take lane in Live. Tracks in Live can have take lanes in Arrangement View, which are used for comping. If take lanes exist for a track, they can be shown by right-clicking on a track and choosing Show Take Lanes.

## **Canonical Path**

```
live_set tracks N take_lanes M
```

## **Children**

## **arrangement_clips** list of Clip

observe read-only

The list of this take lane's Arrangement View clip IDs

## **Properties**

## **name** symbol

observe

The name as shown in the take lane header.

## **Functions**

## **create_audio_clip**

Parameters:

`file_path` [symbol] `start_time` [float]

Given a valid audio file in a supported format, passing its absolute path (on Mac, starting with `/Volumes/(drive name)/` ) creates an audio clip referencing the file in the arrangement view at the specified `start_time` in beats.

Prints an error if the track is not an audio track, if the track is frozen or if the track is being recorded into. `start_time` must be within the range `[0., 1576800]` .

## **create_midi_clip**

Parameters:

`start_time` [float] `length` [float]

Creates an empty MIDI clip with the specified `length` in beats and inserts it into the arrangement at the specified `start_time` in beats.

Prints an error if the track is not a MIDI track, if the track is frozen or when the track is currently being recorded into. `start_time` must be within the range `[0., 1576800]` .

Canonical Path
## **this_device**

This root path represents the device containing the live.path object to which the `goto this_device` message is sent. The class of this object is `Device` .

## **Canonical Path**

```
live_set tracks N devices M
```

## **Track.View**

Representing the view aspects of a track.

## **Canonical Path**

```
live_set tracks N view
```

## **Children**

## **selected_device** Device

observe read-only

The selected device or the first selected device (in case of multi/group selection).

## **Properties**

## **device_insert_mode** int

observe

Determines where a device will be inserted when loaded from the browser. 0 = add device at the end, 1 = add device to the left of the selected device, 2 = add device to the right of the selected device.

## **is_collapsed** bool

observe

In Arrangement View: 1 = track collapsed, 0 = track opened.

## **Functions**

## **select_instrument**

Returns: bool 0 = there are no devices to select

Selects track's instrument or first device, makes it visible and focuses on it.

## **Track**

This class represents a track in Live. It can either be an audio track, a MIDI track, a return track or the master track. The master track and at least one Audio or MIDI track will be always present. Return tracks are optional.

Not all properties are supported by all types of tracks. The properties are marked accordingly.

## **Canonical Path**

```
live_set tracks N
```

## **Children**

## **take_lanes** list of TakeLane

The list of this track's take lanes

**clip_slots** list of ClipSlot

## **arrangement_clips** list of Clip

observe read-only observe read-only observe read-only

The list of this track's Arrangement View clip IDs

_Available since Live 11.0._

**devices** list of Device

observe read-only

Includes mixer device.

## **group_track** Track

read-only

The Group Track, if the Track is grouped. If it is not, _id 0_ is returned.

## **mixer_device** MixerDevice

**view** Track.View

read-only read-only

## **Properties**

## **arm** bool

observe

- 1 = track is armed for recording. [not in return/master tracks]

## **available_input_routing_channels** dictionary

observe read-only

The list of available source channels for the track's input routing. It's represented as a _dictionary_ with the following key:

`available_input_routing_channels` [list]

The list contains _dictionaries_ as described in _input_routing_channel_ .

Only available on MIDI and audio tracks.

## **available_input_routing_types** dictionary

observe read-only

The list of available source types for the track's input routing. It's represented as a _dictionary_ with the following key:

`available_input_routing_types` [list]

The list contains _dictionaries_ as described in _input_routing_type_ .

Only available on MIDI and audio tracks.

## **available_output_routing_channels** dictionary

observe read-only

The list of available target channels for the track's output routing. It's represented as a _dictionary_ with the following key:

`available_output_routing_channels` [list]

The list contains _dictionaries_ as described in _output_routing_channel_ .

Not available on the master track.

## **available_output_routing_types** dictionary

observe read-only

The list of available target types for the track's output routing. It's represented as a _dictionary_ with the following key:

`available_output_routing_types` [list]

The list contains _dictionaries_ as described in _output_routing_type_ .

Not available on the master track.

## **back_to_arranger** bool

observe

Get/set/observe the current state of the Single Track Back to Arrangement button (1 = highlighted). This button is used to indicate that the current state of the playback differs from what is stored in the Arrangement.

Setting this property to 0 will make Live go back to playing the track's arrangement content. For group tracks, this means that all of the tracks that belong to the group and any subgroups will go back to playing the arrangement.

## **can_be_armed** bool

read-only

0 for return and master tracks.

## **can_be_frozen** bool

read-only

1 = the track can be frozen, 0 = otherwise.

## **can_show_chains** bool

read-only

1 = the track contains an Instrument Rack device that can show chains in Session View.

## **color** int

observe

The RGB value of the track's color in the form `0x00rrggbb` or (2^16 * red) + (2^8) * green + blue, where red, green and blue are values from 0 (dark) to 255 (light).

When setting the RGB value, the nearest color from the track color chooser is taken.

## **color_index** long

observe

The color index of the track.

## **fired_slot_index** int

observe read-only

Reflects the blinking clip slot. -1 = no slot fired, -2 = Clip Stop Button fired First clip slot has index 0. [not in return/master tracks]

## **fold_state** int

0 = tracks within the Group Track are visible, 1 = Group Track is folded and the tracks within the Group Track are hidden

[only available if `is_foldable` = 1]

## **has_audio_input** bool

read-only

1 for audio tracks.

## **has_audio_output** bool

read-only

1 for audio tracks and MIDI tracks with instruments.

## **has_midi_input** bool

read-only

1 for MIDI tracks.

## **has_midi_output** bool

read-only

1 for MIDI tracks with no instruments and no audio effects.

## **implicit_arm** bool

observe

A second arm state, only used by Push so far.

## **input_meter_left** float

observe read-only

Smoothed momentary peak value of left channel input meter, 0.0 to 1.0. For tracks with audio output only. This value corresponds to the meters shown in Live. Please take into account that the left/right audio meters put a significant load onto the GUI part of Live.

## **input_meter_level** float

observe read-only

Hold peak value of input meters of audio and MIDI tracks, 0.0 ... 1.0. For audio tracks it is the maximum of the left and right channels. The hold time is 1 second.

## **input_meter_right** float

observe read-only

Smoothed momentary peak value of right channel input meter, 0.0 to 1.0. For tracks with audio output only. This value corresponds to the meters shown in Live.

## **input_routing_channel** dictionary

observe

The currently selected source channel for the track's input routing. It's represented as a _dictionary_ with the following keys:

`display_name` [symbol]

`identifier` [symbol]

Can be set to all values found in the track's _available_input_routing_channels_ . Only available on MIDI and audio tracks.

## **input_routing_type** dictionary

## observe

The currently selected source type for the track's input routing. It's represented as a _dictionary_ with the following keys:

`display_name` [symbol] `identifier` [symbol]

Can be set to all values found in the track's _available_input_routing_types_ .

Only available on MIDI and audio tracks.

## **is_foldable** bool

read-only

1 = track can be (un)folded to hide or reveal the contained tracks. This is currently the case for Group Tracks. Instrument and Drum Racks return 0 although they can be opened/closed. This will be fixed in a later release.

## **is_frozen** bool

observe read-only

1 = the track is currently frozen.

## **is_grouped** bool

read-only

1 = the track is contained within a Group Track.

## **is_part_of_selection** bool

read-only

## **is_showing_chains** bool

observe

Get or set whether a track with an Instrument Rack device is currently showing its chains in Session View.

## **is_visible** bool

read-only

0 = track is hidden in a folded Group Track.

## **mute** bool

observe

[not in master track]

## **muted_via_solo** bool

observe read-only

1 = the track or chain is muted due to Solo being active on at least one other track.

## **name** symbol

observe

As shown in track header.

## **output_meter_left** float

observe read-only

Smoothed momentary peak value of left channel output meter, 0.0 to 1.0. For tracks with audio output only. This value corresponds to the meters shown in Live. Please take into account that the left/right audio meters add a significant load to Live GUI resource usage.

## **output_meter_level** float

observe read-only

Hold peak value of output meters of audio and MIDI tracks, 0.0 to 1.0. For audio tracks, it is the maximum of the left and right channels. The hold time is 1 second.

## **output_meter_right** float

observe read-only

Smoothed momentary peak value of right channel output meter, 0.0 to 1.0. For tracks with audio output only. This value corresponds to the meters shown in Live.

## **performance_impact** float

observe read-only

Reports the performance impact of this track.

## **output_routing_channel** dictionary

observe

The currently selected target channel for the track's output routing. It's represented as a _dictionary_ with the following keys:

`display_name` [symbol]

`identifier` [symbol]

Can be set to all values found in the track's _available_output_routing_channels_ .

Not available on the master track.

## **output_routing_type** dictionary

observe

The currently selected target type for the track's output routing. It's represented as a _dictionary_ with the following keys:

`display_name` [symbol] `identifier` [symbol]

Can be set to all values found in the track's _available_output_routing_types_ .

Not available on the master track.

## **playing_slot_index** int

observe read-only

First slot has index 0, -2 = Clip Stop slot fired in Session View, -1 = Arrangement recording with no Session clip playing. [not in return/master tracks]

## **solo** bool

observe

Remark: when setting this property, the exclusive Solo logic is bypassed, so you have to unsolo the other tracks yourself. [not in master track]

## **Functions**

## **create_audio_clip**

Parameters:

`file_path` [symbol] `position` [float]

Given an absolute path to a valid audio file in a supported format, creates an audio clip that references the file at the specified position in the arrangement view. Prints an error if the track is not an audio track, if the track is frozen, or if the track is being recorded into. The position must be within the range [0., 1576800].

See the `ClipSlot.create_audio_clip` function if you need to create audio clips in session view instead.

## **create_midi_clip**

Parameters:

`start_time` [float] `length` [float]

Creates an empty MIDI clip and inserts it into the arrangement at the specified time. Throws an error when called on a non-MIDI track or a frozen track, when the specified time is outside the [0., 1576800.] range, or when the track is currently being recorded into.

See the `ClipSlot.create_clip` function if you need to create audio clips in session view instead.

## **create_take_lane**

Creates a take lane for this track.

## **delete_clip**

Parameter: `clip` Delete the given clip.

## **delete_device**

Parameter: `index`

Delete the device at the given index.

## **duplicate_clip_slot**

Parameter: `index`

Works like 'Duplicate' in a clip's context menu.

## **duplicate_clip_to_arrangement**

Parameters: `clip``destination_time` [float]

Duplicate the given clip to the Arrangement, placing it at the given _destination_time_ in beats.

## **insert_device**

Parameters: `device_name` [symbol] `target_index` [int] (optional)

Attempts to insert the device specified by `device_name` at the given index in the track's device chain. If no index is provided, attempts to insert the device at the end of the chain. Throws an error if insertion is not possible.

`device_name` is the name as it appears in the UI of Live.

Not all indices are valid. As can be expected, indices outside of the range defined by the current length of the device chain are invalid, but there are other limitations: for example, a MIDI effect can't be inserted after an instrument. The rule of thumb is that if an index would be invalid when inserting using the mouse, it's invalid here.

At the moment, only native Live devices can be inserted. Max for Live devices and plug-in are not supported.

_Available since Live 12.3._

## **jump_in_running_session_clip**

Parameter: `beats`

`beats` [float] is the amount to jump relatively to the current clip position. Modify playback position in running Session clip, if any.

## **stop_all_clips**

Stops all playing and fired clips in this track.
