#!/usr/bin/env bash
# Build a 3.0 (quilt) source package from the in-tree debian/ + working tree.
#
# The orig tarball is generated from the source tree MINUS the packaging dir(s)
# (debian/ and any debian-* variant), so the quilt delta is debian/ only -> zero
# source patches. This is the CI equivalent of the old hand-run build-one.sh.
#
# Works for whichever variant's packaging is currently in debian/: the caller
# activates testnet by mangling debian-testnet/ -> debian/ first; this script
# just reads the *active* debian/changelog.
#
# Usage: build-source-package.sh [OUTDIR]   (default OUTDIR=./obs-out)
export LC_ALL=C
set -euo pipefail

OUTDIR="${1:-$PWD/obs-out}"

command -v dpkg-source >/dev/null || { echo "::error::dpkg-source not found (install dpkg-dev)"; exit 1; }
[ -f debian/changelog ] || { echo "::error::no debian/changelog in $PWD"; exit 1; }

SRC=$(dpkg-parsechangelog -SSource)
FULLVER=$(dpkg-parsechangelog -SVersion)   # e.g. 5.5.1.0-3
UPVER=${FULLVER%-*}                          # strip -<debian_revision> -> 5.5.1.0
PREFIX="${SRC}-${UPVER}"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# Snapshot the ACTUAL working tree, excluding VCS and build output. This must
# capture untracked files too: the testnet variant-mangle copies debian-testnet/
# into debian/, and the testnet-only files (gridcointestnet-*.{desktop,png,
# install,service,...}) are untracked there. git stash/archive would silently
# drop them and the build would fail on a missing debian/ file.
mkdir -p "$WORK/${PREFIX}"
tar -c --exclude=./.git --exclude=./obs-out . | tar -x -C "$WORK/${PREFIX}"

# orig = upstream source = tree minus the packaging dir(s)
tar caf "$WORK/${SRC}_${UPVER}.orig.tar.xz" \
  --exclude="${PREFIX}/debian" \
  --exclude="${PREFIX}/debian-testnet" \
  -C "$WORK" "${PREFIX}"

( cd "$WORK" && dpkg-source -b "${PREFIX}" )

mkdir -p "$OUTDIR"
mv "$WORK"/*.dsc "$WORK"/*.orig.tar.* "$WORK"/*.debian.tar.* "$OUTDIR"/
echo "Source package (${SRC} ${FULLVER}) in ${OUTDIR}:"
ls -1 "$OUTDIR"
