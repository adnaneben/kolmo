#!/bin/sh
# Two versions of the crate in one dependency graph must be refused (Cargo.toml `links = "kolmo"`): otherwise both
# would link one copy of the C core, silently (bug 12 of the review40 synthesis). Resolves a graph holding this crate
# and a copy of its manifest under another version, and expects cargo to refuse it. Nothing is compiled.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
t=$(mktemp -d); trap 'rm -rf "$t"' EXIT
mkdir -p "$t/other/src" "$t/app/src"
sed 's/^version = ".*"/version = "999.0.0"/' "$here/Cargo.toml" > "$t/other/Cargo.toml"
: > "$t/other/src/lib.rs"; echo 'fn main() {}' > "$t/other/build.rs"; echo 'fn main() {}' > "$t/app/src/main.rs"
cat > "$t/app/Cargo.toml" <<TOML
[package]
name = "links-check"
version = "0.0.0"
edition = "2021"
[dependencies]
ka = { package = "kolmo", path = "$here" }
kb = { package = "kolmo", path = "$t/other" }
TOML
if out=$(cargo metadata --format-version 1 --manifest-path "$t/app/Cargo.toml" 2>&1 >/dev/null); then
    echo "links-check: FAIL: cargo accepted two versions of kolmo in one graph"; exit 1
fi
case $out in
    *'links = "kolmo"'*|*"links value"*) echo "links-check: ok, cargo refuses two versions of kolmo in one graph" ;;
    *) echo "links-check: FAIL: cargo failed for another reason:"; echo "$out"; exit 1 ;;
esac
