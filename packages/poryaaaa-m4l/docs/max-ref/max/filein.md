# filein

_max · Files_

> Read and access a file of binary data

filein reads a file of binary data and outputs the data at various points in the file given the appropriate input.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | Read Byte from File |
| in1 | Read Word from File |
| in2 | Read Long from File |
| out0 | File Data Output |
| out1 | bang On End of File |
| out2 | bang When Read/Spool Finished |

## Arguments

- **filename** (`symbol`) _(optional)_ — A file path
  Specifies a filename to be read into the filein object automatically when the patch is loaded.
- **spool** (`symbol`) _(optional)_ — Disk access flag
  A second argument of the word spool will cause the file to be accessed from disk rather than read into memory.

## Messages

- `int(offset: int)` — Set a file offset, output a byte
  Specifies a byte offset in a binary file, and outputs the data stored at that point in the file.
- `fclose` — Close a previously opened file
  Closes the file being read, making filein no longer respond to int or list messages.
- `in1(offset: int)` — Set a file offset, output an integer
  In middle inlet: The 16-bit word contained at that byte offset in the file is sent out the left outlet as an unsigned (short) integer.
- `in2(byte-offset: int)` — Set a file offset, output an integer
  In right inlet: The 32-bit word contained at that byte offset within the file is sent out the left outlet as an unsigned (long) integer.
- `read([filename: symbol])` — Read a file into memory
  Displays a standard file dialog to select a file to be read into memory. If the word read is followed by a filename found in Max's search path, that file will be automatically read into memory.
- `spool([filename: symbol])` — Open a file for disk access
  Displays a standard file dialog to select a file, which will be accessed from disk whenever an int is received. If the word spool is followed by a filename found in Max's search path, that file will be automatically pointed to for future access. This method of accessing a file occupies less RAM, but does not output data immediately at interrupt level in response to an int message.

## Help patcher examples

### basic

```
Example — [filein filein_data.bin]
  fan-in:
    in0 ← [number]    # read byte / access file from disk
    in0 ← [message "fclose"]    # close file
    in0 ← [message "read"]    # read entire file into RAM
    in0 ← [message "spool"]
    in1 ← [number]    # read 16 bit word at offset
    in2 ← [number]    # read 32 bit word at offset
  fan-out:
    out0 → [number]:in0    # data output
    out1 → [button]:in0    # bang on end of file
    out2 → [button]:in0    # bang when 'read' finished
```

## See also

`text`
