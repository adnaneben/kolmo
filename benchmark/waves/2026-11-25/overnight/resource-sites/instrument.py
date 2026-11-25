"""Apply test-only named allocation hooks to a fresh copy of the candidate source."""
import csv
import re
import sys
from pathlib import Path

tree = Path(sys.argv[1])
evidence = Path(__file__).resolve().parent
sets = {
    "checked": ("src/core/compress.c", "src/core/blocks.c", "src/core/decode.c"),
    "direct": ("src/core/dedup.c", "src/core/stream.c", "src/core/extension.c", "src/api/api.c",
               "src/core/compress.c", "src/core/blocks.c", "src/core/decode.c"),
}
originals = {path: (tree / path).read_text().splitlines() for paths in sets.values() for path in paths}
rows = []
for group, paths in sets.items():
    names = {"checked": r"array_alloc|array_resize", "direct": r"malloc|calloc|realloc"}[group]
    for path in paths:
        target = tree / path
        lines = target.read_text().splitlines(keepends=True)
        for index, line in enumerate(lines):
            counter = [0]
            def mark(match):
                counter[0] += 1
                operator = match.group(1)
                label = f"{path}:{index + 1}:{counter[0]}"
                rows.append((label, group, operator, originals[path][index].strip()))
                return f'b60_{operator}("{label}", '
            lines[index] = re.sub(r"\b(" + names + r")\(", mark, line)
        target.write_text("".join(lines))
with (evidence / "instrumented-sites.csv").open("w", newline="") as file:
    writer = csv.writer(file)
    writer.writerow(("site", "mechanism", "allocator", "original_line"))
    writer.writerows(rows)
p = tree / "src/kolmo.c"
s = p.read_text().replace('#undef free', '#undef free\n#include "b60-hooks.h"')
p.write_text(s)
(tree / "src/b60-hooks.h").write_bytes((evidence / "b60-hooks.h").read_bytes())
