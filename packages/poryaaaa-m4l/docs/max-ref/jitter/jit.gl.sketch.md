# jit.gl.sketch

_jit · Jitter OpenGL_

> Use drawing commands with OpenGL

Records and draws based on 3-D drawing commands. These commands range from simple turtle graphics to the majority of the OpenGL API.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages to this 3d object |
| out0 | matrix output if enabled |
| out1 | dumpout |

## Messages

- `beginstroke` — Begin stroke definition
  Begin definition of a stroked path of the style specified by a following stroke_style argument. Currently supported stroke styles are "
  basic2d
  line
- `circle(radius: float, [theta-start: float], [theta-end: float])` — Draw a circle
  Draws a circle with radius specified by radius and center point at the current drawing position. If theta-start and theta-end are specified, then an arc will be drawn instead of a full circle. The theta-start and theta-end arguments are in terms of degrees (0-360). The current shapeorient and shapeslice values will also affect the drawing.
- `cmd_delete(index: int, command: symbol)` — Delete commands from the command list
  Deletes index or all instances of the command specified by the command argument from the command list.
- `cmd_enable(index: int, command: symbol, 0/1: int)` — Enable/disable a command
  Enables/disables command in the command list.
- `cmd_insert(index: int, command: symbol, command-args: variable)` — Insert a command into the command list
  Inserts command at position index in the command list.
- `cmd_replace(index: int, command: symbol, command-args: variable)` — Replace a command in the command list
  Replaces position index in the command list with command.
- `cube(scale-x: float, [scale-y: float], [scale-z: float])` — Draw a box
  Draws a box with width 2* scale-x, height 2* scale-y, depth 2* scale-z, and center point at the current drawing position. If scale-y and scale-z are not specified, they will assume the same value as scale-x. The current shapeorient, shapeslice, and shapeprim values will also affect the drawing.
- `cylinder(radius1: float, radius2: float, mag: float, [theta-start: float], [theta-end: float])` — Draw a cylinder
  Draws a cylinder with top radius specified by radius1, bottom radius specified by radius2, length specified by mag, and center point at the current drawing position. If theta-start and theta-end are specified, then a patch will be drawn instead of a full cylinder. The theta-start and theta-end arguments are in terms of degrees (0-360). The current shapeorient, shapeslice, and shapeprim values will also affect the drawing.
- `drawmatrix` — Draw a matrix
  Draws a matrix. The format of the message is
  drawmatrix
  texflag values are specified as follows: 0=use, 1=ignore, 2=auto
  normalflag values are specified as follows: 0=use, 1=ignore, 2=auto
  colorflag values are specified as follows: 0=use, 1=ignore
  edgeflag values are specified as follows: 0=use, 1=ignore
- `drawobject` — Draw an object
  The word drawobject, followed by a symbol that specifies an object name and a 0/1 flag that specifies whether or not to ignore the OB3D attributes, draws a named jit.gl object.
- `ellipse(radius1: float, radius2: float, [theta-start: float], [theta-end: float])` — Draw an ellipse
  Draws the ellipse specified by radius1, radius2 and center point at the current drawing position. If theta-start and theta-end are specified, then an arc will be drawn instead of a full ellipse. The theta-start and theta-end arguments are in terms of degrees (0-360). The current shapeorient and shapeslice values will also affect the drawing.
- `endstroke` — End a stroked path definition
  Ends the definition of a stroked path and renders the path.
- `framecircle(radius: float, [theta-start: float], [theta-end: float])` — Draw a circle or arc frame
  Draws the circumference of the circle with radius specified by radius and center point at the current drawing position. If theta-start and theta-end are specified, then an arc will be drawn instead of a full circle. The theta-start and theta-end arguments are in terms of degrees (0-360). The current shapeorient and shapeslice values will also affect the drawing.
- `frameellipse(radius1: float, radius2: float, [theta-start: float], [theta-end: float])` — Draw an ellipse frame
  Draws the circumference of the ellipse specified by radius1, radius2 and center point at the current drawing position. If theta-start and theta-end are specified, then an arc will be drawn instead of a full ellipse. The theta-start and theta-end arguments are in terms of degrees (0-360). The current shapeorient and shapeslice values will also affect the drawing.
- `framequad(x1: float, y1: float, z1: float, x2: float, y2: float, z2: float, x3: float, y3: float, z3: float, x4: float, y4: float, z4: float)` — Draw a quadrilateral frame
  Draws the frame of the quadrilateral specified by the four points x1 y1 z1 x2 y2 z2 x3 y3 z3 x4 y4 z4.
- `frametri(x1: float, y1: float, z1: float, x2: float, y2: float, z2: float, x3: float, y3: float, z3: float)` — Draw a triangle frame
  Draws the frame of the triangle specified by the three points x1 y1 z1 x2 y2 z2 x3 y3 z3.
- `getcamera` — Report the camera location
  Returns the x, y, and z coordinates of the camera location.
- `getcmd_index(index: int)` — Get a command list item
  Sends the command list item at index as a message out the object's right outlet.
- `getcmdlist` — Report the command list
  Sends the command list as a series of messages out the object's right outlet. The command list is bracketed by max messages that indicate the beginning and the of the current command list, and the command list is output between these two lines, one line per command. The output takes the form:
  cmdlist_begin index
  cmdlist index command-name command-arg1... command-argN
  cmdlist index....
  cmdlist_end
- `glbegin(draw-prim: symbol)` — Begin a drawing command list
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glcolor(near: float, far: float)` — Set the current drawing color
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glcullface(face-name: symbol, face-number: int)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `gldisable(capability: symbol)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glenable(capability: symbol)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glend` — Complete the drawing command list
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glget` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `gllinestipple(factor: int, bit-pattern: int)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `gllinewidth(width: float)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glnormal(x: float, y: float, z: float)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glpointsize(size: float)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glpolygonmode(face-name: symbol, face-number: int, mode-name: symbol, mode-number: int)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glpolygonoffset(factor: float, units: float)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glpopmatrix` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glpushmatrix` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glrect(x1: float, y1: float, x2: float, y2: float)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glrotate(angle: float, x: float, y: float, z: float)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glscale(x-scale: float, y-scale: float, z-scale: float)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glshademodel(model-name: symbol, model-number: int)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more info.
- `gltexcoord(s: float, t: float)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `gltranslate(delta-x: float, delta-y: float, delta-z: float)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `glvertex(x: float, y: float, z: float)` — OpenGL function
  Please see the OpenGL "Red Book" or "Blue Book" for more information.
- `line(delta-x: float, delta-y: float, delta-z: float)` — Draw a line relative to the current position
  Draws a line from the current drawing position to the location delta-x delta-y delta-z relative to the current drawing position.
- `linesegment(x1: float, y1: float, z1: float, x2: float, y2: float, z2: float)` — Draw a line segment
  Draws a line from the location specified by x1 y1 z1 to the location specified by x2 y2 z2.
- `lineto(x: float, y: float, z: float)` — Draw a line from the current position
  Draws a line from the current drawing position to the location specified by x y z.
- `move(delta-x: float, delta-y: float, delta-z: float)` — Move to a relative position
  Moves the drawing position delta-x delta-y delta-z relative to the current drawing position.
- `moveto(x: float, y: float, z: float)` — Move to an absolute position
  Moves the drawing position to the location specified by x y z.
- `plane(scale-x1: float, [scale-y1: float], [scale-x2: float], [scale-y2: float])` — Draw a plane
  Draws a plane with top width 2* scale-x1, left height 2* scale-y1, bottom width 2* scale-x2, right height 2* scale-y2, and center point at the current drawing position. If scale-y1 is not specified, it will assume the same value as scale-x1. If scale-x2 and scale-y2 are not specified, they will assume the same values as scale-x1 and scale-y1 respectively. The current shapeorient, shapeslice, and shapeprim values will also affect the drawing.
- `point(x: float, y: float, z: float)` — Draw a point
  Draws a point at the location specified by x y z.
- `quad(x1: float, y1: float, z1: float, x2: float, y2: float, z2: float, x3: float, y3: float, z3: float, x4: float, y4: float, z4: float)` — Draw a quadrilateral
  Draws the quadrilateral specified by the four points x1 y1 z1 x2 y2 z2 x3 y3 z3 x4 y4 z4.
- `reset` — Delete command list elements
  Delete all elements of the command list.
- `roundedplane` — Draw a rounded plane
  The message roundedplane round_amount scale_x scale_y draws a rounded plane with width 2 * scale_x, and height 2 * scale_y and center point at the current drawing position. The size of the rounded portion of the plane is determined by the round_amount argument. If scale_y is not specified, it will assume the same value as scale_x.
- `screentoworld` — Convert screen coordinates to world coordinates
  The word screentoworld, followed by a pair of numbers that specify x and y coordinates, returns an array containing the x, y, and z world coordinates associated with a given screen pixel using the same the depth from the camera as 0,0,0. Optionally a third depth arg may be specified, which may be useful for hit detection and other applications. The depth value is typically specified in the range 0.-1. where 0 is the near clipping plane, and 1. is the far clipping plane.
- `shapeorient(rot-x: float, rot-y: float, rot-z: float)` — Set a shape orientation
  Sets the current orientation for shape drawing commands (circle, framecircle, ellipse, frameellipse, sphere, cylinder, torus, cube, and plane). The rot-x, rot-y, and rot-z arguments are in terms of degrees (0-360). The order in which the orientation is applied is first rotate about x axis rot-x, then rotate about y axis rot-y, and finally rotate about z axis rot-z.
- `shapeprim(draw-prim: symbol)` — Set a drawing primitive
  Sets the current drawing primitive for shape drawing commands (circle, framecircle, ellipse, frameellipse, sphere, cylinder, torus, cube, and plane). Valid values for draw-prim are:
  lines
  line_loop
  line_strip
  points
  polygon
  quads
  quad_grid
  quad_strip
  triangles
  tri_grid
  tri_fan
  tri_strip
- `shapeslice(slice-a: int, slice-b: int)` — Set the decimation level
  Sets the current level of decimation (resolution) for shape drawing commands (circle, framecircle, ellipse, frameellipse, sphere, cylinder, torus, cube, and plane).
- `sphere(radius: float, [theta1-start: float], [theta1-end: float], [theta2-start: float], [theta2-end: float])` — Draw a sphere
  Draws a sphere with radius specified by radius and center point at the current drawing position. If theta1-start, theta1-end, theta2-start, and theta2-end are specified, then a patch will be drawn instead of a full sphere. The theta1-start, theta1-end, theta2-start, and theta2-end arguments are in terms of degrees (0-360). The current shapeorient, shapeslice, and shapeprim values will also affect the drawing.
- `strokeparam` — Set a stroked path parameter
  The word strokeparam, followed by a parameter name argument and a list of parameter values, sets the current value of the parameter specified by the parameter_name argument to be the value specified by parameter_value argument(s). Some parameters are global for the extent of a stroked path definition, while others may vary on a point by point basis.
- `strokepoint` — Define a stroked path anchor point
  The word strokepoint, followed by three numbers that specify x, y, z coordinates, defines an anchor point at the location specified by those coordinates. Some stroke styles (such as basic2d) will ignore the z coordinate.
- `torus(radius1: float, radius2: float, [theta1-start: float], [theta1-end: float], [theta2-start: float], [theta2-end: float])` — Draw a torus
  Draws a torus with major radius specified by radius1, minor radius specified by radius1, and center point at the current drawing position. If theta1-start, theta1-end, theta2-start, and theta2-end are specified, then a patch will be drawn instead of a full torus. The theta1-start, theta1-end, theta2-start, and theta2-end arguments are in terms of degrees (0-360). The current shapeorient, shapeslice, and shapeprim values will also affect the drawing.
- `tri(x1: float, y1: float, z1: float, x2: float, y2: float, z2: float, x3: float, y3: float, z3: float)` — Draw a triangle
  Draws the triangle specified by the three points x1 y1 z1 x2 y2 z2 x3 y3 z3.
- `worldtoscreen` — Convert world coordinates to screen coordinates
  The word worldtoscreen, followed by three numbers that specify x, y, z coordinates, returns an array containing the x, y, and depth screen coordinates associated with a given world coordinate. The depth value is typically specified in the range 0.-1. where 0 is the near clipping plane, and 1. is the far clipping plane.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — matrix output if enabled

### advanced

> Advanced:
>
> cmdlist
>
> Using messages to jit.gl.sketch, you can edit the current drawing command list. To see the current list, use the "getcmdlist" message.
>
> object commands
>
> The jit.gl.sketch object can also be used to draw other Jitter OpenGL objects as part of a drawing command list.
>
> drawobject <objectname> <ignore ob3d attributes>;

Attributes demonstrated: `@lighting_enable`, `@shape`

### drawing

> move <x> <y> <z> moveto <x> <y> <z> line <x> <y> <z> lineto <x> <y> <z> linesegment <x1> <y1> <z1> <x2> <y2> <z2> tri <x1> <y1> <z1> <x2> <y2> <z2> <x3> <y3> <z3> frametri <x1> <y1> <z1> <x2> <y2> <z2> <x3> <y3> <z3> quad <x1> <y1> <z1> <x2> <y2> <z2> <x3> <y3> <z3> <x4> <y4> <z4> framequad <x1> <y1> <z1> <x2> <y2> <z2> <x3> <y3> <z3> <x4> <y4> <z4>

### glu-wrapper

> glulookat <eye x> <eye y> <eye z> <lookat x> <lookat y> <lookat z> <up x> <up y> <up z>; gluortho2d <left> <right> <bottom> <top>; gluperspective <fovy> <aspect> <near> <far>;
>
> GLU Tesselation
>
> glutessbegincontour; glutessbeginpolygon; glutessedgeflag <0/1>; glutessendcontour; glutessendpolygon; glutessmatrix <matrixname>; glutessnormal <x> <y> <z>; glutessvertex <x> <y> <z> (<red> <green> <blue> <alpha>);
>
> Tesselation Properties

### commands

> glbegin <draw prim>; glbindtexture <texture name>; glblendfunc <src-function> <dst-function>; glclear; glclearcolor <red> <green> <blue> <alpha>; glcleardepth <depth>; glclipplane <plane> <coeff1> <coeff2> <coeff3> <coeff4>; glcolor <red> <green> <blue> (<alpha>); glcolormask <red> <green> <blue> <alpha>; glcolormaterial <face> <mode>; glcullface <face>; gldepthmask <0/1>; gldepthrange <near> <far>; gldisable <capability>; gldrawpixels <matrixname> (src <x> <y> <width> <height>); gledgeflag <0/1>; glenable <capability>; glend; glfinish; glflush; glfog <parameter name> <value>; glfrustum <left> <right> <bottom> <top> <near> <far>; glhint <target> <mode>; gllight <light> <parameter name> <value>; gllightmodel <parameter name> <value>; gllinestipple <factor> <bit-pattern>; gllinewidth <width>; glloadidentity; glloadmatrix <matrixname>

### basic

```
Example — [jit.gl.sketch skch-ctx]
  fan-in:
    in0 ← [message "reset, glcolor 1. 1. 0. 1., lineto 1. 1. 0, moveto -0.5 0. 0.25, glcolor 0. 1. 0. 1., sphere 0.25"]    # send a set of drawing commands
    in0 ← [r sketchy]
    in0 ← [route text] ← [trigger clear getcmdlist l] ← [textedit]    # try different drawing commands and look at the Max window to see the command list
    in0 ← [trigger clear getcmdlist l] ← [textedit] ← [trigger clear getcmdlist l]    # try different drawing commands and look at the Max window to see the command list
    in0 ← [jit.gl.handle skch-ctx] ← [message "reset"]
    in0 ← [message "reset"]    # clear the command list
    in0 ← [attrui @lighting_enable]
  fan-out:
    out1 → [print @popup 1]:in0
```

Attributes demonstrated: `@lighting_enable`

## See also

`jit.gl.graph`, `jit.gl.gridshape`, `jit.gl.handle`, `jit.gl.isosurf`, `jit.gl.mesh`, `jit.gl.model`, `jit.gl.nurbs`, `jit.gl.plato`, `jit.gl.render`, `jit.gl.shader`, `jit.gl.slab`, `jit.gl.text2d`, `jit.gl.text3d`, `jit.gl.texture`, `jit.gl.videoplane`, `jit.gl.volume`
