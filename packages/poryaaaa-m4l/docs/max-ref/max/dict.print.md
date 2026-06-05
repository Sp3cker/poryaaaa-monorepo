# dict.print

_max · Dictionary_

> Post a dictionary to the Max Console

Use the dict.print object to post the content of a dictionary to the Max Console. For more control over how the printing is formatted, use dict.iter and print.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary input |

## Arguments

- **identifier** (`anything`) _(optional)_ — Max Console identifier
  The argument is an identifier for the dict.print object. Each message printed in the Max Console is preceded by the name of the dict.print object. If there is no argument, the name is dict.print.

## Messages

- `array(ARG_NAME_0: symbol)` — TEXT_HERE
- `dictionary(name: symbol)` — Dictionary whose contents will be posted to the Max Console

## Help patcher examples

### basic

> the arguments to dict.pack define a prototype dictionary with specified keys and, optiionally, values for those keys.

```
Example — [dict.print]
  fan-in:
    in0 ← [dict.join frog: pig: horse: duck: "quack quack"] ← [dict.pack cow: moo lion: roar 5 times noah: 1 @name ark] ← [button]
```

## See also

`dictionaries`, `dict.compare`, `dict.deserialize`, `dict.group`, `dict.iter`, `dict.join`, `dict.pack`, `dict.route`, `dict.serialize`, `dict.slice`, `dict.strip`, `dict.unpack`, `dict.view`, `dict`
