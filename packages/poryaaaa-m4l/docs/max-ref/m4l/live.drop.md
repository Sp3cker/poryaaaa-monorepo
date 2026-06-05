# live.drop

_m4l · Live UI Objects_

> Define a region for dragging and dropping a file

The live.drop objects defines a region for dragging and dropping files and outputs the filepath when a file is dropped onto it.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message In (colors...) |
| out0 | Pathname of the Dropped File |
| out1 | Type Code of Dropped File |

## Messages

- `bang` — Sends the current path out the outlet.
- `clear` — Clears the currently stored filepath.
- `set(filepath-name: list)` — The word set, followed by a list that specifies a filepath, sets a filepath to be reported by the the live.drop when it is initialized.
  The word set, followed by a list that specifies a filepath, sets a filepath to be reported by the the live.drop when it is initialized. The set message allows you to store a filepath as reported by the object in the parameter and display and output the path as if a file had been manually dropped on the object.

## GUI behaviors

- `(drag)` — When a file is selected and dragged from the Live application's File Browser onto a live.drop object in a Max for Live device, the full pathname of the file is sent out the live.drop object's outlet.

 The object will also provide the full pathname for a file when its icon is dragged from the Max File Browser or your computer's Finder onto a live.drop object.

 The live.drop object can be used with encrypted and encoded samples from Live's File Browser. Encrypted samples will be decrypted, and a temporary filename will be passed from the object's outlet, which can be used by objects in MaxMSP. Note that this temporary file will become invalid 1 minute after the last object (including live.drop)
  When a file is selected and dragged from the Live application's File Browser onto a live.drop object in a Max for Live device, the full pathname of the file is sent out the live.drop object's outlet.
  The object will also provide the full pathname for a file when its icon is dragged from the Max File Browser or your computer's Finder onto a live.drop object.
  The live.drop object can be used with encrypted and encoded samples from Live's File Browser. Encrypted samples will be decrypted, and a temporary filename will be passed from the object's outlet, which can be used by objects in MaxMSP. Note that this temporary file will become invalid 1 minute after the last object (including live.drop) in MaxMSP has stopped using it.
  Encoded samples (e.g. .mp3, .flac) are handled similarly, although they are not automatically invalidated after use. Please see the decodemode attribute for more information about encoded samples.
- `(mouse)` — To drop a file, click on the name of an audio file or its icon in the Live application's file browser, drag it onto the live.drop object's display area, and release the mouse button.

 When the mouse is positioned over the live.drop object, a round button will appear in the lower right-hand portion of the object's display. The button indicates the enabled or disabled state for dragging and dropping, and is enabled by default. To toggle drag and drop behavior, click on the round button. The button's color (set using the circlecolor and circleoncolor attributes found in the object Inspector)
  To drop a file, click on the name of an audio file or its icon in the Live application's file browser, drag it onto the live.drop object's display area, and release the mouse button.
  When the mouse is positioned over the live.drop object, a round button will appear in the lower right-hand portion of the object's display. The button indicates the enabled or disabled state for dragging and dropping, and is enabled by default. To toggle drag and drop behavior, click on the round button. The button's color (set using the circlecolor and circleoncolor attributes found in the object Inspector) changes to indicate its state.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@default` (symbol)
- `@label` (symbol)
- `@order` (int)
- `@save` (int)

## Help patcher examples

### appearance

```
Example — [live.drop]
  fan-in:
    in0 ← [attrui @bordercolor]
    in0 ← [attrui @focusbordercolor]
    in0 ← [attrui @textcolor]
```

Attributes demonstrated: `@bordercolor`, `@focusbordercolor`, `@textcolor`

### basic

```
Example — [live.drop]
  fan-out:
    out0 → [message ""]:in1    # filename (full path) of the dropped file
    out1 → [message ""]:in1    # type code of the dropped file
```

## See also

`dropfile`
