#!/bin/bash
# One-time setup on macOS (Apple silicon). Safe to run again: it only installs what is missing.
#   bash mac/setup.sh
set -euo pipefail
cd "$(dirname "$0")/.."

echo "== Xcode command line tools"
if ! xcode-select -p >/dev/null 2>&1; then
    xcode-select --install || true
    echo "A macOS dialog is installing the command line tools. Run this script again when it finishes."
    exit 0
fi

echo "== Homebrew"
if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew is not installed. Install it from https://brew.sh (one command), then run this script again."
    exit 1
fi

echo "== Libraries and reference tools (brotli, xz, zstd, lz4)"
brew list brotli >/dev/null 2>&1 || brew install brotli
brew list xz     >/dev/null 2>&1 || brew install xz
brew list zstd   >/dev/null 2>&1 || brew install zstd
brew list lz4    >/dev/null 2>&1 || brew install lz4

echo "== Python 3 and MLX (only needed for the phase-4 neural tier; skipped if it fails)"
if command -v python3 >/dev/null 2>&1; then
    python3 -m pip install --user --quiet mlx numpy 2>/dev/null || echo "   (MLX not installed now; not needed until phase 4)"
fi

echo "== Datasets"
mkdir -p data
if [ -f data_small.tar.xz ] && [ ! -f data/owid-co2-data.csv ]; then tar -xJf data_small.tar.xz -C data; fi
if [ ! -f data/enwik8 ]; then
    echo "   downloading enwik8 (100 MB text benchmark)"
    curl -fL -o data/enwik8.zip http://mattmahoney.net/dc/enwik8.zip && (cd data && unzip -o -q enwik8.zip && rm enwik8.zip)
fi
if [ ! -f data/silesia.tar ]; then
    echo "   downloading the Silesia corpus (212 MB mixed files)"
    curl -fL -o data/silesia.zip https://sun.aei.polsl.pl//~sdeor/corpus/silesia.zip && \
        (cd data && mkdir -p silesia && unzip -o -q silesia.zip -d silesia && tar -cf silesia.tar -C silesia . && rm -rf silesia silesia.zip)
fi

echo "== Build and self-test"
make clean >/dev/null; make -j"$(sysctl -n hw.ncpu)"
./build/czntest | tail -3
echo
echo "Setup done. Next: bash mac/bench.sh"
