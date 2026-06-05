# dict.pack

_max · Dictionary_

> Create a dictionary and set its values

Use the dict.pack object to create a dictionary and set its values using dedicated inlets.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | value for the 'foo' key |
| out0 | dictionary output |

## Arguments

- **name** (`symbol`) _(optional)_ — Name of the dictionary to create
- **default-values** (`list`) _(optional)_ — Dictionary syntax specifying initial keys and values composing the dictionary
  Dictionary syntax specifying initial keys and values composing the dictionary. See Dictionaries for more information on dictionary syntax.

## Messages

- `bang` — Send out the dictionary
- `int(value: int)` — Define the value for the key associated with the inlet
- `float(value: float)` — Define the value for the key associated with the inlet
- `list(value: list)` — Define the value for the key associated with the inlet
- `anything(value: list)` — Define the value for the key associated with the inlet
- `array(ARG_NAME_0: list)` — TEXT_HERE
- `dictionary(value: symbol)` — Define the value for the key associated with the inlet
- `string(ARG_NAME_0: list)` — TEXT_HERE

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `honk` — seen as: `honk`
- `moo` — seen as: `moo`
- `quack` — seen as: `quack quack`
- `roar` — seen as: `roar $1 times`

## Help patcher examples

### advanced

> by packing a dictionary and then sending it to another dict.pack, a reference to the packed dictionary is packed inside of the second dictionary. however, what is stored in the dictionary is just a dictionary message with the name of the dictionary. in other words, it's a reference -- not a direct copy. this is important because when you clone the dictionary you will only clone the reference information not the actual included dictionary's content.

```
Example #1 — [dict.pack baked: beans:]
  fan-in:
    in0 ← [number]
    in1 ← [number]
  fan-out:
    out0 → [dict.pack bon: jovi:]:in1
    out0 → [message "dictionary u815002856"]:in1
```

```
Example #2 — [dict.pack bon: jovi:]
  fan-in:
    in0 ← [dict.pack jumbo: shrimp:]
    in1 ← [dict.pack baked: beans:]
  fan-out:
    out0 → [dict.unpack bon: jovi:]:in0
    out0 → [dict.view]:in0    # dictionary shown in a dict.view object
```

```
Example #3 — [dict.pack jumbo: shrimp:]
  fan-in:
    in0 ← [number]
    in1 ← [number]
  fan-out:
    out0 → [dict.pack bon: jovi:]:in0
    out0 → [message "dictionary u789002859"]:in1
```

### basic

> dict.unpack extracts only the keys specified as arguments, if they exist in the dictionary received.

```
Example #1 — [dict.pack cow : moo lion : roar 5 times noah : 1 @name ark]  an optional first argument defines the name of the dictionary. additional arguments define the dictionary prototype according to these rules: + a symbol preceeding a colon specifies a key + a value following the colon specifies a default value for the preceeding key + multiple values for a key are interpretted as a list
  fan-in:
    in0 ← [message "moo"]
    in1 ← [message "roar $1 times"]
  fan-out:
    out0 → [t b l]:in0
```

```
Example #2 — [dict.pack frog: pig: horse: duck: "quack quack" goose: @triggers -1]
  fan-in:
    in0 ← [number]
    in0 ← [button]
    in1 ← [flonum]
    in2 ← [message "1 2 foo 3"]
    in3 ← [message "quack quack"]
    in4 ← [message "honk"]
  fan-out:
    out0 → [t b l]:in0
```

## See also

`dictionaries`, `dict.compare`, `dict.deserialize`, `dict.group`, `dict.iter`, `dict.join`, `dict.print`, `dict.route`, `dict.serialize`, `dict.slice`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
