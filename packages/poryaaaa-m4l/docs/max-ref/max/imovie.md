# imovie

_max · U/I_

> Play video

Plays a movie in a user-interface object within the patcher window.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int Locates, start Plays, Many Other Commands |
| out0 | Movie Time in Response To time Message |
| out1 | Horiz Mouse Location When Clicked in Movie |
| out2 | Vert Mouse Location When Clicked in Movie |

## Messages

- `bang` — Begin playback from the current location
  Same as resume.
- `int(location: int)` — Set the current location within the movie
  Sets the current time location of the movie. If the movie is playing, it will play from the newly set location. 0 is always the beginning. The end time varies from one movie to another. (The length message reports the end time location out the left outlet.)
- `active(flag: int)` — Make a move active
  The word active, followed by a non-zero number, makes the movie active (the default). Followed by a 0, active makes the movie inactive. An inactive movie will not play or change location.
- `clear` — Remove all movie files
  Has the same effect as dispose with no arguments.
- `dispose(filename: symbol)` — Remove a movie file from memory
  Removes all movies from the imovie object's memory. If the word dispose is followed by the name of a loaded movie, only the named movie will be removed.
- `duration` — Output the movie duration
  Reports the duration of the movie (in milliseconds) from the left outlet.
- `getduration` — Output the movie duration
  Reports the end time position of the movie (in Time Units) from the left outlet.
- `getrate` — Output the playback rate
  Reports the current rate multiplied by 65536 out the right outlet. Thus, normal speed is reported as 65536, half speed is reported as 32768, double speed backward is reported as -131072, etc. If the movie is not playing, the rate is reported as 0, and if no movie has yet been loaded nothing is sent out.
- `gettime` — Output the current playback location
  Reports the current time location of the movie.
- `length` — Output the movie length
  The length message's functionality is equivalent to the getduration message.
- `loadintoram(flag (0 or non-zero): int)` — Load a movie into RAM memory
  The word loadintoram, followed by a non-zero number, attempts to load the entire movie into memory, if possible. The default is 0.
- `loop(flag: int)` — Set the looping mode
  The word loop, followed by a number in the range 0-2, controls looping for the current film on. The options are:
  0: looping off (default) 1: looping on 2: palindrome mode (forward and then backward)
- `loopend(endpoint: int)` — Set loop end
  Sets the ending point of a loop. The default value the time value that represents the end of the movie.
- `looppoints(start: int, end: int)` — Set both loop points
  The word looppoints, followed by two numbers, sets the beginning and end points of a loop. the default values are 0 (i.e., the start of the film) for the start point and the end of the film for the endpoint.
- `loopset(start: int, end: int)` — Set both loop points
  See the looppoints entry.
- `loopstart(startpoint: int)` — Set loop start
  Sets the beginning point of a loop. The default value is 0 (the start of the movie).
- `matrix(transform: list)` — Perform a transformation matrix
  The word matrix, followed by nine floating point numbers, reloads the current movie into RAM after performing a transformation matrix operation on the image. This transformation is the same one used for the mapping points from one coordinate space (i.e, the original image) into another coordinate space (a scaled, rotated, or translated version of the original image).
  The transform matrix operation consists of nine matrix elements
- `mute(flag: int)` — Set movie audio mute status
  The word mute, followed by a non-zero number, turns off the movie's sound (if it has any). Followed by a 0, mute turns on the movie's sound (the default).
- `next(time-units: int)` — Move the current location forward
  The word next, followed by a number, moves the time location ahead by that amount. If no number is supplied, next moves the time ahead by 5. (The actual time meaning of these units varies from movie to movie.)
- `nextmovie` — Stop playback and switch movies
  Stops the movie if it is playing, and switches to the movie that was loaded just prior to the current movie. (The movies are stored in reverse order from the order in which they were loaded.) If there is no prior movie, nextmovie wraps around back to the most recently loaded movie. Note that the title of the movie window is not automatically changed, even though the "current movie" has been changed by nextmovie.
- `palindrome(flag: int)` — Set the palindrome playback mode
  The word palindrome, followed by a non-zero number, sets the movie to play in palindrome mode (forward and then backward). Looping must be turned on. palindrome 0 (the default) disables palindrome mode.
- `passive(flag: int)` — Set passive mode flag
  The word passive, followed by a non-zero number, sets the passive mode. In passive mode, starting a movie will not cause the frame to change unless a bang message is received. passive 0 (the default) sets the movie object to respond to normal start messages.
- `pause` — Pause movie playback
- `prev(time-units: int)` — Move the current location backward
  The word prev, followed by a number, moves the time location backward by that amount. If no number is supplied, prev moves the time backward by 5.
- `quality(interval: int)` — Set the minimum redraw interval
  The word quality, followed by a number, sets the minimum interval, in milliseconds, between movie redraws. The default is 0 (i.e., no minimum).
- `rate(speed: list)` — Set playback speed
  The word rate, followed by one or more integers or floats, sets the playing speed of the movie. If rate is followed by one integer, that number is taken to be a whole number playing speed. If rate is followed by two numbers, the first number is taken to be the numerator and the second the denominator of a fractional speed. 1 is the normal playing speed, 0 means the movie is stopped, and a negative rate plays backwards. rate 1 2 would play the movie at half speed. Immediately after you send a non-zero rate message, the movie will begin playing, so you may wish to precede any rate messages with an integer to locate to the desired starting position.
- `rd(filename: symbol)` — Load a movie
  Same as read.
- `read([filename: symbol])` — Load a movie
  The word read, followed by a symbol, looks for a movie file with that name in Max's file search path, and opens it if it exists, displaying the movie's first frame in a movie window. If the filename contains any spaces or special characters, the name should be enclosed in double quotes or each special character should be preceded by a backslash (\). The word read by itself puts up a standard Open Document dialog box and reads in any movie file you select. The read message will open at least 26 different types of files that can be opened. These include movie files such as MPEG, audio files including AIFF and MP3, and graphics files including GIF and JPEG.
- `readany([filename: symbol])` — Load a media file
  The readany message opens any type of file to try to interpret it as a movie or other supported media file.
- `rect(x: int, y: int, width: int, height: int)` — Specify the display rectangle
  The word rect, followed by four numbers, specifies the size of the rectangle in which the movie is displayed within the movie window. The first two numbers specify the position of the rectangle within the movie window, in relative coordinates, and the second two numbers specify the width and height, in pixels, of the rectangle.
- `reload` — Reload a movie file
  The word reload will reload the current movie into memory (can be used to refresh; for example, if a movie is playing and the stop message is sent followed by reload, the movie will reload into memory and be set to play from the beginning as a newly loaded movie).
- `resume` — Begin playback from the current location
  Begins playing the movie from its current location, at the most recently specified rate.
- `start(filename: symbol)` — Start playback from the beginning
  Sets the movie's rate to 1 and begins playing from the beginning. If the word start is followed by the name of a specific loaded movie, that movie becomes the current movie before starting.
- `startat(location: list)` — Start playback from a specified location
  The word startat, followed by a number, set the current time location of the movie and begins playing from that point.
- `stop` — Stop movie playback
  Stops the movie.
- `switch(filename: symbol)` — Activate a new movie
  The word switch, followed by a symbol, makes the named movie the active one without changing the transport state (See the start message).
- `time([frame: int])` — Move to a specific frame
  The word time, followed by a number that specifies a frame number, goes to the time location specified by the number. When no argument is present, the time message's functionality is equivalent to the gettime message.
- `timescale` — Output the movie timescale
  Reports the timescale of the movie (the number of Time Units per second) from the left outlet.
- `vol(volume: number)` — Set movie audio volume
  The word vol, followed by a number in the range 1-255, sets the movie's sound volume. Optionally, the volume can be set by using the word vol, followed by a floating-point value in the range 0. - 1.0.

## GUI behaviors

- `(drag)` — Load a file with drag-and-drop
  When a QuickTIme movie file is dragged from the Max File Browser to a imovie object, the file will be loaded.
- `(mouse)` — Activate the movie window
  Double-clicking on the imovie object will make the movie window active.

## Attributes

- `@basic` (int)
- `@category` (atom)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@save` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `border` — seen as: `border $1`

## Help patcher examples

### transport

```
Example — [imovie]  movie length or time reported here
  fan-in:
    in0 ← [message "start"]    # Start playback
    in0 ← [message "start"]
    in0 ← [message "213"]    # Set current time location (QuickTIme units)
    in0 ← [message "pause"]
    in0 ← [message "stop"]
    in0 ← [message "next 50"]    # Move ahead 50 QuickTIme units
    in0 ← [message "prev 20"]    # Move back 20 QuickTIme units
```

### looping

```
Example — [imovie]
  fan-in:
    in0 ← [playbar]
    in0 ← [message "loopset 100 200"]
    in0 ← [message "loopstart 100"]    # Set the loop start (default 0)
    in0 ← [message "loop $1"]    # Turn looping on/off
    in0 ← [message "loopend 2000"]    # Set both start and end / Set the loop end (the default is the end of the movie)
    in0 ← [message "palindrome $1"]    # Set the looping mode (normal or palindrome)
```

### speed and time

```
Example — [imovie]
  fan-in:
    in0 ← [message "rate 1"]    # Play at default speed
    in0 ← [message "time"]    # Send current movie time out the object's left outlet
    in0 ← [message "rate 1 2"]    # Two numbers specify a fractional playback rate (1/2)
    in0 ← [message "rate 0"]    # Stop playback
    in0 ← [message "rate -1.5"]    # Play in reverse at faster speed
    in0 ← [playbar]
    in0 ← [message "length"]    # Send movie length out the object's left outlet
  fan-out:
    out0 → [number]:in0    # movie length or time reported here
```

### files and memory

> Note: the movie box can be rescaled, but the movie scale is independent of the box size

```
Example — [imovie]
  fan-in:
    in0 ← [message "loadintoram $1"]    # Load entire movie into memory (if possible)
    in0 ← [message "switch foo"]    # Make the named movie the active one without changing the transport state
    in0 ← [message "nextmovie"]    # If more than one movie has been loaded, make the next one current
    in0 ← [message "dispose"]    # Dispose of movie and its memory (optional filename argument)
    in0 ← [message "read"]    # Read a movie file (optional filename argument)
    in0 ← [message "reload"]    # Reload current movie file and purge extra memory used
    in0 ← [playbar]
    in0 ← [message "readany"]    # Read any type of file (optional filename argument)
```

### basic

> Send the current movie time out the object's left outlet
>
> Use the playbar object to control playback
>
> In an unlocked patcher, drag and drop a moviefile from the File Browser to load a file (or use the object's Inspector). The name of the last file used is saved in the patcher.
>
> Click on a movie to send the horizontal and vertical mouse positon coordinates out the object's center/right outlets
>
> movie time
>
> Note: the movie box can be rescaled, but the movie scale is independent of the box size

```
Example — [imovie]
  fan-in:
    in0 ← [message "border $1"]    # Draw a border around the movie display area
    in0 ← [message "gettime"]
    in0 ← [playbar]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0    # horizontal mouse location
    out2 → [number]:in0    # vertical mouse location
```

## See also

`lcd`, `movie`, `playbar`
