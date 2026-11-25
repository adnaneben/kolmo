#!/usr/bin/env bash
# Add only absent assets to a draft, then verify the full release by downloading it.
set -euo pipefail

version=$1
commit=$2
tag="v$version"
repo=$GITHUB_REPOSITORY
ref="refs/tags/$tag"
assets=(
    "kolmo-$version.tar.gz"
    "kolmo-$version-linux-x86_64.tar.gz"
    "kolmo-$version-linux-arm64.tar.gz"
    "kolmo-$version-macos-arm64.tar.gz"
    SHA256SUMS
)

check_tag() {
    local remote_commit
    # A lightweight tag has one result; an annotated tag ends in a peeled commit.
    remote_commit=$(git -C source ls-remote origin "$ref" "$ref^{}" |
        awk -v ref="$ref" '$2 == ref {tag = $1} $2 == (ref "^{}") {peeled = $1} END {print (peeled != "" ? peeled : tag)}')
    if [[ "$remote_commit" != "$commit" ]]; then
        echo "$tag does not currently point to the verified source commit $commit" >&2
        exit 1
    fi
}

view() {
    gh release view "$tag" --repo "$repo" --json isDraft,tagName,assets
}

check_tag
if ! release=$(view); then
    gh release create "$tag" --repo "$repo" --draft --verify-tag \
        --title "kolmo $version" --notes "Draft prepared by the release workflow."
    release=$(view)
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

expected=$(printf '%s\n' "${assets[@]}" | jq -R . | jq -s 'sort')
found=$(jq -c '[.assets[].name] | sort' <<< "$release")
if ! jq -e --arg tag "$tag" --argjson expected "$expected" '
     .tagName == $tag and
     ([.assets[].name] | length == (unique | length) and . - $expected == [])
   ' <<< "$release" >/dev/null; then
    echo "$tag contains unexpected or duplicate assets" >&2
    exit 1
fi
missing=()
for asset in "${assets[@]}"; do
    if jq -e --arg asset "$asset" 'index($asset) != null' <<< "$found" >/dev/null; then
        gh release download "$tag" --repo "$repo" --pattern "$asset" --dir "$tmp"
        if ! cmp -s "dist/$asset" "$tmp/$asset"; then
            echo "Existing asset $asset differs from the verified build" >&2
            exit 1
        fi
    else
        missing+=("dist/$asset")
    fi
done

if (( ${#missing[@]} )); then
    check_tag
    release=$(view)
    if ! jq -e --argjson before "$found" '.isDraft and ([.assets[].name] | sort) == $before' <<< "$release" >/dev/null; then
        echo "$tag is public or its assets changed during verification" >&2
        exit 1
    fi
    gh release upload "$tag" --repo "$repo" "${missing[@]}"
fi

check_tag
if ! view | jq -e --argjson expected "$expected" '([.assets[].name] | sort) == $expected' >/dev/null; then
    echo "$tag does not contain the complete verified asset set" >&2
    exit 1
fi
verified="$tmp/verified"
mkdir "$verified"
gh release download "$tag" --repo "$repo" --dir "$verified"
cmp "dist/SHA256SUMS" "$verified/SHA256SUMS"
(cd "$verified" && sha256sum -c SHA256SUMS)
echo "Verified the uploaded assets for $tag at $commit."
