# dict

_max · Dictionary_

> Create and access dictionaries

Use the dict object to create named dictionaries, clone existing dictionaries, and query existing dictionaries to access their data.

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

### Port details

**`out2` (list of keys in response to 'getkeys' message):** List of keys in response to 'getkeys' message, or a boolean in response to the 'contains' message.

## Arguments

- **name** (`symbol`) _(optional)_ — Name to be associated with this dictionary
  Name to be associated with this dictionary. If no argument is given, then a unique name will be generated.
- **filename** (`symbol`) _(optional)_ — Name of a JSON or YAML file to be imported
  Name of a JSON or YAML file to be imported into this dictionary on load.

## Messages

- `bang` — Send a reference to the dictionary from the first outlet.
- `append(key: symbol, value: list)` — Add values to an array.
  Add values to the end of an array associated with the specified key.
- `clear` — Erase the contents of the dictionary, restoring to a clean state.
- `clone(name: symbol)` — Make a clone of the incoming dictionary.
  Make a clone of the incoming dictionary. If received at the first inlet, send a reference to this new clone from the first outlet. Otherwise just clone the dictionary and don't send it out.
- `contains(key: symbol)` — Return a 0 or 1 to the third outlet indicating the specified key exists (or doesn't) in the dictionary.
- `dictionary(name: symbol)` — Make a clone of the incoming named-dictionary.
  Make a clone of the incoming dictionary. If received at the first inlet, send a reference to this new clone from the first outlet. Otherwise just clone the dictionary and don't send it out.
- `edit` — Open a dictionary editor window.
  Open the dictionary editor window.
- `export([filename: symbol])` — Write the dictionary contents to a file
  Write the dictionary contents to a JSON or YAML file. If no path/filename is provided, a dialog will be presented. The file format is determined from the file name extension, either '.json' or '.yaml'. A success/failure notification will be sent to the rightmost outlet in the form export [filename] 0/1.
- `get(key: symbol)` — Return the value associated with a key to the second outlet.
- `getkeys([alphabetize: bool])` — Return a list of all the keys in a dictionary to the third outlet.
  Return a list of all the keys in a dictionary to the third outlet. By default the keys are sorted according to the order in which keys were added to the dictionary. Use the optional argument to specify alphabetical sorting.
- `getnames` — Return a list of all the dictionaries that currently exist to the fourth outlet.
- `getsize(key: symbol)` — Return the number of values associated with a key to the second outlet.
- `gettype(key: symbol)` — Return the type of the values associated with a key to the second outlet.
- `import([filename: symbol])` — Read the dictionary contents from a file
  Read the dictionary contents from a JSON or YAML file. If no path/filename is provided, a dialog will be presented. The file format is determined from the file name extension, either '.json' or '.yaml'. A success/failure notification will be sent to the rightmost outlet in the form import [filename] 0/1.
- `parse(key: symbol, value: symbol)` — Replace the content of a dictionary.
  Replace the content of a dictionary by providing the new content as JSON.
- `pull_from_coll(coll-name: symbol)` — Pull the content of a named coll object into the dictionary
  Pull the content of a named coll object into the dictionary. The indices in the coll will become the keys, and the values for those indices the values for the dictionary's keys.
- `push_to_coll(coll-name: symbol)` — Push the dictionary's content into a named coll object
  Push the dictionary's content into a named coll object. The keys in the dictionary will become the indices in the coll, and the values for those indices the values of the dictionary's keys.
- `read([filename: symbol])` — Read the dictionary contents from a file
  Read the dictionary contents from a JSON or YAML file. If no path/filename is provided, a dialog will be presented. The file format is determined from the file name extension, either '.json' or '.yaml'. A success/failure notification will be sent to the rightmost outlet in the form read [filename] 0/1.
- `readagain` — Reload last used file from disk
  re-reads an JSON or YAML file previously specified by the read or write messages. If no file has been previously specified, a standard File Dialog will be presented for the user to manually choose the file to be read. A success/failure notification will be sent to the rightmost outlet in the form read [filename] 0/1.
- `readany([filename: symbol])` — Read the dictionary contents from a file
  Read the dictionary contents from a JSON or YAML file. If no path/filename is provided, a dialog will be presented. The file format and extension are not checked. The contents of the file are assumed to be in JSON format. A success/failure notification will be sent to the rightmost outlet in the form read [filename] 0/1.
- `remove(key: symbol)` — Remove a key and its associated value from the dictionary.
- `replace(key: symbol, value: list)` — Set the value for a key to a specified value, creating heirarchy.
  Set the value for a key to a specified value. If a heirarchy is specified for the key, and the heirarchy does not exist, then it will be created in the dictionary.
- `set(key: symbol, value: list)` — Set the value for a key to a specified value.
- `setparse(key: symbol, value: symbol)` — Set the value for a key to a dictionary.
  Set the value for a key to dictionary content defined using JSON.
- `wclose` — Close the dictionary editor window.
  Close the dictionary editor window if it is open.
- `write([filename: symbol])` — Write the dictionary contents to a file
  Write the dictionary contents to a JSON or YAML file. If no path/filename is provided, a dialog will be presented. The file format is determined from the file name extension, either '.json' or '.yaml'. A success/failure notification will be sent to the rightmost outlet in the form write [filename] 0/1.
- `writeagain` — Write the dictionary contents to a file
  Write the dictionary contents to a JSON or YAML file. The file provided as an argument for the previous 'write' or 'export' message will be used. A success/failure notification will be sent to the rightmost outlet in the form write [filename] 0/1.

## GUI behaviors

- `(mouse)` — Open a dictionary editor window by double-clicking
  Double-click a dict object to open a dictionary editor window.

## Attributes

- `@introduced` (symbol)

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `out4` — file operation success/failure notifications
> - `in1` — dictionary input clones but does not send

### advanced

> Accessing complex structures such as arrays of dictionaries is best done using the js object. However, with a bit of patching it is also possible to access such members in a patcher.

```
Example — [dict filters @embed 1]
  fan-in:
    in0 ← [prepend get] ← [combine lowpass [ 0 ] @triggers 0 2]
    in0 ← [message "get lowpass"]
    in0 ← [loadbang]
    in0 ← [message "get highpass"]    # Get top-level dictionaries. Watch the bottom right plots.
  fan-out:
    out0 → [dict.view]:in0
    out1 → [route lowpass highpass]:in0
    out1 → [t b l]:in0
```

### arrays

> Access individual items in an array
>
> Use bracket syntax ([ ]) in a key (with no whitespace) to indicate that you wish to directly access a single value in an array that is associated with a given key.
>
> Indexing of the array begins counting at zero.

```
Example #1 — [dict bicycle]
  fan-in:
    in0 ← [message "getsize drivetrain::cassette"]
    in0 ← [message "getsize drivetrain::cassette::cogs"]    # How many values are present for a given key?
  fan-out:
    out1 → [message "drivetrain::cassette::cogs 10"]:in1
```

```
Example #2 — [dict bicycle]
  fan-in:
    in0 ← [message "gettype drivetrain::cassette::cogs[2]"]
    in0 ← [message "gettype drivetrain"]    # What kind of value is associated with a certain key or array element?
    in0 ← [message "gettype drivetrain::cassette::cogs"]
    in0 ← [message "gettype wheels::rear::lacing"]
  fan-out:
    out1 → [message "drivetrain dictionary"]:in1
```

```
Example #3 — [dict bicycle @embed 1]
  fan-in:
    in0 ← [message "get drivetrain::cassette::cogs[9]"]
    in0 ← [loadbang]
    in0 ← [button]
    in0 ← [message "get drivetrain::cassette::cogs"]
    in0 ← [message "set drivetrain::cassette::cogs[9] 26"]
    in0 ← [message "get drivetrain::cassette::cogs[4]"]
    in0 ← [message "set drivetrain::cassette::cogs[9] 25"]
  fan-out:
    out0 → [dict.view]:in0
    out1 → [message "drivetrain::cassette::cogs 10"]:in1
```

### hierarchy

> Use double-colon syntax (::) in the key to specify a key in a nested dictionary.

> Notice that "bicycle" contains the key "wheels" whose value is itself a dictionary! That dictionary contains even further sub-dictionaries.

```
Example — [dict bicycle @embed 1]
  fan-in:
    in0 ← [message "set wheels::front::brand profile"]    # Create a new hierarchical key
    in0 ← [loadbang]
    in0 ← [message "remove wheels::bob"]
    in0 ← [message "replace wheels::rear::tirewidth 22"]    # Change an existing value in the hierarchy
    in0 ← [message "get wheels::front::spokecount"]
    in0 ← [message "get wheels::rear::spokecount"]
    in0 ← [message "get wheels::front::lacing"]    # Get values of nested keys
    in0 ← [message "get wheels::rear::lacing"]
    in0 ← [message "set wheels::bob dictionary nouns"]    # Add an existing dict as a nested sub-dict
  fan-out:
    out0 → [dict.view]:in0
    out1 → [message "wheels::front::spokecount 24"]:in1
```

### embedding

> Dictionary content can be embedded and saved with the patcher by using the @embed attribute.

```
Example — [dict @embed 1]
  fan-in:
    in0 ← [loadbang]
  fan-out:
    out0 → [dict.view]:in0
```

### js

```
Example #1 — [dict "parameter example"]
  fan-in:
    in0 ← [loadbang]
  fan-out:
    out0 → [dict.view]:in0
```

```
Example #2 — [dict lunchmeat]
  fan-in:
    in0 ← [loadbang]
  fan-out:
    out0 → [dict.view]:in0
```

### coll

```
Example — [dict @name "northern copy"]
  fan-in:
    in0 ← [dict.pack wolverine : porcupine : badger : woodchuck : @triggers 0 1 2 3 @name "northern animals"]
    in0 ← [message "pull_from_coll phoo"]
    in0 ← [message "push_to_coll boo"]
  fan-out:
    out0 → [dict.view]:in0
```

### files

```
Example #1 — [dict joe dict_file.json]
  fan-in:
    in0 ← [button]    # output dict
  fan-out:
    out0 → [dict.view]:in0    # The optional second argument will load the .json or .yaml/.yml file on instantiation
```

```
Example #2 — [dict fred]
  fan-in:
    in0 ← [message "import dict_file.json"]
    in0 ← [button]    # output current dict
    in0 ← [message "import dict_file2.yml"]    # Import either .json or .yaml/.yml files
    in0 ← [message "export ~/Desktop/my_dict_file.json"]    # Write to a json file to the Desktop
    in0 ← [message "import"]
    in0 ← [message "export"]    # If no filename is given, a dialog will ask where to import/export
  fan-out:
    out0 → [dict.view]:in0
```

### copies

> Sending a named dictionary to another named dictionary will clone (copy) its contents

> Notice that changes/updates to dictionary "a" do not automatically change dictionary "b".

```
Example #1 — [dict b]
  fan-in:
    in0 ← [dict a]
    in0 ← [message "clear"]
  fan-out:
    out0 → [dict.view]:in0
```

```
Example #2 — [dict a]
  fan-in:
    in0 ← [message "set do re mi, set z 500"]
    in0 ← [button]
    in0 ← [message "replace z new"]
  fan-out:
    out0 → [dict.view]:in0
    out0 → [dict b]:in0
```

### queries

> Request information from a dictionary such as the valid keys, the data stored at those keys, or a list of all the names for currently existing dictionaries.

> Get a list of all named dictionaries out the last outlet

```
Example #1 — [dict nouns @embed 1]
  (no patch cords)
```

```
Example #2 — [dict nouns]
  fan-in:
    in0 ← [message "getsize colors"]
    in0 ← [message "gettype animals"]
    in0 ← [message "gettype colors"]
    in0 ← [message "getsize animals"]
    in0 ← [message "get rocks"]    # Get info about the value associated with a key
    in0 ← [message "get plants"]
  fan-out:
    out1 → [message ""]:in1
```

```
Example #3 — [dict]
  fan-in:
    in0 ← [message "getnames"]
  fan-out:
    out3 → [message ""]:in1
```

```
Example #4 — [dict nouns]
  fan-in:
    in0 ← [message "contains animals"]
    in0 ← [message "contains buildings"]
    in0 ← [message "getkeys"]    # Get a list of all keys in a dict or check if the dict contains a key
  fan-out:
    out2 → [message ""]:in1
```

### basic

> It updates any time its associated named dictionary updates.

```
Example #1 — [dict myDict]
  fan-in:
    in0 ← [button]
  fan-out:
    out0 → [dict.view]:in0    # The dict.view object displays dictionary content directly in the patcher.
    out0 → [message "dictionary myDict"]:in1    # Dictionaries are passed by sending the 'dictionary' message with the name as an argument (like Jitter matrices)
```

```
Example #2 — [dict myDict]
  fan-in:
    in0 ← [message "set numbah 1"]
    in0 ← [message "clear"]
    in0 ← [message "set liszt z y x 74"]
    in0 ← [message "set y foo"]
    in0 ← [message "set y do"]
    in0 ← [message "remove numbah"]    # Remove keys
```

## See also

`dictionaries`, `external_text_editor`, `dict.view`, `dict.pack`, `dict.unpack`, `dict.group`, `dict.iter`, `dict.join`, `dict.slice`, `dict.print`, `dict.route`, `dict.strip`, `dict.serialize`, `dict.deserialize`
