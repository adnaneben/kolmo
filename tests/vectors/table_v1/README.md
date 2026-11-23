# Table v1 (tflag 3) test vectors

No 0.0.1 or 0.0.2 encoder writes tflag 3, so the frozen corpus has no frame that uses it. Each `NAME.t` here is the
table-v1 transform of `NAME.csv` (delimiter `,`), written by `tbl_encode` of the 0.0.2 sources
(`src/transforms/table_v03.c`). The C decoder `tbl_decode` was checked to give `NAME.csv` back. `tests/coder_reference.py`
decodes each `NAME.t` by FORMAT.md 20.9 and compares the result with `NAME.csv`.

`quoted.csv` was transformed with quote-aware field splitting, and `nonl.csv` ends without a newline.
