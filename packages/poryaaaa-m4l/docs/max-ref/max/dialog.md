# dialog

_max · Interaction_

> Open a dialog box

Displays a dialog box with a selection of appearance modes. In Default mode, the dialog object permits entry of a symbol (as text) and sends it out the outlet when you click on the "OK" button in the dialog box. In the other modes, the dialog object displays text, but doesn't permit data entry.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Show Dialog |
| in1 | 0 Sets Output as Symbol, 1 as List/Message |
| out0 | Text Entered in Dialog / bang |
| out1 | bang When User Cancels |
| out2 | bang When User Clicks No in Extended mode |

## Arguments

- **label** (`symbol`) _(optional)_ — The dialog window label
  Sets the prompt which will appear above the text entry box in the dialog window. In Alert, Confirmation and Extended modes (mode= 1, 2, 3 or 4), the prompt is displayed as a title above the default text. See label.

## Messages

- `bang` — Open dialog box with default text
  In left inlet: Opens the dialog box with the previous text displayed as the default.
- `int(text: int)` — Open dialog box, using the value as default text
  See the symbol entry.
- `float(text: float)` — Open dialog box, using the value as default text
  In left inlet: Same as symbol.
- `clearsymbol` — Clear the default text
  In left inlet: Clears any previously set default text.
- `in1(output-mode: int)` — Set output mode
  Only applicable to Default display mode ( mode= 0):
  In right inlet: The number 0 configures the dialog object to send out the text typed by the user into the dialog box as a symbol preceded by the word symbol. A non-zero number configures the dialog object to send out the typed-in text exactly as-is if it begins with a word, or preceded by the word list if it begins with a number. If no number is received, it is considered 0 by default.
- `symbol(text: symbol)` — Open dialog box, using the value as default text
  In Default mode (mode= 0):
  In left inlet: The word symbol, followed by any word, opens a dialog box prompting the user to enter text. The word following symbol is shown as the default text.
  In other modes:
  In left inlet: the word symbol, followed by any word, opens a dialog box displaying that word.
  If you want more than one word to appear as the default text, you must enclose the words in double quotes.

## Attributes

- `@label` (symbol)
- `@style` (symbol)

## Help patcher examples

### basic

```
Example — [dialog Enter Name]
  fan-in:
    in0 ← [attrui @mask]    # Mask input text as bullets in text entry field (mode 0)
    in0 ← [button]    # Open dialog window
    in0 ← [message "symbol Untitled"]    # Set default text in text entry or text display
    in0 ← [attrui @mode]    # Set mode / 0: label, text field for entry and OK/Cancel buttons 1: label, symbol text and OK button 2: label, symbol text and Yes/No/Cancel buttons
    in0 ← [attrui @label]    # Set prompt / title text
    in1 ← [toggle]    # Output text as symbol or list (0/1)
  fan-out:
    out0 → [print text @popup 1]:in0    # output entered text or bang for 'Yes' or 'OK'
    out1 → [print cancelled @popup 1]:in0    # output bang for 'Cancel' (mode 1 & 3)
    out2 → [print no @popup 1]:in0    # output bang for 'No' (mode 2 only)
```

Attributes demonstrated: `@label`, `@mask`, `@mode`

## See also

`message`, `opendialog`, `savedialog`, `sprintf`
