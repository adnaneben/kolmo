#!/bin/sh
# Recreate the two unmodified source trees and stock/research executables in an empty destination.
set -eu
repo=${1:?path to the Kolmo Git repository}
dest=${2:?path to a new build directory}
mkdir "$dest"
for variant in main rc; do
    case "$variant" in
        main) commit=a1d39559686a9ec17c8614b9154a64d7b5b98672 ;;
        rc) commit=a2e430fed2fbc6dc357a78651cadf0e1fed611f2 ;;
    esac
    mkdir "$dest/$variant"
    git -C "$repo" archive "$commit" Makefile experimental.mk src benchmark/impact/run.py |
        tar xf - -C "$dest/$variant"
    make -C "$dest/$variant" -j3 build/kolmo kolmo-impact
done
