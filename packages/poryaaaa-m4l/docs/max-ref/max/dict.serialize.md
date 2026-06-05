# dict.serialize

_max · Dictionary_

> Convert a dictionary's content to text

Use the dict.serialize object to output a serialized form of the contents of dictionary in text format. The text may be Dictionary Syntax, JSON, or Base64-compressed forms of these formats.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary whose contents will be serialized |
| out0 | serialized data from dictionary |

## Messages

- `dictionary(name: symbol)` — Name of a dictionary whose content will be serialized

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### advanced

> see the discussion in the reference page for caveats regarding use of these attributes.

```
Example #1 — [dict.serialize @mode dictionary @compress 1]
  fan-in:
    in0 ← [dict.pack frog: pig: horse: duck: "quack quack" goose: @triggers 0 1 2 3 4]
  fan-out:
    out0 → [message "75.3o8TJ87yu3TURAkrBHNi7yKafTEjY5PDv.8LvTETJshxGJecs.jZJBl5MTAi.Ja94qjBFqfRoTZxYC0XJrzDSNaE.SpDC.fnkhgg"]:in1    # note that this 'compressed' version actually has a higher byte-count than the uncompressed version. / benefits: + no symbol table bloating + smallest footprint / bytecount + no cpu spent on codec + human readable
```

```
Example #2 — [dict.serialize @mode dictionary]  this is the default
  fan-in:
    in0 ← [dict.pack frog: pig: horse: duck: "quack quack" goose: @triggers 0 1 2 3 4]
  fan-out:
    out0 → [message "goose : honk pig : 0.05 frog : -8 horse : 1 2 foo 3 duck : "quack quack""]:in1
```

```
Example #3 — [dict.serialize @mode json @compress 1]
  fan-in:
    in0 ← [dict.pack frog: pig: horse: duck: "quack quack" goose: @triggers 0 1 2 3 4]
  fan-out:
    out0 → [message "91.3o8plKNUpfLSWIErRACzy.S0AH2zJJev700BP7xH+hJNUPbiVAC0QAizQAkRK+7URGELVgXAIc54mOLoApz7xVIHBmRoImMTQKrzDSNaE.SBTRtpkK..JfFG4."]:in1
```

```
Example #4 — [dict.serialize @mode json]
  fan-in:
    in0 ← [dict.pack frog: pig: horse: duck: "quack quack" goose: @triggers 0 1 2 3 4]
  fan-out:
    out0 → [message ""{
	\"pig\" : 0.05,
	\"frog\" : -8,
	\"horse\" : [ 1, 2, \"foo\", 3 ],
	\"goose\" : [ \"honk\" ],
	\"duck\" : [ \"quack quack\" ]
}
""]:in1
```

### basic

```
Example — [dict.serialize]
  fan-in:
    in0 ← [dict.pack runner : true flyer : false trips : 5 @triggers 2]
  fan-out:
    out0 → [prepend /my/osc/message]:in0
```

## See also

`dictionaries`, `dict.compare`, `dict.deserialize`, `dict.group`, `dict.iter`, `dict.join`, `dict.pack`, `dict.print`, `dict.route`, `dict.slice`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
