# dict.deserialize

_max · Dictionary_

> Create a dictionary from text

Use the dict.deserialize object to create a dictionary from text passed in using Max's dictionary syntax or JSON. Alternatively, compressed forms of either format may be used.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary whose contents will be deserialized |
| out0 | deserialized data from dictionary |

## Arguments

- **name** (`symbol`) _(optional)_ — Name of the dictionary to create from serialized input

## Messages

- `anything(serialized-dictionary: list)` — Dictionary syntax, JSON, or a compressed form of either, from which a dictionary will be created

## Attributes

- `@basic` (int)
- `@category` (symbol)
- `@label` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `"{` — seen as: `"{
	\"pig\" : 0.05,
	\"frog\" : -8,
	\"horse\" : [ 1, 2, \"foo\", 3 ],
	\"goose\" : [ \"honk\" ],
	\"duck\" : [ \"quack quack\" ]
}
"`
- `goose` — seen as: `goose : honk pig : 0.05 frog : -8 horse : 1 2 foo 3 duck : "quack quack"`

## Help patcher examples

### advanced

> see the discussion in the reference page for caveats regarding use of these attributes.

```
Example #1 — [dict.deserialize]
  fan-in:
    in0 ← [message "75.3o8TJ87yu3TURAkrBHNi7yKafTEjY5PDv.8LvTETJshxGJecs.jZJBl5MTAi.Ja94qjBFqfRoTZxYC0XJrzDSNaE.SpDC.fnkhgg"]    # note that this 'compressed' version actually has a higher byte-count than the uncompressed version. / benefits: + no symbol table bloating + smallest footprint / bytecount + no cpu spent on codec + human readable
  fan-out:
    out0 → [dict.iter]:in0
```

```
Example #2 — [dict.deserialize]
  fan-in:
    in0 ← [message "goose : honk pig : 0.05 frog : -8 horse : 1 2 foo 3 duck : "quack quack""]
  fan-out:
    out0 → [dict.iter]:in0
```

```
Example #3 — [dict.deserialize]
  fan-in:
    in0 ← [message "91.3o8plKNUpfLSWIErRACzy.S0AH2zJJev700BP7xH+hJNUPbiVAC0QAizQAkRK+7URGELVgXAIc54mOLoApz7xVIHBmRoImMTQKrzDSNaE.SBTRtpkK..JfFG4."]
  fan-out:
    out0 → [dict.iter]:in0
```

```
Example #4 — [dict.deserialize]
  fan-in:
    in0 ← [message ""{
	\"pig\" : 0.05,
	\"frog\" : -8,
	\"horse\" : [ 1, 2, \"foo\", 3 ],
	\"goose\" : [ \"honk\" ],
	\"duck\" : [ \"quack quack\" ]
}
""]
  fan-out:
    out0 → [dict.iter]:in0
```

### basic

```
Example — [dict.deserialize]
  fan-in:
    in0 ← [route /my/osc/message] ← [udpreceive 7474]
  fan-out:
    out0 → [dict.iter]:in0
```

## See also

`dictionaries`, `dict.compare`, `dict.group`, `dict.iter`, `dict.join`, `dict.pack`, `dict.print`, `dict.route`, `dict.serialize`, `dict.slice`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
