# comment

_max · U/I_

> Explanatory note or label

comment displays text which is typed into it in order to serve as a label or explanatory text.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Messages in |

## Messages

- `append(message: list)` — Add text at end of current contents
  The word append followed by any message will set the comment object to display that message after any text which it already contains.
- `prepend(message: list)` — Add text in front of current contents
  The word prepend followed by any message will set the comment object to display that message before any text which it already contains.
- `set(message: list)` — Displays a message
  The word set followed by any message will set the comment object to display that message.
- `setwithtruncation(message: symbol, width: int, [suffix: symbol])` — Display a truncated message
  The word set followed by a symbol and a width (in pixels) will truncate the symbol to the given width. An optional third symbol will be appended onto the truncated text.
- `string(ARG_NAME_0: list)` — TEXT_HERE

## Attributes

- `@attr_attr_save` (int)
- `@basic` (int)
- `@category` (symbol)
- `@dynamiccolor_default` (symbol)
- `@label` (symbol)
- `@paint` (int)
- `@preview` (symbol)
- `@save` (int)
- `@set` (pointer)
- `@style` (symbol)
- `@stylemap` (symbol)

## Help patcher examples

### basic

> Color Commentary

```
Example #1 — [comment "Setting display:"]
  (no patch cords)
```

```
Example #2 — [comment "clear"]
  (no patch cords)
```

```
Example #3 — [comment "append"]
  (no patch cords)
```

```
Example #4 — [comment "prepend"]
  (no patch cords)
```

```
Example #5 — [comment "set displayed text"]
  (no patch cords)
```

```
Example #6 — [comment "....."]
  fan-in:
    in0 ← [message "set"]    # clear / .....
    in0 ← [message "prepend "and a one, ""]    # prepend
    in0 ← [message "append "and a three""]    # append
    in0 ← [message "set "and a two, ""]    # Setting display: / set displayed text
```

```
Example #7 — [comment "Unlock patcher and double-click to edit"]
  (no patch cords)
```

```
Example #8 — [comment "toggle underline"]
  (no patch cords)
```

```
Example #9 — [comment "Appearance:"]
  (no patch cords)
```

```
Example #10 — [comment "change background color"]
  (no patch cords)
```

```
Example #11 — [comment "change text color"]
  (no patch cords)
```

```
Example #12 — [comment "Color Commentary"]
  fan-in:
    in0 ← [attrui @bgcolor]    # change background color
    in0 ← [attrui @textcolor]    # Appearance: / change text color
    in0 ← [attrui @underline]    # toggle underline
```

```
Example #13 — [comment "Important thing here:"]
  (no patch cords)
```

Attributes demonstrated: `@bgcolor`, `@textcolor`, `@underline`

## See also

`ubutton`, `textedit`, `message`, `textbutton`, `live.comment`
