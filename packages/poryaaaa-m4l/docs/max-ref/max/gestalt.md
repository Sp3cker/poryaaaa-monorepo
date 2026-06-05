# gestalt

_max · System_

> Retrieve system information

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | symbol Reports System Information |
| out0 | Response to System Information Query |
| out1 | Error Code Returned by Query |

## Messages

- `bang` — Output previous data
  bang message causes gestalt to repeat its previous output.
- `anything(selector: symbol)` — Return selected system data
  Performs the same function as symbol.
- `keys` — List key names
  Returns a dictionary containing short key and long-form descriptive names (also functional) for available info.
- `symbol(selector: symbol)` — Return selected system data
  The symbol message containing a Gestalt selector will return the appropriate system specific information. Some sample selectors are "sysv" to return the system version and "qtim" to return the quicktime version.

## Attributes

- `@default` (int)
- `@label` (symbol)
- `@save` (int)
- `@style` (symbol)

## Additional messages used in help patcher

_These appear in example wiring but have no entry in the reference XML. Inferred from the help patcher only._

- `arch` — seen as: `arch`
- `architecture` — seen as: `architecture`
- `args` — seen as: `args`
- `arguments` — seen as: `arguments`
- `env` — seen as: `env`
- `environment` — seen as: `environment`
- `host` — seen as: `host`
- `host_name` — seen as: `host_name`
- `ncpu` — seen as: `ncpu`
- `num_processors` — seen as: `num_processors`
- `physical_memory` — seen as: `physical_memory`
- `pid` — seen as: `pid`
- `plat` — seen as: `plat`
- `platform` — seen as: `platform`
- `pmem` — seen as: `pmem`
- `pname` — seen as: `pname`
- `process_id` — seen as: `process_id`
- `process_name` — seen as: `process_name`
- `system_version` — seen as: `system_version`
- `system_version_legacy` — seen as: `system_version_legacy`
- `system_version_string` — seen as: `system_version_string`
- `sysv` — seen as: `sysv`
- `sysvers` — seen as: `sysvers`
- `vers` — seen as: `vers`

## Help patcher examples

### basic

```
Example — [gestalt @outputmode 1]  outputmode=1 prepends output from the left output with the selector
  fan-in:
    in0 ← [message "system_version"]
    in0 ← [message "vers"]
    in0 ← [message "pid"]
    in0 ← [message "ncpu"]
    in0 ← [message "pmem"]
    in0 ← [message "args"]
    in0 ← [message "keys"]    # get a list of short key + long-form descriptive names (also functional) for available info
    in0 ← [message "sysv"]
    in0 ← [message "system_version_legacy"]    # aka
    in0 ← [message "system_version_string"]    # aka
    in0 ← [message "process_id"]    # aka
    in0 ← [message "pname"]    # aka
    in0 ← [message "process_name"]
    in0 ← [message "num_processors"]    # aka
    in0 ← [message "physical_memory"]    # bytes / aka
    in0 ← [message "host_name"]    # aka
    in0 ← [message "platform"]
    in0 ← [message "plat"]
    in0 ← [message "architecture"]    # aka
    in0 ← [message "arch"]
    in0 ← [message "arguments"]    # aka
    in0 ← [message "environment"]    # aka
    in0 ← [message "sysvers"]    # aka
    in0 ← [message "host"]
    in0 ← [message "env"]
  fan-out:
    out0 → [route keys env environment]:in0
    out1 → [print err]:in0    # refer to Apple developer documentation for selectors and error codes
```

## See also

`screensize`
