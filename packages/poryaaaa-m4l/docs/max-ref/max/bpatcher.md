# bpatcher

_max · U/I_

> Embed a subpatch with a visible UI

The bpatcher object holds the contents of a patcher or subpatcher, displaying only those visual elements that fall within its box rectangle. The number of inlets and outlets in a bpatcher object is determined by the number of inlet and outlet objects contained in its contained subpatcher.

## Messages

- `replace(filename: symbol)` — Replace a contained patcher
  The replace message is intended for use in conjunction with scripting messages to the thispatcher object. If you send a replace message via an inlet to the bpatcher object, it will only work if there is no patcher inside the bpatcher; the inlet to the bpatcher object is used for sending messages into its subpatcher, not for sending messages to the bpatcher object itself.
  To use the replace message feature via scripting messages, give your bpatcher object a Scripting Name using the Inspector, then use the message script sendbox replace to send the message to the named bpatcher object.

## GUI behaviors

- `(drag)` — Load a patcher
  When a patcher file is dragged from the Max File Browser to a bpatcher object, the file will be loaded.
- `(mouse)` — Route gesture to contained patcher
  When the window containing the bpatcher object is locked (or the Command key on Macintosh or Control key on Windows is held down) and the mouse is clicked inside the bpatcher object’s box, the gesture is handled by the patch inside the box.
  If the Shift and Command keys on Macintosh or Shift and Control keys on Windows are held down while clicking on a bpatcher object, dragging the mouse moves the upper-left corner of the visible part of the patch inside the box. The Assistance area of the patcher window shows the pixel values of the offset. If Enable Drag-Scrolling is unchecked in the bpatcher Inspector window, this feature is disabled.
  If the Command and Option keys on Macintosh or Control and Alt keys on Windows are held down while clicking in a bpatcher object, a pop-up menu allows you to open the original file of the patch contained inside the box in its own window, or change the patch currently contained inside the box in its own window.

## Attributes

- `@category` (atom)
- `@default` (atom, size 0)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `offset` — seen as: `offset 0 -124`, `offset 0 -19`, `offset 0 -228`

## Help patcher examples

### Presentation Mode

> If the patcher embedded in your bpatcher is set to open in Presentation Mode, the bpatcher window will display its contents in Presentation Mode
>
> normal patch
>
> 1. Open the patch embedded in your bpatcher
>
> 2. Choose "Inspector Window" from the View menu to display the Inspector
>
> 3. Select "Open in Presentation" and save the file.
>
> 4. If you close and reopen the parent patch, you should see the embedded patch in Presentation mode.

```
Example #1 — [bpatcher]  patch in Presentation Mode
  (no patch cords)
```

```
Example #2 — [bpatcher]
  (no patch cords)
```

### using offsets

> Using bpatcher for switchable user interfaces:
>
> the example used here is called BPswitch
>
> 1. Create a patcher with an inlet object connected to a thispatcher object. Make these two objects and their patch cord hidden or move them out of the way of your interfaces.
>
> 2. Create alternative user interfaces in different areas of the patcher. Save the patcher as a file.
>
> 3. Create a bpatcher and load the patcher you saved into it. Then hold down the control and shift keys (commmand and shift on Mac) while dragging in the bpatcher to find the offsets corresponding to each user interface area. Read the offsets in the inspector.
>
> 4. Make "offset" messages with these numbers and connect the boxes to the bpatcher's inlet.

> This patch is in Presentation Mode. Click on the tabs and watch how the display changes. Click on the Patching Mode button in the toolbar to turn Presentation Mode off and you'll see a little bit more about how to make tabs like that.

```
Example — [bpatcher]
  fan-in:
    in0 ← [message "offset 0 -228"]
    in0 ← [message "offset 0 -124"]
    in0 ← [message "offset 0 -19"]
```

### basic

> This is a file called BPflip--you can use its number boxes, too
>
> The patcher file shown in the bpatcher window needs to be in your search path to be displayed. As an alternative, you can embed a subpatcher using the bpatcher Inspector. Select a bpatcher and click on the Inspector icon in the patcher window toolbar to show the Inspector, and check "Embed Patcher with Parent."
>
> A patcher contained in a bpatcher will automatically update if a new version of the original is saved. You can choose a new patcher file for a box by using the bpatcher Inspector. Select a bpatcher and click on the Inspector icon in the patcher window toolbar to show the Inspector.

```
Example — [bpatcher]  input / output
  fan-in:
    in0 ← [slider]
  fan-out:
    out0 → [slider]:in0
```

## See also

`search_path`, `patcher`, `patcherargs`, `pcontrol`, `thispatcher`
