#!/bin/bash -eu
# Draft for google/oss-fuzz: projects/kolmo/build.sh. Not submitted (see packaging/oss-fuzz/README.md).
# The fuzz targets, their seed corpora and options files are built by the repository's own script, so the target list
# lives with the code: fuzz/build.sh (also run by .github/workflows/fuzz.yml on every pull request).
cd "$SRC/kolmo"
bash fuzz/build.sh
