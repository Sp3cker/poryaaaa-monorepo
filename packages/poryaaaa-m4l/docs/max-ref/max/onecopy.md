# onecopy

_max · Files_

> Prevent multiple copies of the same patcher from being opened

Use the onecopy object inside a patcher that you want to place in the extras folder for inclusion in the Extras menu. When the patcher's name is chosen using the Extras menu, its window will be brought to the front instead of opened a second time if it has already been loaded. The patch will be loaded if it is not currently open. The onecopy object cooperates with the Extras menu to ensure that only one copy of the patcher is opened at a time. However, opening the patcher containing a onecopy object by choosing Open... from the File menu will open additional copies.

## Help patcher examples

### basic

```
Example — [onecopy]
  (no patch cords)
```

## See also

`thispatcher`, `pcontrol`
