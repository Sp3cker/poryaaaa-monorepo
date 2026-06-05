# dict.codebox

_max · Dictionary_

> Create and access dictionaries

The dict.codebox object is a UI object for display and editing of dictionaries. Use the dict.codebox object to create named dictionaries, clone existing dictionaries, and query existing dictionaries to access their data.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | dictionary input clones and sends, bang sends |
| in1 | dictionary input clones but does not send |
| out0 | dictionary |
| out1 | value for an invidual key in response to the 'get' message |
| out2 | list of keys in response to 'getkeys' message |
| out3 | list of dictionaries in response to 'getnames' message |
| out4 | file operation success/failure notifications |

## Messages

- `bang` — Send a reference to the dictionary from the first outlet.
- `clear` — Erase the contents of the dictionary, restoring to a clean state.
- `remove(key: symbol)` — Remove a key and its associated value from the dictionary.
- `getkeys([alphabetize: bool])` — Return a list of all the keys in a dictionary to the third outlet.
  Return a list of all the keys in a dictionary to the third outlet. By default the keys are sorted according to the order in which keys were added to the dictionary. Use the optional argument to specify alphabetical sorting.
- `contains(key: symbol)` — Return a 0 or 1 to the third outlet indicating the specified key exists (or doesn't) in the dictionary.
- `getnames` — Return a list of all the dictionaries that currently exist to the fourth outlet.
- `getsize(key: symbol)` — Return the number of values associated with a key to the second outlet.
- `gettype(key: symbol)` — Return the type of the values associated with a key to the second outlet.
- `get(key: symbol)` — Return the value associated with a key to the second outlet.
- `set(key: symbol, value: list)` — Set the value for a key to a specified value.
- `append(key: symbol, value: list)` — Add values to an array.
  Add values to the end of an array associated with the specified key.
- `replace(key: symbol, value: list)` — Set the value for a key to a specified value, creating heirarchy.
  Set the value for a key to a specified value. If a heirarchy is specified for the key, and the heirarchy does not exist, then it will be created in the dictionary.
- `setparse(key: symbol, value: symbol)` — Set the value for a key to a dictionary.
  Set the value for a key to dictionary content defined using JSON.
- `parse(key: symbol, value: symbol)` — Replace the content of a dictionary.
  Replace the content of a dictionary by providing the new content as JSON.
- `clone(name: symbol)` — Make a clone of the incoming dictionary.
  Make a clone of the incoming dictionary. If received at the first inlet, send a reference to this new clone from the first outlet. Otherwise just clone the dictionary and don't send it out.
- `dictionary(name: symbol)` — Make a clone of the incoming named-dictionary.
  Make a clone of the incoming dictionary. If received at the first inlet, send a reference to this new clone from the first outlet. Otherwise just clone the dictionary and don't send it out.
- `read([filename: symbol])` — Read the dictionary contents from a file
  Read the dictionary contents from a JSON or YAML file. If no path/filename is provided, a dialog will be presented. The file format is determined from the file name extension, either '.json' or '.yaml'. A success/failure notification will be sent to the rightmost outlet in the form read [filename] 0/1.
- `readagain` — Reload last used file from disk
  re-reads an JSON or YAML file previously specified by the read or write messages. If no file has been previously specified, a standard File Dialog will be presented for the user to manually choose the file to be read. A success/failure notification will be sent to the rightmost outlet in the form read [filename] 0/1.
- `import([filename: symbol])` — Read the dictionary contents from a file
  Read the dictionary contents from a JSON or YAML file. If no path/filename is provided, a dialog will be presented. The file format is determined from the file name extension, either '.json' or '.yaml'. A success/failure notification will be sent to the rightmost outlet in the form import [filename] 0/1.
- `readany([filename: symbol])` — Read the dictionary contents from a file
  Read the dictionary contents from a JSON or YAML file. If no path/filename is provided, a dialog will be presented. The file format and extension are not checked. The contents of the file are assumed to be in JSON format. A success/failure notification will be sent to the rightmost outlet in the form read [filename] 0/1.
- `write([filename: symbol])` — Write the dictionary contents to a file
  Write the dictionary contents to a JSON or YAML file. If no path/filename is provided, a dialog will be presented. The file format is determined from the file name extension, either '.json' or '.yaml'. A success/failure notification will be sent to the rightmost outlet in the form write [filename] 0/1.
- `export([filename: symbol])` — Write the dictionary contents to a file
  Write the dictionary contents to a JSON or YAML file. If no path/filename is provided, a dialog will be presented. The file format is determined from the file name extension, either '.json' or '.yaml'. A success/failure notification will be sent to the rightmost outlet in the form export [filename] 0/1.
- `writeagain` — Write the dictionary contents to a file
  Write the dictionary contents to a JSON or YAML file. The file provided as an argument for the previous 'write' or 'export' message will be used. A success/failure notification will be sent to the rightmost outlet in the form write [filename] 0/1.
- `edit` — Open a dictionary editor window.
  Open the dictionary editor window.
- `wclose` — Close the dictionary editor window.
  Close the dictionary editor window if it is open.
- `pull_from_coll(coll-name: symbol)` — Pull the content of a named coll object into the dictionary
  Pull the content of a named coll object into the dictionary. The indices in the coll will become the keys, and the values for those indices the values for the dictionary's keys.
- `push_to_coll(coll-name: symbol)` — Push the dictionary's content into a named coll object
  Push the dictionary's content into a named coll object. The keys in the dictionary will become the indices in the coll, and the values for those indices the values of the dictionary's keys.

## GUI behaviors

- `(mouse)` — Open a dictionary editor window by double-clicking
  Double-click a dict object to open a dictionary editor window.

## Attributes

- `@attr_attr_save` (int)
- `@category` (symbol)
- `@dynamiccolor_default` (symbol)
- `@label` (symbol)
- `@legacydefault` (float, size 4)
- `@paint` (int)
- `@preview` (symbol)
- `@save` (int)
- `@set` (pointer)
- `@style` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out1` — value for an invidual key in response to the 'get' message
> - `out2` — list of keys in response to 'getkeys' message
> - `out3` — list of dictionaries in response to 'getnames' message
> - `out4` — file operation success/failure notifications
> - `in1` — dictionary input clones but does not send

### basic

```
Example #1 — [dict.codebox]
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [dict.view]:in0    # The dict.view object displays dictionary content directly in the patcher.
    out0 → [message "dictionary u327001345"]:in1    # Dictionaries are passed by sending the 'dictionary' message with the name as an argument (like Jitter matrices)
```

```
Example #2 — [dict.codebox]  It updates any time its associated named dictionary updates.
  fan-in:
    in0 ← [message "set numbah 1"]
    in0 ← [message "clear"]
    in0 ← [message "set liszt z y x 74"]
    in0 ← [message "set y foo"]
    in0 ← [message "set y do"]
    in0 ← [message "remove numbah"]    # Remove keys
    in0 ← [dict @embed 1] ← [button]
```

## See also

`dict`, `coll`, `coll.codebox`, `osc.codebox`
