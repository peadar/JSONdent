# JSONdent

## Tiny JSON indenter using a streaming C++ sax-like parser.

Unlike the python json.tool indenter, this preserves the ordering of
items from the input.

As far as tested, the output is byte-for-byte identical to python's
"json.tool" as long as there's no non-integer numbers. 

By default this tool parses numbers as integers. You can pass "-f" to
make it parse floats too, but there are likely to be rounding differences
in the output.
