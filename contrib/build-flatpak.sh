#!/usr/bin/env bash
export LC_ALL=C
set -e
SECONDS=0

gc_platform="org.kde.Platform//5.15-23.08"
gc_sdk="org.kde.Sdk//5.15-23.08"
gc_id="us.gridcoin.Gridcoin-Research"
gc_output_file="Gridcoin-Research.flatpak"
gc_repo_root="$(realpath "$(dirname "$0")/..")"
gc_builddir="$gc_repo_root/build"

# Add the required flathub.org repo to the list of local repos, if it wasn't added earlier.
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

# Check if the required SDK and runtime are installed. If not, install them.
flatpak info "$gc_platform" &>/dev/null || flatpak install --noninteractive -y flathub "$gc_platform"
flatpak info "$gc_sdk" &>/dev/null || flatpak install --noninteractive -y flathub "$gc_sdk"

echo "Started building the Gridcoin-Research Flatpak bundle..."
mkdir -p "$gc_builddir"
flatpak-builder --state-dir="$gc_builddir/flatpak-cache" "$gc_builddir/flatpak-build" "$gc_repo_root/contrib/flatpak-manifest.json" --force-clean
flatpak build-export "$gc_builddir/gridcoin-flatpak" "$gc_builddir/flatpak-build"
flatpak build-bundle "$gc_builddir/gridcoin-flatpak" "$gc_builddir/$gc_output_file" "$gc_id"

relative_output="$(realpath --relative-to="$PWD" "$gc_builddir/$gc_output_file")"
echo ""
echo "Took $SECONDS seconds to build the Flatpak bundle. You can find the output in $relative_output"
echo "To install it, run the command below:"
echo "flatpak install $relative_output"
