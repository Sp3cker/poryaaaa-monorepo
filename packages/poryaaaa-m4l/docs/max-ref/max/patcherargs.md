# patcherargs

_max · Patching_

> Retrieve patcher arguments

Retrieves patcher arguments and parses attribute style arguments. "normal" arguments are sent out the left outlet (first), and attribute style arguments are sent out the right outlet (second).

## Inlets / Outlets

| port | type | meaning |
|------|------|---------|
| in0 | bang | (bang) trigger output |
| out0 | list | (list) normal arguments |
| out1 | list | (list) attribute arguments |

## Arguments

- **defaults** (`int, float, symbol`) — Default arguments
  The patcherargs object permits access to more than 10 arguments for patchers which are typed into an object box, but those contained within a bpatcher object remain limited to 10 arguments.

## Messages

- `bang` — Output patcher arguments
  Sends a list of the parent patcher's arguments out the left outlet.
  If the parent patcher uses any attribute-style arguments (e.g. if any Jitter objects are used in the patcher), they are sent out the right outlet as a series of lists.

## GUI behaviors

- `(mouse)` — Output patcher arguments
  Double-clicking on the object will send the parent patcher's arguments out the left outlet.
  If the parent patcher uses any attribute-style arguments (e.g. if any Jitter objects are used in the patcher), they are sent out the right outlet as a series of lists.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — (list) normal arguments
> - `out1` — (list) attribute arguments
> - `in0` — (bang) trigger output

## See also

`bpatcher`, `thispatcher`
