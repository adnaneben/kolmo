#!/usr/bin/env bash
# Emit the version and the checked-out commit for the release build jobs.
set -euo pipefail

version=$(cat VERSION)
if [[ ! "$version" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
    echo "VERSION must be a release version in X.Y.Z form" >&2
    exit 1
fi
if [[ "$GITHUB_REF_TYPE" == tag && "$GITHUB_REF_NAME" != "v$version" ]]; then
    echo "Tag $GITHUB_REF_NAME disagrees with VERSION $version" >&2
    exit 1
fi
python3 tests/check_release_docs.py
printf 'version=%s\ncommit=%s\n' "$version" "$(git rev-parse HEAD)" >> "$GITHUB_OUTPUT"
