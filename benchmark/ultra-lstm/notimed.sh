#!/bin/sh
# Stand-in for $V/tools/timed.sh when the caller already holds the timing lock (coordinator quiet-window queue):
# drop the owner tag and run the command.
shift; exec "$@"
