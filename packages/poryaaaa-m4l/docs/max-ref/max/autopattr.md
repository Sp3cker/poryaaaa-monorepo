# autopattr

_max · Data_

> Expose multiple objects to the pattr system

Causes multiple objects within a patcher to be automatically included in the pattr system.

 Note: you should use only one instance of an autopattr object per level in a patch.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | messages or data in |
| out0 | include connection |
| out1 | exclude connection |
| out2 | passout |
| out3 | dumpout |

## Arguments

- **name** (`symbol`) _(optional)_ — Object name
  A symbol argument can be used to set the autopattr object's name. In the absence of an argument, the autopattr object is given an arbitrary, semi-random name, such as u197000004.

## Messages

- `bang` — Message passed through
  bang is passed through the autopattr object and output from its center right outlet.
- `int(input: int)` — Message passed through
  An int is passed through the autopattr object and output from its center right outlet.
- `float(input: float)` — Message passed through
  float is passed through the autopattr object and output from its center right outlet.
- `list(input: list)` — Message passed through
  list is passed through the autopattr object and output from its center right outlet.
- `anything(input/arguments: list)` — Analyze message, pass through non-functions
  Any message is analyzed. If the first element of the message matches the name of an object maintained by the autopattr, the subsequent arguments in the message set that object's value. If the first element of the message matches get (name), where (name) matches the name of an object maintained by the autopattr, the value of that object is sent from the autopattr object's right outlet, prepended by the object's name. Otherwise, the message is passed through the autopattr object and output from its center right outlet.
- `getattributes` — Report accessed objects
  Causes a list of all objects names maintained by the autopattr object to be output from the right outlet, prepended by the symbol attributes.
- `getstate` — Report accessed object values
  Causes a series of lists to be output from the autopattr object's right outlet, one for every object maintained by the autopattr. Each list begins with the name of the object, and is followed by the object's current value.

## Attributes

- `@autoname` (int) — Auto-name state
  The word autoname, followed by a 1 or 0, enables or disables the autopattr object's autoname state. The default is 0 (off). When enabled, the autopattr object will automatically name any unnamed objects in the patcher supported by the pattr system and bind to them, if possible. Naming only occurs when the patcher loads, when the autopattr object is again instantiated, or when the autopattr object receives the message autoname 1. New objects placed in a patcher after the autopattr has been instantiated will not be autonamed until one of these conditions is met.
- `@autorestore` (int) — Auto-restore state
  The word autorestore, followed by a 1 or 0, enables or disables the autopattr object's autorestore state. The default is 1 (on). When enabled, the autopattr object will automatically output its last-saved values when the patcher is loaded, and distribute them to bound objects. Values are saved whenever the patcher is saved.
- `@dirty` (int) — Patcher dirty flag
  The word dirty, followed by a 1 or 0, enables or disables the patcher-dirty flag. The default is 0 (disabled). When enabled, the autopattr object will dirty the patch whenever its state changes.
- `@greedy` (int) — Greedy mode
  The word greedy, followed by a 1 or 0, enables or disables the attribute-gathering feature of the autopattr object. The default is 0 (disabled). When enabled, any internal attributes of objects attached to the left outlet of the autopattr object will be exposed to the pattr system (as well as the normal value, if present).
- `@name` (symbol) — Object name
  The word name, followed by a symbol, sets the autopattr object's patcher name.

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `day` — seen as: `day $1`
- `getday` — seen as: `getday`
- `gethour` — seen as: `gethour`
- `getmonth` — seen as: `getmonth`
- `getyear` — seen as: `getyear`
- `month` — seen as: `month $1`
- `year` — seen as: `year $1`

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — include connection

### snapshots

```
Example — [autopattr]  autopattr will gather the amxd~ parameters as a dictionary
  (no patch cords)
```

### autoname

> If the autoname attribute is 1, then all UI-objects in this patch will be named when the patch is loaded.

```
Example — [autopattr @autoname 1]  default is 0.
  (no patch cords)
```

### basic

> If a UI-object has a Scripting Name, then this autopattr will include the object in this patch's attribute (pattr) system.

```
Example — [autopattr]  include
  fan-in:
    in0 ← [message "year $1"]
    in0 ← [message "getyear"]
    in0 ← [message "getmonth"]
    in0 ← [message "month $1"]
    in0 ← [message "getday"]
    in0 ← [message "day $1"]    # send values to named objects
    in0 ← [message "gethour"]    # query non-existant name
  fan-out:
    out1 → [number]:in0    # exclude
    out2 → [print passout @popup 1]:in0    # passout
    out3 → [print dumpout @popup 1]:in0    # autopattr includes the functionality of a pattrhub and pattr objects. / dumpout
```

## See also

`pattr`, `pattrforward`, `pattrhub`, `pattrmarker`, `pattrstorage`
