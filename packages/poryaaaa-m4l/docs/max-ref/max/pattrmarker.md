# pattrmarker

_max · Data_

> Provide pattr communication between patchers

The pattrmarker object associates a patcher with a global name, which can be used when looking up named objects. This permits, among other conveniences, name lookup and communication between two or more independent patcher hierarchies.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | set name |
| out0 | dumpout |

## Arguments

- **name** (`symbol`) — Global access name
  The name for the parent patcher of the pattrmarker object in the pattr global namespace.

## Messages

- `getmarkerlist([all: int])` — Output all global pattr names
  Reports a list of all registered names in the pattr global namespace from the pattrmarker object's outlet, prepended by the word markerlist. These names are reported whether they were registered via pattrmarker objects or via the globalpatchername attribute of the patcher object. With no argument, or an argument of 0, the names of pattrmarker objects with the invisible attribute enabled will not appear in the reported list. With an argument of 1, all names, regardless of the invisible status, will be reported.
- `reveal(name: symbol)` — Open a patcher view for a global name
  Opens a view of the patcher referred to by the global name argument.

## Attributes

- `@invisible` (int) — Suppress visibility
  When enabled, the name of this pattrmarker object will not appear in the list reported by the getmarkerlist message.
- `@name` (symbol) — Parent patcher name
  The global name of the parent patcher of the pattrmarker object.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — dumpout
> - `in0` — set name

### basic

> A pattrmaker object in this patcher means that this patcher is at the root level of the global pattr hierarchy. Any patcher with a "pattrmarker <name>" has a global name of "::<name>".

```
Example — [pattrmarker biggy]
  (no patch cords)
```

## See also

`autopattr`, `pattr`, `pattrforward`, `pattrhub`, `pattrstorage`
