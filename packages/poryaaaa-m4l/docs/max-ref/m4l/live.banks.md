# live.banks

_m4l · Live Control Surface_

> Manage Max for Live Device banks for Push controllers.

Create, edit and delete Max for Live Device parameter banks, as displayed on Ableton's Push controllers. Banks are saved with the device, but can be modified in real-time to cause updates on the Push display. For instance, you might want to reveal or hide particular parameters within a bank depending on device state.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Message in |
| out0 | Color in RGBA Format |

## Messages

- `delete(bank_id: int)` — Delete an existing bank.
  Delete a parameter bank. Deleting a bank will decrement the index of the banks with higher indices.
- `edit(bank_id: int, bank_name: symbol, [bank_parameters: list])` — Edit an existing bank.
  Edit a parameter bank, specified by bank_id (0-indexed). To change the bank name, pass a new name as bank_name, or pass - (minus) to use the existing name. The optional list of parameters should be a list of index/name pairs in the form parameter_index parameter_name. For instance, edit 0 newname 0 foo 2 bar 4 - 5 bap would change the name of bank 0 to newname and modify parameter encoder slot 0 to use the parameter foo, slot 2 to use the parameter bar, eliminate the parameter in slot 4, and assign slot 5 to the parameter named bap.
  To add button assignments, add the word @buttons after the list of encoder assignments, followed by a list of parameters using the same format as above.
- `getcount` — Report the current number of banks.
  Sends the count message from the outlet, followed by the number of banks.
- `getname(bank_id: int)` — Report the name of a bank.
  Sends the name message from the outlet, followed by the index (int) and name (symbol) of the specified bank.
- `getparameters(bank_id: int)` — Report the parameters in a bank.
  Sends the parameters message from the outlet, followed by the index (int) of and a list of parameter names (symbol) in the specified bank.
- `new(bank_id: int, bank_name: symbol, [bank_parameters: list])` — Create a parameter bank.
  Create a bank at the index specified by bank_id (0-indexed). If a bank already exists at that index, the new bank will be inserted at the specified index and all higher-indexed banks will have their indices incremented. The bank requires a name, specified by bank_name. The list of parameters should be a list of up to 8 symbols, specifying the parameter name for bank encoder slots 0-7. A - (minus) can be used to indicate that a slot should have no parameter associated with it.
  To add button assignments, add the word @buttons after the list of encoder assignments, followed by a list of parameters using the same format as above.
  Note: when creating banks, it is possible to specify indices higher than the bank count. That is, one could start with an empty set of banks and create bank 3 first. In that case, dummy banks will be created in slots 0, 1 and 2. Dummy banks are displayed with grey text in the Parameter Banks window. Editing a dummy bank will cause it to become a 'real' bank. Creating a new bank at the index occupied by a dummy bank will replace the dummy bank in-place, rather than performing an insert.

## GUI behaviors

- `(mouse)` — Double-click to open the Parameter Banks window.
  Double-click to open the Parameter Banks window. Only available in the editor, this feature is disabled inside of Live.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out0` — Color in RGBA Format

### buttons

> live.banks allows assigning parameters to control surface buttons, like the buttons above the display on Push.
>
> In the Inspector of a parameter object, you can select how the buttons will behave and look for this parameter using the Control Surface Button Mode attribute.
>
> On Push, button assignments are visible after accessing the device banks by pressing the display button above the device’s name. The first display button is not available for parameters on Push, since it is already in use as the bank exit button.

> edit <index> <name> [[paramindex paramname1]...] @buttons [[paramindex paramname3]...]

> buttons can be assigned by going to the Buttons tab in the Parameter Banks window.

```
Example — [live.banks]  double-click to see the editor window (only works in the Max For Live Editor)
  fan-in:
    in0 ← [message "edit 1 newestbankname 0 triple 1 baz 2 - @buttons 0 dual 1 triple"]    # edit <index> <name> [paramname1...] @buttons [paramname3...] / edit existing parameter bank with button assignments using parameter indices
    in0 ← [message "edit 0 newbankname triple - baz - - - - - @buttons dual triple"]    # edit existing parameter bank with button assignments
    in0 ← [message "new 0 slartybank baz - triple @buttons dual"]    # create new parameter bank with button assignments
```

### basic

> new <index> <name> [paramname1, paramname2...]

> edit <index> <name> [paramname1, paramname2...]

> edit existing parameter bank using parameter indices

> Note: after adding a new parameter object to a device, you need to save the device before the parameter is available for mapping with live.banks.

```
Example — [live.banks]  double-click to see the editor window (only works in the Max For Live Editor)
  fan-in:
    in0 ← [message "new 2 brillobank bar foo enum enum foo bar"]
    in0 ← [message "delete 1"]    # delete <index> / delete existing parameter bank
    in0 ← [message "new 1 bartbank bar foo enum bar foo enum"]
    in0 ← [message "new 3 fastbank enum foo bar - bar - foo enum"]    # for a slot without a parameter, use - instead of a name / edit existing parameter bank
    in0 ← [message "edit 1 newestbank 0 foo 2 bar 4 enum"]
    in0 ← [message "edit 1 newbank bar - foo - bar - - - -"]
    in0 ← [message "new 0 slartybank bar foo enum"]
    in0 ← [message "edit 1 - 0 bar 2 bar 4 bar 5 -"]
```

## See also

`live.arrows`, `live.button`, `live.dial`, `live.drop`, `live.gain`, `live.line`, `live.numbox`, `live.slider`, `live.tab`, `live.toggle`
