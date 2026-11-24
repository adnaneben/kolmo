#!/bin/sh
# The library must never stop the host process (SECURITY.md): no abort(), exit() or assert() in the code built into
# libkolmo (src/ without the CLI and the research tools). Prints the offending lines and fails if any is found.
cd "$(dirname "$0")/.." || exit 1
hits=$(grep -rnE '(^|[^_a-zA-Z0-9.>])(abort|exit|_Exit|quick_exit|assert)[[:space:]]*\(' src \
         --include='*.c' --include='*.h' --include='*.inc' \
       | grep -v '^src/cli/' | grep -v '^src/experimental/' | grep -vE '^[^:]+:[0-9]+:[[:space:]]*(/\*|\*|//)' \
       | grep -vE 'static_assert|_Static_assert')
if [ -n "$hits" ]; then echo "$hits"; echo "check_no_abort: FAILED (the library must return errors)"; exit 1; fi
echo "check_no_abort: PASSED (no abort, exit or assert in the library)"
