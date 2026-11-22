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
# Requires dpkg-dev and GNU sed.
export LC_ALL=C
set -euo pipefail

OUTDIR="${1:-$PWD/obs-out}"

command -v dpkg-source >/dev/null || { echo "::error::dpkg-source not found (install dpkg-dev)"; exit 1; }
[ -f debian/changelog ] || { echo "::error::no debian/changelog in $PWD"; exit 1; }

SRC=$(dpkg-parsechangelog -SSource)
FULLVER=$(dpkg-parsechangelog -SVersion)   # e.g. 5.5.1.0-3
UPVER=${FULLVER#*:}                          # strip an epoch, if any
UPVER=${UPVER%-*}                            # strip -<debian_revision> -> 5.5.1.0
PREFIX="${SRC}-${UPVER}"

# The package version comes from the top changelog entry, which the release step
# writes (contrib/devtools/debian-changelog-release.sh). An entry still marked
# UNRELEASED, or one for a version other than the one in CMakeLists.txt, would
# label the build with the wrong version, so the build stops here instead.
# The project() call whose name is exactly Gridcoin, up to its closing parenthesis,
# with or without quotes and surrounding whitespace; the first VERSION line inside it.
# Same expression as in contrib/devtools/debian-changelog-release.sh.
CMAKEVER="$(sed -n '/^[[:space:]]*project([[:space:]]*"\{0,1\}Gridcoin"\{0,1\}\([[:space:]]\|$\)/,/^[[:space:]]*)/s/^[[:space:]]*VERSION[[:space:]]\{1,\}"\{0,1\}\([^"[:space:]]*\)"\{0,1\}[[:space:]]*$/\1/p' CMakeLists.txt)"
CMAKEVER="${CMAKEVER%%
*}"
if [ -z "$CMAKEVER" ]; then
  echo "::error::no VERSION found in the project(Gridcoin ...) call in CMakeLists.txt"
  exit 1
fi
DIST=$(dpkg-parsechangelog -SDistribution)
if [ "$DIST" = UNRELEASED ]; then
  echo "::error::the top debian/changelog entry (${FULLVER}) is UNRELEASED while CMakeLists.txt is at ${CMAKEVER}; run contrib/devtools/debian-changelog-release.sh in the release commit"
  exit 1
fi
if [ "$UPVER" != "$CMAKEVER" ]; then
  echo "::error::debian/changelog is at upstream version ${UPVER} but CMakeLists.txt is at ${CMAKEVER}; run contrib/devtools/debian-changelog-release.sh in the release commit"
  exit 1
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# Snapshot the ACTUAL working tree, excluding VCS and build output. This must
# capture untracked files too: the testnet variant-mangle copies debian-testnet/
# into debian/, and the testnet-only files (gridcointestnet.desktop, the
# gridcointestnet*.install files, the maintainer scripts, ...) are untracked
# there. git stash/archive would silently drop them and the build would fail on
# a missing debian/ file.
mkdir -p "$WORK/${PREFIX}"
tar -c --exclude=./.git --exclude=./obs-out . | tar -x -C "$WORK/${PREFIX}"

# orig = upstream source = tree minus the packaging dir(s)
tar caf "$WORK/${SRC}_${UPVER}.orig.tar.xz" \
  --exclude="${PREFIX}/debian" \
  --exclude="${PREFIX}/debian-testnet" \
  -C "$WORK" "${PREFIX}"

# Drop the inactive sibling packaging dir from the source tree before dpkg-source.
# The orig tarball already excludes it (above); dpkg-source -b must not see it either,
# or it treats the variant's files as local changes to the upstream source and aborts.
# This makes the mainnet build symmetric with the testnet path, where the caller has
# already renamed it into debian/.
rm -rf "$WORK/${PREFIX}/debian-testnet"

( cd "$WORK" && dpkg-source -b "${PREFIX}" )

mkdir -p "$OUTDIR"
mv "$WORK"/*.dsc "$WORK"/*.orig.tar.* "$WORK"/*.debian.tar.* "$OUTDIR"/
echo "Source package (${SRC} ${FULLVER}) in ${OUTDIR}:"
ls -1 "$OUTDIR"
