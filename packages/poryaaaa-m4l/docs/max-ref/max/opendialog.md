# opendialog

_max · Files_

> Open a dialog to ask for a file or folder

Use the opendialog object to select a file of a specific type or folder from a standard dialog window. To choose a folder, use the "fold" type. opendialog reports the entire pathname of the file or folder chosen, which can be passed to any Max object after the word read or load.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | bang Shows Dialog, symbol(s) Set File Types |
| out0 | Complete Path of Chosen File |
| out1 | bang When User Cancels |

## Arguments

- **folder** (`symbol`) _(optional)_ — Sets opendialog to choose folders only
- **soundfile** (`symbol`) _(optional)_ — Sets opendialog to choose audio files only
  Use this argument to list audio files (AIFF, NeXT/Sun, and WAV, along with some generic data file types). Jitter Appendix A lists all the files that can be opened.
- **file-types** (`symbol`) _(optional)_ — Set the list of file types
  One or more symbols set the list of file types or file extensions (beginning with '.') that determine which files are listed by the opendialog object.

## Messages

- `bang` — Opens a dialog window
  Opens a standard Open Document dialog window for choosing a file or folder.
- `anything(filetype: list)` — Sets file type list and opens a dialog window
  One or more symbols are interpreted as one or more type codes used to determine which files are listed by the opendialog object. Example type codes for files are TEXT for text files, maxb for Max binary format patcher files, .maxpat for modern Max patcher files, and AIFF for AIFF format audio files. types with no arguments makes the object accept all file types, which is the default setting.
- `path([filepath: symbol])` — Opens a dialog to the specified path
  Opens a a standard dialog window to the specified path, if valid, and caches the path for any subsequent opening via the bang message. Omitting the filepath argument unsets any path previously set.
- `set(filetype: list)` — Sets file type list without opening a dialog window
  The word set, followed by one or more four-letter filetype codes or file extensions (beginning with '.') sets the opendialog object to search for the specified file type(s) or extension(s) without opening a dialog window. Example type codes for files are TEXT for text files, maxb for Max binary format patcher files, .maxpat for modern Max patcher files, and AIFF for AIFF format audio files. set with no arguments makes the object accept all file types, which is the default setting.
- `setpath([filepath: symbol])` — Specifies a file path to use when the dialog is opened
  Specifies a file path to use when the standard dialog window is opened with the bang message, if the path is valid. Omitting the filepath argument unsets any path previously set.
- `sound` — Sets opendialog to list audio files and opens a dialog
  Sets opendialog to list audio files (AIFF, NeXT/Sun, and WAV, along with some generic data file types) and opens a standard dialog window. Jitter Appendix A lists all the files that can be opened.
- `types(filetype: list)` — Sets file type list and opens a dialog window
  The word types, followed by one or more four-letter filetype codes sets the opendialog object to search for the specified file type(s) or extension(s) and opens a standard dialog window. Example type codes for files are TEXT for text files, maxb for Max binary format patcher files, .maxpat for modern Max patcher files, and AIFF for AIFF format audio files. types with no arguments makes the object accept all file types, which is the default setting.

## Help patcher examples

### file types

```
Example #1 — [opendialog]
  fan-in:
    in0 ← [umenu]    # choose file type and open dialog
  fan-out:
    out0 → [message ""]:in1
```

```
Example #2 — [opendialog sound]
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [message ""]:in1
    out1 → [button]:in0    # 'sound' looks for standard sound file types
```

```
Example #3 — [opendialog]
  fan-in:
    in0 ← [message "set MooV"]
    in0 ← [message "set AIFF"]    # set types without opening dialog
    in0 ← [message "set"]    # choose any file type
    in0 ← [button]    # bang to open dialog
  fan-out:
    out0 → [message ""]:in1
```

### basic

```
Example #1 — [opendialog fold]  specify 'fold' type to choose a folder
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [message ""]:in1    # outputs the absolute path of the chosen folder
    out1 → [button]:in0
```

```
Example #2 — [opendialog]
  fan-in:
    in0 ← [button]    # bang to open dialog
  fan-out:
    out0 → [message ""]:in1    # outputs the absolute path name of chosen file
    out1 → [button]:in0    # bang reports when user chooses 'cancel' from dialog
```

## See also

`conformpath`, `dialog`, `dropfile`, `date`, `filedate`, `filein`, `filepath`, `folder`, `savedialog`, `strippath`
