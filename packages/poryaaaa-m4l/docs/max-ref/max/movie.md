# movie

_max · U/I_

> Play a movie in a window

Plays a movie in a separate window. All movie playback control is managed by messages to the object.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | int Locates, start Plays, Many Other Commands |
| out0 | Movie Time in Response To time Message |
| out1 | Horiz Mouse Location When Clicked in Movie |
| out2 | Vert Mouse Location When Clicked in Movie |

## Arguments

- **filename** (`symbol`) _(optional)_ — Movie file name
  Specifies the name of a movie file to be read into movie automatically when the patch is loaded. The same effect can be achieved for imovie by selecting the object in an unlocked patcher and choosing Get Info... from the Object menu to select a movie file. Both objects retain the name(s) of the movie(s) they have loaded at the time that the patch is saved, and attempt to load the same movie(s) the next time the patch is opened.

## Messages

- `bang` — Restart playback at the current location
  Same as resume.
- `int(location: int)` — Set the current location
  Sets the current time location of the movie. If the movie is playing, it will play from the newly set location. 0 is always the beginning. The end time varies from one movie to another. (The length message reports the end time location out the left outlet.)
- `active(flag: int)` — Set the active flag
  The word active, followed by a non-zero number, makes the movie active (the default). Followed by a 0, active makes the movie inactive. An inactive movie will not play or change location.
- `clear` — Close window and remove all movies
  Has the same effect as dispose with no arguments.
- `dispose(filename: symbol)` — Close window and remove movies
  Closes the movie window if it is open, and removes all movies from the movie object's memory. If the word dispose is followed by the name of a loaded movie, only the named movie will be removed.
- `duration` — Report the movie duration
  Reports the duration of the movie (in time units) out the left-most outlet.
- `ff` — Fast-forward the movie
  The word ff fast-forwards the movie.
- `getduration` — Report the movie duration
  Reports the end time position of the movie (in QuickTime Time Units) from the left outlet.
- `getrate` — Report the current playback rate
  Reports the current rate multiplied by 65536 out the right outlet. Thus, normal speed is reported as 65536, half speed is reported as 32768, double speed backward is reported as -131072, etc. If the movie is not playing, the rate is reported as 0, and if no movie has yet been loaded nothing is sent out.
- `gettime` — Report the current time location
  Reports the current time location of the movie.
- `length` — Report the movie duration
  The length message's functionality is equivalent to the getduration message.
- `loadintoram(flag: int)` — Load the movie into RAM memory
  The word loadintoram, followed by a non-zero number, attempts to load the entire movie into memory, if possible. The default is 0.
- `loop(flag: int)` — Set the looping playback mode
  The word loop, followed by a number in the range 0-2, controls looping for the current film on. The options are:
  0: looping off (default) 1: looping on 2: palindrome mode (forward and then backward)
- `loopend(end: int)` — Set the loop end point
  The word loopend, followed by a number, sets the end point of a loop. The default value is corresponds to the end of the film.
- `looppoints(beginning: int, end: int)` — Set both loop points
  The word looppoints, followed by two numbers, sets the beginning and end points of a loop. the default values are 0 (i.e., the start of the film) for the start point and the end of the film for the endpoint.
- `loopset(beginning: int, end: int)` — Set the loop points
  The loopset message's functionality is equivalent to the looppoints message.
- `loopstart(beginning: int)` — Set the loop start point
  The word loopstart, followed by a number, sets the beginning point of a loop. The default value is 0 (i.e., the start of the film).
- `matrix(matrix: list)` — Perform a transform on the current movie
  The word matrix, followed by nine floating point numbers, reloads the current movie into RAM after performing a transformation matrix operation on the image. This transformation is the same one used for the mapping in QuickTime of points from one coordinate space (i.e, the original image) into another coordinate space (a scaled, rotated, or translated version of the original image).
  The transform matrix operation consists of nine matrix elements
- `mute(flag: int)` — Turn off movie sound
  The word mute, followed by a non-zero number, turns off the movie's sound (if it has any). Followed by a 0, mute turns on the movie's sound (the default).
- `next(time-units: int)` — Move the playback location forward
  The word next, followed by a number, moves the time location ahead by that amount. If no number is supplied, next moves the time ahead by 5. (The actual time meaning of these units varies from movie to movie.)
- `nextmovie` — Select previous movie for playback
  Stops the movie if it is playing, and switches to the movie that was loaded just prior to the current movie. (The movies are stored in reverse order from the order in which they were loaded.) If there is no prior movie, nextmovie wraps around back to the most recently loaded movie. Note that the title of the movie window is not automatically changed, even though the "current movie" has been changed by nextmovie.
- `open` — Bring the movie window to the foreground
  Brings the movie window to the foreground (applies only to movie, not imovie).
- `palindrome(flag: int)` — Set palindrome mode
  The word palindrome, followed by a non-zero number, sets the movie to play in palindrome mode (forward and then backward). Looping must be turned on. palindrome 0 (the default) disables palindrome mode.
- `passive(mode: int)` — Set the passive mode
  The word passive, followed by a non-zero number, sets the passive mode. In passive mode, starting a movie will not cause the frame to change unless a bang message is received. passive 0 (the default) sets the movie object to respond to normal start messages.
- `pause` — Pause movie playback
  Stops the movie.
- `pos(index: float)` — Jump to a position within the movie
  The word pos followed by a floating-point number which denotes a position-index within the movie will cause the playback-position to jump to the specified position in the movie.
- `prev(time-units: int)` — Move the playback location backward
  The word prev, followed by a number, moves the time location backward by that amount. If no number is supplied, prev moves the time backward by 5.
- `quality(interval: int)` — Set the movie redraw interval
  The word quality, followed by a number, sets the minimum interval, in milliseconds, between movie redraws. The default is 0 (i.e., no minimum).
- `rate(input: list)` — Set playback rate
  The word rate, followed by one or more integers or floats, sets the playing speed of the movie. If rate is followed by one integer, that number is taken to be a whole number playing speed. If rate is followed by two numbers, the first number is taken to be the numerator and the second the denominator of a fractional speed. 1 is the normal playing speed, 0 means the movie is stopped, and a negative rate plays backwards. rate 1 2 would play the movie at half speed. Immediately after you send a non-zero rate message, the movie will begin playing, so you may wish to precede any rate messages with an integer to locate to the desired starting position.
- `rd(filename: symbol)` — Open a movie file
  Same as read.
- `read(filename: symbol)` — Open a movie file
  The word read, followed by a symbol, looks for a movie file with that name in Max's file search path, and opens it if it exists, displaying the movie's first frame in a movie window. If the filename contains any spaces or special characters, the name should be enclosed in double quotes or each special character should be preceded by a backslash (\). The word read by itself puts up a standard Open Document dialog box and reads in any movie file you select. The read message will open at least 26 different types of files that can be opened by QuickTime, these include movie files such as MPEG, audio files including AIFF and MP3, and graphics files including GIF and JPEG.
- `readany(filename: symbol)` — Open any file as a movie
  The readany message opens any type of file, using QuickTime routines to try to interpret it as a movie or other supported media file.
- `rect(x-position-coordinate: int, y-position-coordinate: int, width (pixels): int, height (pixels): int)` — Set the movie location within the window
  The word rect, followed by four numbers, specifies the size of the rectangle in which the movie is displayed within the movie window. The first two numbers specify the position of the rectangle within the movie window, in relative coordinates, and the second two numbers specify the width and height, in pixels, of the rectangle.
- `reload` — Reload the current movie
  The word reload will reload the current movie into memory (can be used to refresh; for example, if a movie is playing and the stop message is sent followed by reload, the movie will reload into memory and be set to play from the beginning as a newly loaded movie).
- `resume` — Restart playback at the current location
  Begins playing the movie from its current location, at the most recently specified rate.
- `rw` — Rewind the movie
  The word rw rewinds the movie.
- `start(filename: symbol)` — Start playback from the beginning
  Sets the movie's rate to 1 and begins playing from the beginning. If the word start is followed by the name of a specific loaded movie, that movie becomes the current movie before starting.
- `startat(location: list)` — Start playback at a specific location
  The word startat, followed by a number, set the current time location of the movie and begins playing from that point.
- `stop` — Stop movie playback
  Stops the movie.
- `switch(filename: symbol)` — Switch movie used for playback
  The word switch, followed by a symbol, makes the named movie the active one without changing the transport state (See the start message).
- `time(frame: list)` — Set the movie location to a specific frame
  The word time, followed by a number that specifies a QuickTime frame number, goes to the time location specified by the number. When no argument is present, the time message's functionality is equivalent to the gettime message.
- `timescale` — Report the movie timescale
  Reports the timescale of the movie out the left-most outlet.
- `toggleplay` — Toggle movie playback
  The word toggleplay activates or pauses playback of the movie.
- `vol(volume: int)` — Set movie sound volume
  The word vol, followed by a number in the range 1-255, sets the movie's sound volume. Optionally, the volume can be set by using the word vol, followed by a floating-point value in the range 0. - 1.0.
- `wclose` — Close the movie window
  Closes the movie window.
- `windowpos(left-coordinate: int, top-coordinate: int, right-coordinate: int, bottom-coordinate: int)` — Set the movie window location
  The word windowpos, followed by four numbers, specifies the location and size of the movie window on the screen. The four numbers specify the left, top, right, and bottom of the movie window in global coordinates. This message is only supported by the movie object, not the imovie object.

## GUI behaviors

- `(drag)` — Load a movie file
  When a QuickTIme movie file is dragged from the Max File Browser to a movie object, the file will be loaded.
- `(mouse)` — Activate the movie window
  Double-clicking on the movie object will make the movie window active.

## Attributes

- `@basic` (int)
- `@default` (int)
- `@label` (symbol)
- `@paint` (int)
- `@save` (int)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example — [movie]
  fan-in:
    in0 ← [message "startat 100"]    # start at a time location
    in0 ← [button]    # movie starts (optional name for movie to play)
    in0 ← [r to_movie_obj]    # double-click to make movie window active
    in0 ← [message "start"]
    in0 ← [message "prev 100"]    # move back
    in0 ← [message "next 100"]    # move ahead 100 (time units). Without argument, prev and next use current increment (default is 5 )
    in0 ← [message "2000"]    # int sets current time location (whether playing or stopped)
    in0 ← [message "resume"]
    in0 ← [message "pause"]
    in0 ← [message "stop"]
  fan-out:
    out0 → [number]:in0    # left outlet: movie time (time message)
    out1 → [number]:in0
    out2 → [number]:in0    # right outlet: y coord of mouse click in window / middle outlet: x coord of mouse click in window
```

## See also

`imovie`
