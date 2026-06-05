# array.regexp

_max · Array_

> Use regular expressions to process input

With array.regexp, it's possible to use PERL-compatible regular expressions (PCRE) to match or make substitutions within arrays.

## Inlets / Outlets

| port | meaning |
|------|---------|
| in0 | re, substitute, or text in |
| in1 | re (regular expression) in |
| in2 | substitute in |
| out0 | substitutions |
| out1 | capture groups |
| out2 | subarrays |
| out3 | unmatched |
| out4 | subarray offsets |

## Arguments

- **expression** (`symbol`) _(optional)_ — Regular expression
  A regular expression may be used as an argument to set the regular expression (see above for regular expression formatting and metacharacter information).
- **substitution** (`array`) _(optional)_ — Substitution array
  An optional second argument will set the substitution array. As in other regexp objects, substitution arrays may contain capture groups; here they can be in the classic form %n (e.g. %1 for the first capture group), or encoded as bytes (%= 37, 1= 49).

## Messages

- `bang` — Trigger output
  Reprocess previously received array and trigger output.
- `int(value: int)` — Convert an integer to an array.
  Convert an incoming integer to an array, then process as described for the array message.
- `float(value: float)` — Convert a floating-point number to an array.
  Convert an incoming floating-point number to an array, then process as described for the array message.
- `list(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
- `anything(list-value: list)` — Convert a list to an array.
  Convert an incoming list to an array, then process as described for the array message.
  In the middle inlet, a symbol/list can be provided as a new re. In the right inlet, a symbol/list can be provided as a substitute.
- `array(ARG_NAME_0: list)` — Process an array using PERL-Compatible Regular Expressions (PCRE)
  In the left inlet, an array will be processed as the subject array by the PCRE engine, using any re and substitute provided.
- `dictionary(ARG_NAME_0: list)` — TEXT_HERE
- `string(ARG_NAME_0: list)` — TEXT_HERE

## Attributes

- `@re` (symbol) — Regular expression
  The PCRE reference

 is the best place to learn more about how regular expressions in Max are built, but here is a quick summary of the basics:

 The word re, followed by a PERL-compatible regular expression, sets the regular expression rules to be used when parsing or making substitutions within any symbol or list input.

 If a regular expression contains spaces, it must be enclosed within double quotes when specified using the re message or as a typed-in argument to the array.regexp object.

 Regular expressions use the following form and syntax:

 [...] defines a 'class' of characters. any of the characters within it may be matched. several special symbols may also appear within it:

 ...-... specifies a range (within ASCII codes)

 \\d specifies a decimal digit (\\D specifies a non-decimal digit). Note that double backslashes must be used -- Max erases single backslashes.

 \\s specifies white space (\\S specifies non-white space). Note that double backslashes must be used -- Max erases single backslashes.

 \\w specifies an alphanumeric (\\W specifies a nonalphanumeric). Note that double backslashes must be used -- Max erases single backslashes.

 ^... specifies a complement of

 ...* appears zero times

 ...+ appears at least once

 ...? appears once or not at all

 (...) specifies a capture group that may be referred to in a substitution array such as %n, where n is the position of the parenthesis in left-to-right order.
- `@substitute` (atom) — Substitution array
  The word substitute, followed by a list of integer values between 0 and 255, specifies an array to be used in substitutions. If the word substitute is not followed by any values, the previous substitution symbol is removed. The word can also specify that no substitution should occur. The word emptyarray indicates that substitution should occur, but that the matched values should be deleted from the final output array.

 Note: If you need to output a % followed by a number in any substitution array, you should use %% (encoded: 37 37) , so that the % is not interpreted as a capture group.

## Help patcher examples

> **Documented but not demonstrated in any help-patcher example:**
> - `in1` — re (regular expression) in
> - `in2` — substitute in

### basic

```
Example #1 — [array.regexp @re (.)\\02\\03\\04 @substitute %1 %1 %1 37 49]  Like the classic 'regexp' object, you can use capture groups inside of substitutions as array elements, either directly as in 'regexp', e.g. %1, or encoded as bytes, as [37, 49] (37 = '%', 49 = '1'). / offsets / unmatched / subarrays / capture groups / substitutions
  fan-in:
    in0 ← [message "1 2 3 4 2 2 3 4 3 2 3 4 4 2 3 4"]    # Input text containing matches
  fan-out:
    out0 → [print regexp-substitutions regexp-capturegroups regexp-subarrays regexp-unmatched regexp-offsets @popup 1]:in0    # offsets of matched subarrays (array) / unmatched array (array) / matched subarrays (array) / capture groups are enclosed in parentheses (nested array) / array with substitutions (array)
    out1 → [print regexp-substitutions regexp-capturegroups regexp-subarrays regexp-unmatched regexp-offsets @popup 1]:in1    # offsets of matched subarrays (array) / unmatched array (array) / matched subarrays (array) / capture groups are enclosed in parentheses (nested array) / array with substitutions (array)
    out2 → [print regexp-substitutions regexp-capturegroups regexp-subarrays regexp-unmatched regexp-offsets @popup 1]:in2    # offsets of matched subarrays (array) / unmatched array (array) / matched subarrays (array) / capture groups are enclosed in parentheses (nested array) / array with substitutions (array)
    out3 → [print regexp-substitutions regexp-capturegroups regexp-subarrays regexp-unmatched regexp-offsets @popup 1]:in3    # offsets of matched subarrays (array) / unmatched array (array) / matched subarrays (array) / capture groups are enclosed in parentheses (nested array) / array with substitutions (array)
    out4 → [print regexp-substitutions regexp-capturegroups regexp-subarrays regexp-unmatched regexp-offsets @popup 1]:in4    # offsets of matched subarrays (array) / unmatched array (array) / matched subarrays (array) / capture groups are enclosed in parentheses (nested array) / array with substitutions (array)
```

```
Example #2 — [array.regexp \\0+\\02]
  fan-in:
    in0 ← [message "1 0 1 0 2 0 3 0 4 0 3 0 2 0 1 0 0 2"]    # A simple argument (with no metacharacters): / Input array for processing.
  fan-out:
    out2 → [print regexp-subarrays @popup 1]:in0    # list of matches
```

## See also

`fromsymbol`, `key`, `keyup`, `message`, `regexp`, `spell`, `tosymbol`, `array`, `string.regexp`
