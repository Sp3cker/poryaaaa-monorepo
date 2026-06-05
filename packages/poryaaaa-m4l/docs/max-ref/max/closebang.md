# closebang

_max · Patching_

> Send a bang on close

Sends a bang whenever the patcher window within which it resides is closed.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Sends bang When Patcher Window Is Closed |
| out0 | Output bang |

### Port details

**`out0` (Output bang):** Output bang When Patcher Window Is Closed.

## Messages

- `bang` — Output a bang message
  Sending a bang message to a closebang object causes it to output a bang message.

## GUI behaviors

- `(mouse)` — TEXT_HERE

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Output bang
> - `in0` — Sends bang When Patcher Window Is Closed

## See also

`active`, `button`, `freebang`, `loadbang`, `loadmess`, `savebang`
