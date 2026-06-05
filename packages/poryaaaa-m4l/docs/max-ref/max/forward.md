# forward

_max · Control_

> Send messages to specified receive objects

Relays messages to other objects remotely. Unlike the send object, the destination receive object of forward can be changed with each message.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message To Remote, send Changes Receiver |

## Arguments

- **receiver** (`symbol`) _(optional)_ — Initial receive object name
  Sets the name for the receive object which will receive messages. This name can later be changed with the send message.

## Messages

- `bang` — Send a message to named receive objects
  See the anything entry.
- `int(input: int)` — Send a message to named receive objects
  See the anything entry.
- `float(input: float)` — Send a message to named receive objects
  See the anything entry.
- `list(input: list)` — Send a message to named receive objects
  See the anything entry.
- `anything(any message: list)` — Send a message to named receive objects
  Sends any message to all receive objects which share the name currently referred to by forward.
- `send(arguments: list)` — Set the destination for messages
  The word send, followed by the name of a receive object, sets the destination for any subsequent messages received by the forward object. This ability to change the destination of messages on the fly distinguishes forward from the send object.

## Help patcher examples

### basic

```
Example — [forward bip]
  fan-in:
    in0 ← [number]    # send data
    in0 ← [message "send bip"]    # specify a receive object
    in0 ← [message "send yyy"]    # change receiver
    in0 ← [message "send bem"]    # can be any receive
```

## See also

`message`, `pattrforward`, `receive`, `route`, `send`, `value`
