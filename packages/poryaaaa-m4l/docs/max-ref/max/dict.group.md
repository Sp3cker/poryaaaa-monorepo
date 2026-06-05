# dict.group

_max · Dictionary_

> Build a dictionary iteratively

Use the dict.group object to build up a dictionary by sending key-value pairs as lists. The key-value pairs will be collected into the dictionary until a 'bang' is received. The 'bang' will send out the dictionary and start the process over again.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary syntax for appending a key/value pair, bang sends the dictionary and resets |
| out0 | dictionary output |

## Arguments

- **name** (`symbol`) _(optional)_ — Name of the dictionary to create

## Messages

- `bang` — Send out the dictionary and reset the contents
- `list(key-value: list)` — Set a key and its values
  Set a key and its values. The first element of the list is the key, followed by its values. Lists will be collected into the dictionary until a bang is received.
- `anything(dictionary-or-keyvaluepair: list)` — A key-value pair will add the key/value to the dictionary or use dictionary syntax to add more complex structures.

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `a` — seen as: `a : 10 20 30, b : 40 50 60, bang`
- `bar:` — seen as: `bar: goo wah`
- `cat` — seen as: `cat : on the mat`
- `foo` — seen as: `foo man chu`
- `moo` — seen as: `moo :shoo flu`

## Help patcher examples

### preserving-contents

> The dict.group object functions for dictionaries like the zl.group object does for lists. This means that as soon as the dictionary is sent forward, the dictionary is then cleared so that the process can begin over again.
>
> Unlike lists, however, dictionaries are passed by reference. The result is that if you do not use the dictionary immediately, the values collected in the dictionary might be cleared before you actually get around to using them!
>
> The solution is to insert a dict object afterwards. This will clone (make a copy of) the dictionary prior to its being cleared.

```
Example — [dict.group]
  fan-in:
    in0 ← [message "a : 10 20 30, b : 40 50 60, bang"]
  fan-out:
    out0 → [dict]:in0    # clone input dictionary
    out0 → [message "dictionary u444000026"]:in1
```

### basic

```
Example — [dict.group]
  fan-in:
    in0 ← [button]
    in0 ← [message "cat : on the mat"]
    in0 ← [message "moo :shoo flu"]    # messages may also use dictionary syntax
    in0 ← [message "bar: goo wah"]
    in0 ← [message "foo man chu"]    # messages may pass a key followed by N values
  fan-out:
    out0 → [dict.print]:in0
    out0 → [dict]:in0    # make a copy to preserve the output
```

## See also

`dictionaries`, `dict.compare`, `dict.deserialize`, `dict.iter`, `dict.join`, `dict.pack`, `dict.print`, `dict.route`, `dict.serialize`, `dict.slice`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
