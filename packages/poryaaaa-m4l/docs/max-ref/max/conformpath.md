# conformpath

_max · Files_

> Convert file paths styles

Converts paths between the older colon style formats and the current slash style. It can also be used to conform paths to either absolute, relative, boot volume relative, or Cycling 74 folder relative types.

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | symbol | (symbol) Path Input |
| out0 | symbol | (symbol) Conformed Path Output |
| out1 | int | (int) Success |

## Arguments

- **pathstyle** (`symbol`) _(optional)_ — The path style for output
  An optional symbol argument specifies the pathstyle to be used as output. The possible pathstyle arguments are:

 colon: Specifies that the colon pathstyle is used for output.

 max: Specifies that the max pathstyle is used for output.

 native: Specifies that the native pathstyle is used for output.

 native_win: Specifies that the native_win pathstyle is used for output (See description of the pathstyle method for more details).

 Note: The use of the native_win style paths is not advised except for display purposes.

 slash: Specifies that the slash pathstyle is used for output.

 (See the description of the pathstyle method for more details).
- **pathtype** (`symbol`) _(optional)_ — The path type for output
  An optional symbol argument specifies the pathtype to be used as output. The possible pathtype arguments are:

 absolute: Specifies the output of the absolute pathname of the file or folder as a symbol.

 boot: Specifies the output of the pathname of the file or folder relative to the boot volume as a symbol.

 C74: Specifies the output of the pathname of the file or folder relative to the Cycling 74 folder as a symbol.

 relative: Specifies the output of the pathname of the file or folder relative to the Max application folder as a symbol.

 ignore: Specifies that no pathtype conversion is performed.

 (See description of the pathtype method for more details).

## Messages

- `anything(filepath: symbol)` — Convert a file path
  A file name or path as a symbol. The conformpath object converts paths of one pathstyle (i.e., file paths that use colons or slashes as separators) and/or pathtype (paths that are absolute, relative, boot volume-relative, or Cycling 74 folder-relative) to another. It provides a superset of the functionality of the absolutepath and relativepath objects.
- `pathstyle(pathstyle: symbol)` — Set the path style for conversion
  The word pathstyle, followed by a word that specifies a pathstyle, will conform the output pathname to the chosen styles. The possible styles are:
  colon: The colon style will use colons as separators when passing paths between objects. This style was used in Max versions 4.2 and earlier on Macintoshes Note: Since the native Macintosh pathstyle is the same as the colon path style, there is no native_mac pathstyle.
  max: (default) The max style will use whatever style the currently running version of Max uses to pass paths between objects.
  native: The native style will use whatever format is used by the currently running operating system to specify paths. Note: When working with native paths, only absolute paths will be valid for the operating system.
  native_win: The native_win style will use native Windows OS format (i.e., backslashes as separators) to specify paths. Note: The use of the native_win style paths is not advised except for display purposes- In Max, the backslash character is used as an escape character and could lead to problems if used in conjunction with message boxes, sprintf, coll, and other objects which parse text into atoms.
  slash: The slash style will use slashes as separators when passing paths between objects.
- `pathtype(pathtype: symbol)` — Set the path type for conversion
  The word pathtype, followed by a word that specifies a pathtype, will conform the output pathname to the chosen type. The possible types are:
  absolute: The absolute type will output the absolute pathname of the file or folder as a symbol.
  boot: The boot type will output the pathname of the file or folder relative to the boot volume as a symbol. If the file is not relative to the boot file, the conformpath object will send a zero out the right outlet and send the output path out the left outlet unchanged.
  C74: The C74 type will output the pathname of the file or folder relative to the Cycling 74 folder as a symbol. If the file is not relative to the Cycling 74 folder, the conformpath object will send a zero out the right outlet and send the output path out the left outlet unchanged.
  relative: The relative type will output the pathname of the file or folder relative to the Max application folder as a symbol. If the file is not relative to the Max application folder, the conformpath object will send a zero out the right outlet and send the output path out the left outlet unchanged.
  tilde: The tilde type will attempt to output the pathname of the file or folder relative to the current user's home folder. If the file is not in the current user's home folder, the conformpath object will send a zero out the right outlet and send the output path out the left outlet unchanged.
  desktop: The desktop type will output the pathname of the file or folder relative to the current user's Desktop folder. If the file is not in the current user's Desktop folder, the conformpath object will send a zero out the right outlet and send the output path out the left outlet unchanged.
  tempfolder: The tempfolder type will output the pathname of the file or folder relative to Max's temporary folder. If the file is not in Max's temporary folder, the conformpath object will send a zero out the right outlet and send the output path out the left outlet unchanged.
  usermax: The usermax type will output the pathname of the file or folder relative to user's Max 9 folder (in the Documents folder). If the file is not in the Max 9 folder, the conformpath object will send a zero out the right outlet and send the output path out the left outlet unchanged.
  package: The package type will output the pathname of the file or folder relative to an active package (e.g. Package:/VIDDLL/patchers). If the file is not in a package, the conformpath object will send a zero out the right outlet and send the output path out the left outlet unchanged.
  plugin: The plugin type will output the pathname to a plugin file as a Max-specific plugin path (e.g. C74_VST3:/PluginName). If this is not possible, the conformpath object will send a zero out the right outlet and send the output path out the left outlet unchanged.
  ignore: (default) The ignore type will perform no path type conversion.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `"~/Library/Application` — seen as: `"~/Library/Application Support"`
- `./patches` — seen as: `./patches`
- `/` — seen as: `/`
- `:patches` — seen as: `:patches`
- `C74:/init` — seen as: `C74:/init`
- `Desktop:/my_pic.png` — seen as: `Desktop:/my_pic.png`
- `^:` — seen as: `^:`

## Help patcher examples

### basic

> in MaxMSP, the backslash character is used as an escape character and could lead to problems if used in conjunction with message boxes, sprintf, coll, and other objects which parse text into atoms. hence using native_win style paths is not advised except for display purposes.

```
Example — [conformpath max boot]
  fan-in:
    in0 ← [message "Desktop:/my_pic.png"]    # Desktop path
    in0 ← [opendialog] ← [button]
    in0 ← [prepend pathtype] ← [umenu]
    in0 ← [message "^:"]    # boot paths
    in0 ← [message "./patches"]
    in0 ← [prepend pathstyle] ← [umenu]
    in0 ← [message "/"]
    in0 ← [message "C74:/init"]    # C74 path
    in0 ← [message ":patches"]    # relative paths
    in0 ← [message ""~/Library/Application Support""]    # ~ path
    in0 ← [combine path-input ../../] ← [thispatcher] ← [message "path"]
  fan-out:
    out0 → [print path @popup 1]:in0    # path conformed according to style + type
    out1 → [button]:in0
    out1 → [toggle]:in0    # Was the path conversion successful?
```

## See also

`absolutepath`, `opendialog`, `relativepath`, `savedialog`, `strippath`
