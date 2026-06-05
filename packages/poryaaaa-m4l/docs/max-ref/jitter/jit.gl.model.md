# jit.gl.model

_jit · Jitter OpenGL_

> Read and draw various 3D model formats

jit.gl.model Reads and draws a variety of 3D model formats, such as OBJ, Collada, and Blender. Only tessellated polygons are drawn, and surfaces that are not tessellated are converted before drawing. Certain model formats, such as Collada, support skinned animation.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | — | messages to this 3d object |
| out0 | disabled | matrix output if enabled |
| out1 | disabled | dumpout |

## Messages

- `animenable(anim-index (optional): int, enable: int)` — Enable model animation
  Enable a model animation. If two arguments are provided, the first is the animation index to enable. Otherwise the current animation is used.
- `animloop(anim-index (optional): int, loop-enable: int)` — Enable animation looping
  Enable/disable looping on an animation. Looping on animations is enabled by default. If two arguments are provided, the first is the animation index to enable. Otherwise the current animation is used.
- `animlooppoints(anim-index (optional): int, loop-start: float, loop-stop: float)` — Set animation loop points
  Set the loop points of an animation, if looping is currently enabled on that animation. If two arguments are provided, the first is the animation index to enable. Otherwise the current animation is used.
- `animrate(anim-index (optional): int, rate: float)` — Animation rate
  Set an animation's rate. If two arguments are provided, the first is the animation index, otherwise the current animation is used.
- `animreset(anim-index (optional): int)` — Reset an animation
  Reset the state of an animation. Time is set to 0., rate to 1., weight to 1., and the loop points are set to the beginning and end of the animation. If two arguments are provided, the first is the animation index to enable. Otherwise the current animation is used.
- `animtime(anim-index (optional): int, time: float)` — Jump to animation time
  Jump to a specific time, in seconds, of an animation. If two arguments are provided, the first is the animation index, otherwise the current animation is used.
- `animweight(anim-index (optional): int, weight: float)` — Set animation weight
  Set an animation's weight which determines how much influence the animation has on the affected mesh. If two arguments are provided, the first is the animation index, otherwise the current animation is used.
- `copynodestoclipboard` — Copy all nodes to the clipboard
  Copy all nodes in the model as jit.anim.node objects, to the clipboard. When the nodes are pasted in the patch, they will control the internal nodes of the model. Make sure the name attribute is set before using this feature.
- `dispose` — Unload the model file and free all resources
- `getanim_dict(anim-index: int)` — Send animation description as dictionary
  Send a description of the animation at anim-index as a dictionary out the dumpout.
- `getanimnames` — Report animation names
  Sends a list of the scenes named animations out the dumpout. Some model files don't support named animations, and therefore the names will not be sent, even though animations are present.
- `getbonenames` — Report available bones
  Sends a list of the bones currently loaded in the model, if any, out the dumpout.
- `getmaterial_dict(drawgroup (optional): int)` — Send material description as dictionary
  Send a description of the drawgroup material as a dictionary out the dumpout. If no argument, use the current drawgroup attribute.
- `getnodenames` — Report available scene nodes
  Sends a list of the scene nodes currently loaded in the model out the dumpout.
- `gettexnames` — Report available textures
  Sends a list of all the textures currently loaded in the model out the dumpout.
- `nodeanimenable(model node name: symbol, anim-enable: int)` — Toggle a node animation
  Toggle the animation of the named node. If disabled, model animations will have no effect on that node.
- `nodebind(model node name: symbol, jit.anim.node name: symbol)` — Bind a node to a jit.anim.node
  Takes two args, the name of the node in the model to bind, and the name of an jit.anim.node object to bind to.
- `nodereset(model node name: symbol)` — Reset a node's transform
  Resets the named node in the model to it's initial spatial transform state.
- `nodesetinitial(model node name: symbol)` — Set the transform state
  Sets the named nodes initial transform state to it's current transform.
- `read` — Load a model file from disk
  Loads an model file from disk. The read message will attempt to find the model file in the Max search path and load it. If no file name is specified, a file dialog box is presented.
- `sendmaterial(drawgroup: int, message: symbol, [values: list])` — Send a material a message
  Send the internal material for drawgroup a message. See jit.gl.material for possible messages and attributes.
- `sendtexture(texture name: symbol, message: symbol, [values: list])` — Send a named texture a message
  Send the internal named texture a message. See jit.gl.texture for possible messages and attributes.
- `texgroup(group-number: int, texture-name: symbol)` — Apply a named texture to a mesh group
  The texgroup message is used to apply a named texture to a specific mesh within the model. The model's mesh-groups are specified in its model file. The texture applied to a group will override any textures which have been applied to the object using the texture attribute, and textures loaded from the model file.

## GUI behaviors

- `(drag)` — Drag and drop model files

## Attributes

- `@animblendmode` (int) — Animation blend mode
  Determine how multiple enabled animations will be blended (default = 0 - Average).
- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### drawing

Attributes demonstrated: `@xfade`

### basic

```
Example — [jit.gl.model mod-ctx @file duck.dae]
  fan-in:
    in0 ← [message "read"]
    in0 ← [r to-model]
    in0 ← [message "read duck.dae"]
    in0 ← [attrui @lighting_enable]
    in0 ← [attrui @smooth_shading]
    in0 ← [attrui @normalize]
    in0 ← [attrui @drawgroup]
    in0 ← [attrui @material_mode]
    in0 ← [message "dispose"]
  fan-out:
    out0 → [s matrixout]:in0
    out1 → [s from-model]:in0
```

Attributes demonstrated: `@drawgroup`, `@lighting_enable`, `@material_mode`, `@normalize`, `@smooth_shading`

## See also

`jit.gl.mesh`, `jit.gl.multiple`, `jit.gl.node`, `jit.gl.render`, `jit.gl.texture`, `jit.gl.material`, `jit.gl.camera`
