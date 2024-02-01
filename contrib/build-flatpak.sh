#!/usr/bin/env bash
export LC_ALL=C
set -e
SECONDS=0

repo_root="$(realpath $(dirname "$0")/..)"
mkdir -p "$repo_root/build"
cd "$repo_root/build"

echo $repo_root
flatpak-builder flatpak-build "$repo_root/contrib/flatpak-manifest.json" --force-clean
flatpak build-export gridcoin-flatpak flatpak-build
flatpak build-bundle gridcoin-flatpak Gridcoin-Research.flatpak us.gridcoin.Gridcoin-Research

echo "Took $SECONDS seconds to build the Flatpak bundle."
