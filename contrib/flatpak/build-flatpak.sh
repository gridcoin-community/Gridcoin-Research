#!/usr/bin/env bash
export LC_ALL=C
#
# Build a Gridcoin Flatpak bundle locally.
#
# Prerequisites:
#   flatpak install flathub org.kde.Platform//6.11 org.kde.Sdk//6.11
#   sudo apt install flatpak-builder   # or equivalent
#
# Usage:
#   contrib/flatpak/build-flatpak.sh
#
# Outputs:
#   gridcoin-<VERSION>-x86_64.flatpak in the repo root

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

MANIFEST="${SCRIPT_DIR}/us.gridcoin.GridcoinResearch.yml"
APP_ID="us.gridcoin.GridcoinResearch"
BUILD_DIR="${REPO_ROOT}/flatpak-build"
REPO_DIR="${REPO_ROOT}/flatpak-repo"

# Extract version from CMakeLists.txt
VERSION=$(sed -n 's/^[[:space:]]*VERSION \([0-9.]*\)$/\1/p' "${REPO_ROOT}/CMakeLists.txt")
if [ -z "$VERSION" ]; then
    echo "Error: Could not extract version from CMakeLists.txt"
    exit 1
fi

ARCH=$(flatpak --default-arch)
BUNDLE_NAME="gridcoin-${VERSION}-${ARCH}.flatpak"

echo "=== Gridcoin Flatpak Builder ==="
echo "Version:  ${VERSION}"
echo "Arch:     ${ARCH}"
echo "Manifest: ${MANIFEST}"
echo ""

# Build
echo "Building Flatpak..."
flatpak-builder \
    --force-clean \
    --repo="${REPO_DIR}" \
    "${BUILD_DIR}" \
    "${MANIFEST}"

# Bundle
echo "Creating bundle: ${BUNDLE_NAME}"
flatpak build-bundle \
    "${REPO_DIR}" \
    "${REPO_ROOT}/${BUNDLE_NAME}" \
    "${APP_ID}"

echo ""
echo "Done! Bundle: ${REPO_ROOT}/${BUNDLE_NAME}"
echo ""
echo "To install locally:"
echo "  flatpak install ${REPO_ROOT}/${BUNDLE_NAME}"
echo ""
echo "To run:"
echo "  flatpak run ${APP_ID}"
