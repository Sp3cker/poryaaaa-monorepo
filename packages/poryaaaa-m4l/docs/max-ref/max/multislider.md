# multislider

_max · U/I_

> Display data as sliders or a scrolling display

Displays data as either an array of sliders or a scrolling display. When configured as sliders, the values are set (and output) as numeric lists. When configured as a scrolling display, multislider receives numbers, plots them, and scrolls the display area.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | list | Sets Sliders |
| out0 | list | Values of All Sliders |
| out1 | int/float | Value of One Slider |

## Messages

- `bang` — Output the current slider values as a list
  Outputs the current slider values as a list.
- `int(input: int)` — Set all slider values and output a list
  Sets all slider values and positions to the number received and outputs a list reflecting the current values. If the multislider data type is set to float, the values in the incoming list are converted to floats.
- `float(input: float)` — Set all slider values and output a list
  Sets all slider values and positions to the number received and outputs a list reflecting the current values. If the multislider data type is set to int, the values in the incoming list are truncated and converted to ints.
- `list(input: list)` — Set slider values
  Sets each slider to a corresponding value in the list from left to right, with the first value in the list setting the first slider. If the multislider has a different number of sliders than is present in the list, the number of sliders is changed to the number of items in the list. In such a case, the outside dimensions of the multislider will not change, only the width or height of the sliders.
- `anything(list: list)` — Function depends on compatibility mode
  anything is used to offer backwards compatibility in conjunction with the compatibility message.
- `copy` — Copy multislider values
  The copy message copies the values of a multislider, including the settype, setstyle, and setminmax attributes.
- `drawbars(flag: int)` — Display slider values as bars
  The word drawbars followed by a non-zero number will set the multislider to draw bars when displaying slider values. When followed by a 0, the sliders' values will not be drawn in bars.
- `drawlines(flag: int)` — Display slider values as lines
  The word drawlines followed by a non-zero number will set the multislider to draw lines when displaying slider values. When followed by a 0, the sliders' values will not be drawn in lines.
- `echo(flag: int)` — Enable echoing input lists to output
  Toggles echo mode on and off. When echo mode is on, the multislider object will output any list received in its inlet. The default is on (1).
- `fetch(slider: int)` — Fetch and output the value of a slider
  The word fetch, followed by a number, sends the value of the numbered slider out the right (single slider value) outlet. Slider numbering starts at 1. Any number less than 1 will report the first slider's value.
- `interp(mode: int)` — Enable interpolated output values
  Sending the word interp, followed by a one or zero, enables or disables interpolation mode. When interpolation mode is on (the default), the multislider object will output interpolated values when a slider is moved. In most cases you probably will not want to disable interpolation mode.
- `max` — Set all sliders to maximum values
  Sets all sliders to their maximum values.
- `maximum` — Output the largest slider value
  The word maximum causes the value of the slider with the largest value to be sent out the right outlet.
- `min` — Set all sliders to minimum values
  Sets all sliders to their minimum values.
- `minimum` — Output the smallest slider value
  The word minimum causes the value of the slider with the smallest value to be sent out the right outlet.
- `normalize(number: float)` — Rescale slider values relative to a given value
  The word normalize, followed by a float, will scale the sample values in the multislider so that the highest number matches the value given by the argument. Every value is scaled, and this activity cannot be undone. Although the normalize message calculates a normalized list based on the value passed as its argument, the message does not update the values in the multislider object itself or display them.
- `paste` — Paste multislider values if settype, setstyle, and setminmax are the same
  The paste message pastes multislider values that were previously copied if the settype, setstyle, and setminmax attributes are the same. Pasting will fail if the multislider that you paste into does not have the same settype, setstyle, and setminmax attributes.
- `pastereplace` — Paste multislider values and replace settype, setstyle, and setminmax
  The pastereplace message pastes multislider values that were previously copied and sets the settype, setstyle, and setminmax attributes so that they are the same as what was copied.
- `peakreset` — Reset peak values to current slider values
- `quantiles(numbers: list)` — Output the quantile for an input list
  In left inlet: The word quantiles, followed by a list of floats between 0 and 1.0, multiplies each list element by the sum of all the values in the multislider. This result is then divided by 215 (32,768). Then, multislider sends out the address at which the sum of all values up to that address is greater than or equal to the result for each list element.
- `range(minimum: number, maximum: number)` — Set the range for all sliders
  The word range followed by a minimum number and a maximum number will set all sliders to operate within that range.
- `scrollclear` — Clear the multislider object in scrolling mode.
- `select(value-pairs: list)` — Set selected slider values
  Selectively sets slider values. For example, select 1 30 2 4 5 50 sets the first slider to 30, the second to 4, and the fifth slider to 50 (the top or leftmost slider is always number 1).
- `set(slider: number, value: number)` — Set a slider value with no output
  The word set, followed by a slider number and a value, sets the numbered slider to that value without triggering any output.
- `setlist(values: list)` — Set slider values from a list with no output
  The word setlist, followed by a list of slider values, sets the sliders to the listed values without triggering any output.
- `setmax(value: float)` — Set the maximum value for all sliders
  The word setmax, followed by a number, sets the high values for the multislider object.
- `setmin(value: float)` — Set the maximum value for all sliders
  The word setmin, followed by a number, sets the low values for the multislider object.
- `sum` — Outputs a sum of all slider values as a float
  Outputs a sum of all current slider values as a float.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

### shadow

```
Example — [multislider]
  fan-in:
    in0 ← [attrui @shadowproportion]    # blend point
    in0 ← [attrui @shadowalpha]    # brightness of shadow gradient (0 is off)
    in0 ← [attrui @shadoworientation]    # orientation of shadow gradient
    in0 ← [attrui @shadowblend]    # background / color blend
```

Attributes demonstrated: `@shadowalpha`, `@shadowblend`, `@shadoworientation`, `@shadowproportion`

### more attributes

```
Example — [multislider]
  fan-in:
    in0 ← [attrui @contdata]    # output data continously when mousing
    in0 ← [attrui @signed]    # set "origin" of bar sliders
    in0 ← [attrui @thickness]    # change width of thin line
    in0 ← [attrui @spacing]    # set distance between sliders
    in0 ← [attrui @setstyle]    # change slider type to "thin line"
  fan-out:
    out0 → [message "0. -0.132075 -0.132075 0.232704 0.18239 0.132075 0.044025 -0.18239 -0.396226 -0.647799 -0.748428 -0.534591 -0.383648 -0.018868 0. 0."]:in1
```

Attributes demonstrated: `@contdata`, `@setstyle`, `@signed`, `@spacing`, `@thickness`

### appearance

```
Example — [multislider]
  fan-in:
    in0 ← [attrui @size] ← [message "8"]    # The size and candycane attributes control the look of the sliders
    in0 ← [attrui @peakcolor]
    in0 ← [attrui @bgcolor]
    in0 ← [attrui @slidercolor]
    in0 ← [attrui @drawpeaks]    # Enable drawpeaks and move a slider to see the peakcolor
    in0 ← [attrui @candycane]
    in0 ← [attrui @style]    # Create and apply styles using the format palette or type a style name here
```

Attributes demonstrated: `@bgcolor`, `@candycane`, `@drawpeaks`, `@peakcolor`, `@size`, `@slidercolor`, `@style`

### peak-meters

```
Example #1 — [multislider]
  fan-in:
    in0 ← [message "peakreset"]    # reset the peaks
    in0 ← [attrui @drawpeaks]
    in0 ← [coll] ← [message "next"]
```

```
Example #2 — [multislider]
  fan-in:
    in0 ← [message "peakreset"]    # reset the peaks
    in0 ← [attrui @drawpeaks]
    in0 ← [coll] ← [message "next"]
```

Attributes demonstrated: `@drawpeaks`

### getting index/value

> When you drag very quickly using the mouse, multiple values of the list that the multislider object outputs can vary. To deal with rapid mousing, you can use the zl compare object to output a list as shown here.

```
Example — [multislider]
  fan-out:
    out0 → [t l l l]:in0
    out0 → [t l l l]:in0
```

### messages

> select slider1 value1 slider2 value2, etc

> The setlist message sets all sliders at once but does not output the slider values

```
Example #1 — [multislider]
  fan-in:
    in0 ← [message "fetch $1"]    # fetch a single slider value and output out the right outlet
    in0 ← [message "-0.222222 0.388889 0.138889 0.611111 0.277778 -0.833333 -0.444444 1. 0.583333 -0.194444"]
    in0 ← [message "sum"]
    in0 ← [message "minimum"]    # fetch the minimum slider value
    in0 ← [message "maximum"]    # fetch the maximum slider value
  fan-out:
    out1 → [flonum]:in0
    out1 → [print Single_Value @popup 1]:in0
```

```
Example #2 — [multislider]
  fan-in:
    in0 ← [message "max"]    # set all sliders to maximum value
    in0 ← [message "min"]    # set all sliders to minimum value
    in0 ← [message "select 1 0.9 3 0. 5 -0.5"]    # selectively position individual sliders
    in0 ← [message "set 1 0.9 3 0. 5 -0.5"]    # The set message functions identically to the select message except it does not output the slider values
    in0 ← [message "setlist 0.2 -0.3 0.4 0.7 -0.8"]
  fan-out:
    out0 → [print @popup 1]:in0
```

### basic

> Slider values may be either ints or floats.
>
> Bang causes current values to be output.
>
> The number of sliders, their range, style and additional attributes can all be set from the object's Inspector. The drawing styles include bar or thin-line slider handles (when using it as a slider), or point-value or zero-to-point in scrolling mode.

> Note: Shift-Click on a slider will only move the initially selected slider reguardless of which slider the mouse arrow is currently on.

> Note: Scrolling while in overdrive may not give accurate results. In overdrive, drawing is low priority.

```
Example #1 — [multislider]  outputs a list of current slider values
  fan-in:
    in0 ← [preset]    # also works with the preset object:
    in0 ← [multislider]
```

```
Example #2 — [multislider]  move the slider and watch the scrolling display graph the slider's history
  fan-in:
    in0 ← [multislider]
```

```
Example #3 — [multislider]
  fan-in:
    in0 ← [preset]    # also works with the preset object:
    in0 ← [message "55 44 33 22 11"]
    in0 ← [message "10 50 100"]    # lists set the sliders to the respective values. multislider automatically reconfigures itself so it has the same the number of sliders as elements in the list.
    in0 ← [number]    # an int (or float) will set all sliders to the indicated value
    in0 ← [message "10 20 30 40 50 60 70 80 90 100"]
  fan-out:
    out0 → [multislider]:in0    # outputs a list of current slider values
    out0 → [message "10 20 30 40 50 60 70 80 90 100"]:in1
```

```
Example #4 — [multislider]  <-- point-value scrolling / Notice that the scrolling display creates one "track" for each element in the input list!
  fan-in:
    in0 ← [p EKG] ← [toggle]    # <-- turn on the medical equipment to monitor Max's heartbeat / p EKG emits: "0"
```

```
Example #5 — [multislider]  zero-to-pointvalue scrolling
  fan-in:
    in0 ← [p EKG] ← [toggle]    # <-- turn on the medical equipment to monitor Max's heartbeat / p EKG emits: "0"
```

```
Example #6 — [multislider]
  fan-out:
    out0 → [multislider]:in0    # move the slider and watch the scrolling display graph the slider's history
```

## See also

`itable`, `kslider`, `matrixctrl`, `pictslider`, `rslider`, `slider`
