# lcd

_max · U/I_

> Display graphics (deprecated)

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Drawing Commands |
| out0 | Mouse (X,Y) Location When Clicked |
| out1 | Mouse Idle (X,Y) Position |
| out2 | Mouse Down (1/0) |
| out3 | Dump Output |

## Messages

- `anything(arguments: list)` — anything refers to the backwards-compatible capitalized messages.
- `ascii(ascii value: int)` — Write an ASCII character
  The word ascii, followed by a number between 0 and 255, writes the character corresponding to that ASCII value at the current pen position, then moves the pen position to the right of that character. Numbers that exceed the 0-255 range are restricted to that range with a modulus operation.
- `backsprite(any symbol: list)` — Set a named sprite's drawing order so that it is drawn first/displayed last
  The word backsprite, followed by a symbol, sets the named sprite's drawing order so that it is drawn first (and displayed last). This command can be used to alter the order in which sprites are drawn. (Normally, sprites are drawn in the order they are recorded.)
- `brgb(red: int, green: int, blue: int)` — Set the background color
- `clear(arguments: list)` — Erase the contents of the lcd display
- `clearpicts` — Delete all of an lcd object's named pictures.
  The word clearpicts, followed by a symbol, deletes all of an lcd object's named pictures from memory so that they can no longer be drawn. To remove the images from the lcd object's display, the command should be followed by a clear message.
- `clearsprites` — Delete all named sprites
  Deletes all of an lcd object's named sprites.
- `closesprite(any symbol: list)` — Stop sprite command collection and associate the defined region with a name
  The word closesprite, followed by a symbol argument that names the sprite, turns off sprite command collection and associates the defined region with the symbol. After the closesprite message, drawing commands function normally again.
- `color(color-id: int)` — Specify a Max color palette selection for drawing graphics
  Max color palette selections are specified by a number in the range 0 - 255. Values exceeding 255 are range-restricted using a modulus operation.
- `deletepict(any symbol: list)` — Delete a named picture
  The word deletepict, followed by a symbol, deletes the named the picture from memory so that it can no longer be drawn. To remove the image from the lcd object's display, the command should be followed by a clear message.
- `deletesprite(any symbol: list)` — Delete a named sprite
  The word deletesprite, followed by a symbol, deletes the named sprite.
- `drawpict(arguments: list)` — Draw a named picture
  The word drawpict, followed by a symbol, draws the named picture. Optionally there may follow four numbers specifying a destination rectangle in which the picture is scaled and drawn, and source rectangle that specifies the area of the picture to use in the operation. These rectangles are specified as left, top, width, and height values in pixels. The destination rectangle is relative to the top left corner of the lcd display area. The source rectangle is relative to the top, left corner of the picture. If not present, these rectangles are both set to be the same size as the picture.
- `drawsprite(any symbol: list)` — Draw a named sprite
  The word drawsprite, followed by a symbol, draws the named sprite. Optionally this may be followed by a pair of numbers that specify a horizontal and vertical offset for drawing the sprite.
- `font(font-id: list, font-size: list)` — Specify a font ID and a font size for text drawing
  The font message specifies a font ID using the font number mapping that Max 6 uses generally and a font size to select a font used when drawing text in response to a write or ascii message.
- `framearc(left: int, top: int, right: int, bottom: int, start-angle: degrees, degrees-arc: degrees, [red: int], [green: int], [blue: int])` — Paint an unfilled arc
  Same as paintarc except that only the unfilled outline of the arc is drawn.
- `frameoval(left: int, top: int, right: int, bottom: int, [red: int], [green: int], [blue: int])` — Paint an unfilled oval
  Same as paintoval except that only the unfilled outline of the oval is drawn.
- `framepoly(x/y int pairs: list)` — Paint an unfilled polygon
  Same as paintpoly except that only the unfilled outline of the polygon is drawn.
- `framerect(left: int, top: int, right: int, bottom: int, [red: int], [green: int], [blue: int])` — Paint an unfilled rectangle
  Same as paintrect except that only the unfilled outline of the rectangle is drawn.
- `frameroundrect(left: int, top: int, right: int, bottom: int, horizontal-roundness: int, vertical-roundness: int, [red: int], [green: int], [blue: int])` — Paint an unfilled rounded rectangle
  Same as paintroundrect except that only the unfilled outline of the rounded rectangle is drawn.
- `frgb(red: int, green: int, blue: int)` — Set the foreground (pen) color
- `frontsprite(any symbol: list)` — Set a named sprite's drawing order so that it is drawn last/displayed first
  The word frontsprite, followed by a symbol, sets the named sprite's drawing order so that it is drawn last (and displayed first). This command can be used to alter the order in which sprites are drawn. (Normally, sprites are drawn in the order they are recorded.)
- `getpenloc(local-coordinates: list)` — Output the current pen location
  The word getpenloc outputs a message consisting of the word penloc followed by two numbers, out the lcd object's right outlet. The numbers represent local coordinates relative to the top-left corner of the lcd display area. The first number is the number of pixels to the right of that corner, and the second number is the number of pixels down from that corner.
- `getpixel(x-offset: int, y-offset: int)` — Output the current pixel color
  The word getpixel, followed by two integers specifying an offset from the top left corner of the lcd object's display, sends a message consisting of the word pixel followed by three numbers that specify the color of the pixel in RGB format and two additional numbers that specify the pixel offset.
- `getsize` — Output the largest size of the lcd canvas (patching / presentation).
  Ouputs a message from the right outlet consisting of the word size and two numbers which report the width and height in pixels of the lcd display area.
- `hidesprite(any symbol: list)` — Disable the drawing of a named sprite
- `line(x position: int, y position: int)` — Draw a line and move the pen position
  The word line, followed by two int arguments for horizontal and vertical offset, in pixels, relative to the current pen position, draws a line from the current pen position to a point determined by the specified offset, and that point becomes the new pen position. Positive arguments draw the line to the right or down; negative arguments draw up or to the left.
- `linesegment(x start: int, y start: int, x end: int, y end: int)` — Draw a line segment
  The word linesegment, followed by four int arguments that specify the endpoints of a line segment, draw a line. The numbers represent the horizontal and vertical offset of the beginning endpoint, and the horizontal and vertical offset of the finishing endpoint, in pixels, relative to the top left corner of the lcd display area. A color may optionally be specified using a single number that selects a color from Max's color palette (similar to the color message), or by using three additional numbers that describe an RGB value (similar to the frgb message).
- `lineto(x endpoint: int, y endpoint: int)` — Draw a line
  The word lineto, followed by two int arguments for horizontal and vertical ending point, draws a line from the current pen position to the position specified by the arguments.
- `move(x position: int, y position: int)` — Move the pen position
  Moves the pen position a certain number of pixels down from, and to the right of, its current position. The word move must be followed by two int arguments for horizontal and vertical offset, in pixels, relative to the current pen position. Negative arguments may be used to move the pen position up or to the left.
- `moveto(x position: int, y position: int)` — Move the pen position
  Sets the pen position at which the next graphic instruction will be drawn. The moveto message must include two int arguments for horizontal and vertical offset, in pixels, relative to the upper left corner of the lcd display area.
- `oprgb(red: int, green: int, blue: int)` — Set the penmode opcolor (legacy)
  The word oprgb, followed by three numbers between 0 and 255, specify an RGB value used as the opcolor for penmodes that support it.
  If the penmode is set to 32, the alpha channel value is derived by averaging the three RGB values and mapping the average to the range 0. - 1.0 (e.g., an oprgb value of 128 128 128 would have an alpha value of .5 when the penmode is set to 32).
- `paintarc(left: int, top: int, right: int, bottom: int, start-angle: degrees, degrees-arc: degrees, [red: int], [green: int], [blue: int])` — Paint a filled arc
  The word paintarc, followed by six int arguments that specify the left, top, right, and bottom extremities of an oval across which the arc will be drawn, the start angle (0 is the top) and degrees of arc, paints an arc.
- `paintoval(left: int, top: int, right: int, bottom: int, [red: int], [green: int], [blue: int])` — Paint a filled oval
  The word paintoval, followed by four int arguments specifying the left, top, right, and bottom extremities of an oval, paints an oval. These extremities are specified in pixels, relative to the top left corner of the lcd display area. A color may optionally be specified using a single number that selects a color from Max's color palette (similar to the color message), or by using three additional numbers that describe an RGB value (similar to the frgb message).
- `paintpoly(x/y int pairs: list)` — Paint a filled polygon
  The word paintpoly may be followed by as many as 254 int arguments that would specify a series of x/y pairs that define a polygon to be painted in lcd. These x/y pairs are specified in pixels, relative to the top left corner of the lcd display area.
- `paintrect(left: int, top: int, right: int, bottom: int, [red: int], [green: int], [blue: int])` — Paint a filled rectangle
  Left, top, right and bottom positions for the rectangle are specified in pixels, relative to the top left corner of the lcd display area when painting a filled rectangle. A color may optionally be specified using a single number that selects a color from Max's color palette (similar to the color message), or by using three additional numbers that describe an RGB value (similar to the frgb message).
- `paintroundrect(left: int, top: int, right: int, bottom: int, horizontal-roundness: int, vertical-roundness: int, [red: int], [green: int], [blue: int])` — Paint a filled rounded rectangle
  The word paintroundrect, followed by six int arguments specifying the left, top, right, and bottom positions of a rectangle and the amount of horizontal and vertical roundness in pixels, paints a rounded rectangle. The edge positions are specified in pixels, relative to the top left corner of the lcd display area. A color may optionally be specified using a single number that selects a color from Max's color palette (similar to the color message), or by using three additional numbers that describe an RGB value (similar to the frgb message).
- `penmode(transfer-mode: list)` — Set the pen mode
  The word penmode, followed by a number in the range 0-7, sets the transfer mode for subsequent drawing operations. The following are transfer mode constants;
  Copy 0
  Or 1
  Xor 2
  Bic 3
  NotCopy 4
  NotOr 5
  NotXor 6
  NotBic 7
- `pensize(horizontal-thickness: int, vertical-thickness: int)` — Set the pen size
  The word pensize, followed by two equal int arguments specifying horizontal and vertical thickness in pixels (e.g., 4 4), sets the current pensize. The horizontal and vertical thicknesses must be equal.
- `readpict(filename: list)` — Read a picture file from disk into RAM
  The word readpict followed by a symbol which specifies a filename, looks for a graphic file (a .pct file openable on Windows using the QuickTime Picture Viewer for Windows) with that name in Max's file search path, and reads the picture file from disk into RAM. This named picture can then be drawn in lcd with the drawpict and tilepict messages. In response to the readpict message, the object sends a message out the right outlet of the lcd object consisting of the word pict followed by a symbol which specifies the name of the picture file and two numbers which specify the file's width and height. If the read is unsuccessful, the error message pict error will be sent out the right outlet.
- `recordsprite` — Record drawing commands to be stored in a named sprite
  Initiates the recording of drawing commands which will be stored in a named sprite. While recording, drawing commands will have no effect on the contents of the lcd object's window.
- `reset(arguments: list)` — Erase the contents of the lcd display and reset the pen state
  The erase message erases the current display and resets the colors, pen, and pen position to their default states. It is equivalent to the message
  clear, pensize 1, penmode 0, frgb 0 0 0, brgb 255 255 255, moveto 0 0
- `scrollrect(left: int, top: int, right: int, bottom: int, x-offset: int, y-offset: int)` — Define a rectangle and move its contents
  The word scrollrect, followed by six int arguments that specify the left, top, right, and bottom positions of a rectangle to be scrolled and the number of pixels to scroll in the x and y direction, scrolls a rectangle within the lcd object's display area.
- `setpixel(x coordinate: int, y coordinate: int, [red: int], [green: int], [blue: int])` — Change the color of a pixel
  The word setpixel, followed by two numbers which specify a location in local coordinates relative to the top-left corner of the lcd display area and a list of three integers in the range 0-255 which specify a color in RGB format, will set the pixel at the designated location to the selected color.
- `size(horizontal-size: int, vertical-size: int)` — Change the lcd object window size
  The word size, followed by two integers, changes the lcd object window size. The Maximum size is 8192 x 8192.
- `textface(text-styles: list)` — The word textface Set a text style for text rendering
  The word textface, followed by one or more names specifying text style(s), sets the font style(s) to be used when rendering text. Text style names are normal, bold, and italic.
- `tilepict(arguments: list)` — Fill a rectangle by tiling a picture
  The word tilepict, followed by a picture name argument, fills a rectangle by tiling a picture. Optionally there may follow, four numbers that specify a destination rectangle in which the picture is tiled and four numbers that specify a source rectangle that specifies the area of the picture to use in the operation. These rectangles are specified as left, top, width, and height values in pixels. The destination rectangle is relative to the top left corner of the lcd display area. The source rectangle is relative to the top, left corner of the picture. If not present, the destination rectangle is set to the same size of lcd, and the source rectangle is set to be the same size as the picture.
- `write(any symbol: list)` — Write a symbol and move the pen position
  The word write, followed by any symbol, writes that symbol beginning at the current pen position, and moves the pen position to the end of the text.
- `writepict(filename: symbol)` — Save the display area as a PNG file
  The word writepict, followed by an optional filename argument, writes the current contents of the lcd display area to a PNG file.

## GUI behaviors

- `(mouse)` — Draw freehand with the mouse
  You can draw freehand in lcd with the mouse (provided this feature has not been turned off with a local 0 message). The mouse will draw with the current pen and color characteristics, and the mouse location will be sent out the outlet.

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

- `enablesprites` — seen as: `enablesprites $1`
- `idle` — seen as: `idle $1`, `idle 1, local 0`
- `textmode` — seen as: `textmode $1`

## Help patcher examples

### sprites

> lcd

> Sprites are collections of drawing commands that are executed and drawn on top of the lcd "background"

> Create a sprite with the recordsprite, (drawing), closesprite <name> sequence of messages

```
Example — [lcd]
  fan-in:
    in0 ← [message "enablesprites $1"]
    in0 ← [message "clearsprites"]
    in0 ← [message "hidesprite calvin"]    # Hide your sprite
    in0 ← [message "drawsprite calvin $1 $2"]    # Draw your sprite somewhere
    in0 ← [message "deletesprite calvin"]    # Delete all sprites / Delete a sprite / Enable sprites. This will use up twice as much memory if not in onscreen mode
    in0 ← [message "recordsprite"]
    in0 ← [message "paintroundrect 50 20 100 80 12 20 153 51 51"]    # Draw something / Stop recording commands and name the sprite / Start drawing a sprite
    in0 ← [message "closesprite calvin"]
    in0 ← [message "frontsprite hobbes"]    # Change the order the sprites are displayed
    in0 ← [message "frontsprite calvin"]
    in0 ← [message "recordsprite, paintarc 20 20 120 120 0 135 51 51 153, closesprite hobbes, drawsprite hobbes"]    # Create another sprite and draw it
    in0 ← [message "clear"]
```

### images and transparency

> lcd

> You can import graphic images for use in your display, and position and scale them. The 'bgtransparent' attribute can be used to set the background of lcd to transparent so that objects behind it are visible.

> Some commands (deletepict, clearpicts) delete the named the picture from memory so that they can no longer be drawn. To remove the image from the object's display, the command should be followed by a clear message. Changing background transparency is a similar kind of operation.

```
Example — [lcd]
  fan-in:
    in0 ← [attrui @bgtransparent]    # Enable transparency and click on the clear message box to see the invisible fun start
    in0 ← [message "readpict myimage lcd_benadryl.jpg"]    # Read an image file from disk and name it
    in0 ← [message "drawpict myimage 20 20"]    # Draw the image (full size) at a specific location
    in0 ← [message "deletepict myimage"]    # Delete a specific pict (Click clear to redraw)
    in0 ← [message "drawpict myimage 5 110 220 25"]    # The list describes startX, startY coordinates for the image, displayed at full size. An optional additional two ints can be added to scale the image. / Draw the image at a specific location and scale it
    in0 ← [message "clearpicts"]    # Clear all picts (Click clear to redraw)
    in0 ← [message "clear"]    # Clear the lcd to the current background color
```

Attributes demonstrated: `@bgtransparent`

### text

```
Example — [lcd]
  fan-in:
    in0 ← [message "textface bold italic outline underline extend"]    # Combine text face styles
    in0 ← [message "brgb 204 255 255"]
    in0 ← [message "font Arial 32"]    # Set the font and size (in points)
    in0 ← [message "textmode $1"]
    in0 ← [message "textface $1"]
    in0 ← [message "ascii $1"]
    in0 ← [message "moveto 40 40, write hello\, baby!"]    # Move the 'pen' location and write a symbol in the lcd (you need a backslash before the comma!)
    in0 ← [message "clear"]    # Clear the lcd to the current background color
```

### mousing

```
Example #1 — [lcd]
  fan-in:
    in0 ← [message "clear"]
    in0 ← [p using-mouse-data]    # p using-mouse-data emits: "recordsprite, pensize 4 4, frameoval -12 -12 12 12 0 0 0, closesprite circle" | "hidesprite circle" | "$1 1000" | "$1 1300" | "$1 900"
    in0 ← [message "idle 1, local 0"]
  fan-out:
    out0 → [p using-mouse-data]:in0
    out1 → [p using-mouse-data]:in1
    out2 → [p using-mouse-data]:in2
```

```
Example #2 — [lcd]  Click and drag to see mouse position information sent from outlets one and 3
  fan-in:
    in0 ← [message "idle $1"]    # idle mouse reporting
    in0 ← [message "getsize"]    # output attribute values ('get' + attribute name)
    in0 ← [message "clear"]
  fan-out:
    out0 → [message "65 92"]:in1    # mouse position (while drawing)
    out1 → [message "70 0"]:in1    # mouse position (when idle mode is enabled)
    out2 → [toggle]:in0    # Is the mouse down?
    out3 → [message "size 200 170"]:in1    # attribute queries and update messages
```

### position and line

```
Example — [lcd]
  fan-in:
    in0 ← [message "line $1 $2"]    # draw line relative to current position
    in0 ← [message "moveto $1 $2"]    # set pen position
    in0 ← [message "move $1 $2"]
    in0 ← [pak setpixel 0 0 255 0 0]    # Set the RGB color for a single pixel
    in0 ← [message "lineto $1 $2"]    # draw line and move pen
    in0 ← [pak getpixel 0 0]    # get rgb value of pixel
    in0 ← [message "getpenloc"]    # output pen location
    in0 ← [message "clear"]    # Clear the lcd to the current background color
  fan-out:
    out3 → [print @popup 1]:in0
```

### more shapes

```
Example — [lcd]
  fan-in:
    in0 ← [message "framepoly 70 110 100 10 130 110 50 20 160 80 40 70 150 30 70 110"]    # Draw a polygon (use foreground color)
    in0 ← [message "frameroundrect 110 20 190 100 40 40 0 102 102"]    # Draw an rounded rectangle with a specified RGB color
    in0 ← [message "framearc 60 60 160 160 135 90"]    # Draw an arc (use foreground color)
    in0 ← [message "paintarc 10 10 100 100 45 180 255 0 255"]    # Draw a filled arc with a specified RGB color / a list of six numbers defines 4 coordinates for the circle/oval that containst the arc (left, top, right, bottom), followed by the start angle and degrees of arc. An optional additional three ints can be added to specify a color in RGB format
    in0 ← [message "paintroundrect 5 5 25 160 10 10"]    # a list of six numbers defines 4 coordinates for the rectangle (left, top, right, bottom), followed by the width and height of the corner radii. An optional additional three ints can be added to specify a color in RGB format / Draw a filled rounded rectangle (use foreground color)
    in0 ← [message "reset"]    # Reset to the default (initial) state
    in0 ← [message "paintpoly 30 10 110 30 110 160 30 10"]    # Draw a filled polygon (use foreground color)
    in0 ← [message "clear"]    # Clear the lcd to the current background color
```

### basic

> Click on the display to position the pen cursor at the point where you click and drag to draw freehand in the lcd pane.

```
Example — [lcd]
  fan-in:
    in0 ← [message "frameoval 180 20 40 80 255 0 255"]    # Draw an circle/oval with a specified RGB color
    in0 ← [message "framerect 40 80 80 120"]
    in0 ← [message "paintrect 60 60 90 90 204 0 0"]    # Draw a filled rectangle with a specified RGB color
    in0 ← [message "paintoval 10 20 30 120"]    # Draw a filled circle/oval (use foreground color) / The lists describe startX, startY, endX and endY coordinates for a line or shape. An optional additional three ints can be added to specify a color in RGB format
    in0 ← [message "linesegment 5 5 150 50"]    # Draw a line segment (use foreground color)
    in0 ← [message "brgb 204 255 255"]    # Set the background color in RGB format (Click clear to see the new background)
    in0 ← [message "frgb 51 0 51"]    # Set the foreground (pen) color in RGB format
    in0 ← [message "reset"]    # Reset to the default (initial) state
    in0 ← [message "pensize $1 $1"]    # Change the pen size
    in0 ← [message "clear"]    # Clear the lcd to the current background color
```

## See also

`mousestate`, `panel`
