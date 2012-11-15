JSONdent
========

Tiny JSON indenter using a streaming C++ sax-like parser.

Unlike the python json.tool indenter, this preserves the ordering of
items from the input. For a limited subset, its output appears the same
as that tools on the same input.
