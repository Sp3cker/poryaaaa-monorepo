# dict.view

_max · U/I_

> View the contents of a dictionary

Use the dict.view object to view the contents of a dictionary.

## Messages

- `bang` — Refresh the display
- `dictionary(name: symbol)` — Display a dictionary
- `expand(level: int)` — Set expand level
  The word expand, when followed by a number, sets the display of dict.view to show the specified level of hierarchy. expand 0 will collapse all levels; expand 1 will expand only the first level, etc.
  When expand is followed by the name of a key associated with a sub-dictionary, dict.view will expand the associated dictionary. If multiple dictionaries are associated with the key argument to expand only the first one encountered will be expanded.
- `expandall(ARG_NAME_0: list)` — Show all levels
  The word expandall expands dict.view to show all levels of all sub-dictionaries.

## GUI behaviors

- `(mouse)` — Naviagate dictionary view
  Clicking on a triangle will expand or collapse that element in the dictionary hierarchy. You can scroll the view of the dictionary using the mousewheel or trackpad or by clicking and dragging the scrollbar on the right.

## Attributes

- `@attr_attr_save` (int)
- `@dynamiccolor_default` (symbol)
- `@introduced` (symbol)
- `@label` (symbol)
- `@paint` (int)
- `@preview` (symbol)
- `@save` (int)
- `@set` (pointer)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example — [dict.view]
  fan-in:
    in0 ← [dict @embed 1] ← [button] ← [loadbang]
```

## See also

`dictionaries`, `dict`, `dict.print`
