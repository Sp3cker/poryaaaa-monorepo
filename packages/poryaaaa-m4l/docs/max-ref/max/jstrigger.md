# jstrigger

_max · Languages_

> Execute Javascript instructions sequentially

The jstrigger object is similar to the trigger object, except that typed-in arguments within parentheses are passed to the Javascript evaluator. For more information on the Max implementation of Javascript, refer to the Javascript in Max manual. For complete information about Javascript itself, consult a reference book such as Javascript: The Definitive Guide by David Flanagan, published by O'Reilly.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message Used for Actions |

## Arguments

- **sequential-Javascript-instructions** (`numbers, symbols, or expressions`) — TEXT_HERE
  The arguments to the jstrigger object may be either constants or expressions. Constants are numbers or symbols. For each constant, an outlet will be created, and the constant value will be sent out the corresponding outlet when the object receives a message in its left inlet. For example, jstrigger with the arguments ready set 74 would send 74 out the right outlet, followed by set out the middle outlet, followed by ready out the left outlet.

 Expressions are Javascript expressions contained within parentheses. You can include more than one Javascript statement can be contained within the parentheses, but you must separate the statements by semicolons (;). A semicolon after the last statements is not required, and the word return is not required either. To return a list, you can either create an array object or place items in square brackets separated by commas. Javascript allows you to enter expressions between the commas. See the Examples section.

 For each expression, an outlet will be created, and the value of the expression will be sent out the corresponding outlet when the jstrigger object receives a message in its left inlet.

 Note that any use of semicolons or commas in an object box require a preceding backslash (\) character, otherwise you will see the following error message in the Max Console and the object will not be created:

 * error: object box has comma or semicolon

 In addition, it is strongly recommended to use single quotes (') rather than double quotes to define string literals. The use of double quotes can produce unexpected results in jstrigger when the object is saved and recreated in a patcher.

## Messages

- `bang` — The most recently stored values for each argument are assigned to the a array.
  The most recently stored values for each argument are assigned to the a array. Then the expressions in the object are evaluated, right to left, and the value of each expression or constant is sent out the outlet corresponding to each expression.
- `int(input: int)` — Contextual/User-specified according to Javascript arguments.
- `float(input: float)` — Contextual/User-specified according to Javascript arguments.
- `list(input: list)` — Contextual/User-specified according to Javascript arguments.
- `anything(input: list)` — Contextual/User-specified according to Javascript arguments.

## Help patcher examples

### basic

```
Example #1 — [jstrigger (a[0] * Math.random()) bang ( ['brad'\,a[1]\,2\,3] )]
  fan-in:
    in0 ← [message "74 53"]    # Output the symbol brad and the second arg then 2 3 followed by bang and then a random number times the value of the incoming argument.
  fan-out:
    out0 → [number]:in0    # Array literals in Javascript are in square brackets separated by commas. In message boxes, these must be preceded by a backslash character. In object boxes, commas do not need to be escaped.
    out1 → [button]:in0
    out2 → [message ""]:in1
```

```
Example #2 — [jstrigger (a[0] + 1) (a[0] - 1)]
  fan-in:
    in0 ← [number]
  fan-out:
    out0 → [number]:in0
    out1 → [number]:in0
```

## See also

`bangbang`, `js`, `jsui`
