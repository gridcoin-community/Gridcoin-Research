#!/usr/bin/env bash
# Add the "New upstream release." entry for the version in CMakeLists.txt to the
# Debian changelogs, as part of the release step.
#
# The version is MAJOR.MINOR.PATCH.VARIANT. VARIANT 0 is a mainnet release and
# gets an entry in both debian/changelog and debian-testnet/changelog; any other
# VARIANT is a testnet release and gets one in debian-testnet/changelog only.
# A changelog whose top entry already has this version is left as it is, except
# that an UNRELEASED entry is finalized for unstable, so the script can be run
# again safely. An UNRELEASED top entry with an older version is promoted to the
# release version, keeping its notes. It does not commit.
#
# For a mainnet release, if writing the second changelog fails after the first
# was written, fix the cause and run the script again: the first changelog is
# then already at the release version and left alone, and the second is written.
# dch writes the new changelog to changelog.dch and then copies it over the
# changelog, so after an interruption changelog.dch may be the only complete
# copy. The script never removes it, and refuses to start while one exists:
# compare it with the changelog, keep whichever is complete, remove the .dch
# and run again.
#
# Usage: contrib/devtools/debian-changelog-release.sh
#
# Each entry is signed with the Maintainer field of the control file next to
# its changelog. Requires dch (devscripts) and dpkg-parsechangelog (dpkg-dev).
export LC_ALL=C
set -euo pipefail

usage() {
    sed -n '2,/^export LC_ALL=C$/s/^# \{0,1\}//p' "$0"
}

if [ "$#" -gt 0 ]; then
    case "$1" in
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 1 ;;
    esac
fi

for tool in dch dpkg-parsechangelog; do
    command -v "$tool" >/dev/null || { echo "$0: $tool not found (install devscripts and dpkg-dev)" >&2; exit 1; }
done

TOPDIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$TOPDIR"

VERSION="$(sed -n '/^project("Gridcoin"$/,/^)/s/^[[:space:]]*VERSION[[:space:]]\{1,\}\([^[:space:]]*\)[[:space:]]*$/\1/p' CMakeLists.txt)"
if [ -z "$VERSION" ]; then
    echo "$0: no VERSION found in the project(\"Gridcoin\" call in CMakeLists.txt" >&2
    exit 1
fi
if ! printf '%s\n' "$VERSION" | grep -Eqx '[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+'; then
    echo "$0: VERSION in CMakeLists.txt is not MAJOR.MINOR.PATCH.VARIANT: $VERSION" >&2
    exit 1
fi

VARIANT="${VERSION##*.}"
if [ "$VARIANT" -eq 0 ]; then
    echo "$VERSION is a mainnet release"
    PKGDIRS=(debian debian-testnet)
else
    echo "$VERSION is a testnet release"
    PKGDIRS=(debian-testnet)
fi

for pkgdir in "${PKGDIRS[@]}"; do
    if [ -e "$pkgdir/changelog.dch" ]; then
        echo "$0: $pkgdir/changelog.dch exists, left by an interrupted dch run, and may be the only complete copy: compare it with $pkgdir/changelog, keep the complete one, remove the .dch and run again" >&2
        exit 1
    fi
done

# dch --controlmaint reads debian/control relative to the working directory, so
# each packaging directory is presented to dch as debian/ through a symlink.
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# dch runs as a child so that an interrupted run can wait for it to stop before
# removing the temporary directory dch is running in.
DCH_PID=""
CURRENT=""
on_signal() {
    trap '' INT TERM HUP
    if [ -n "$DCH_PID" ]; then
        wait "$DCH_PID" 2>/dev/null || true
        if [ -e "$TOPDIR/$CURRENT/changelog.dch" ]; then
            echo "$0: interrupted while writing $CURRENT/changelog; $CURRENT/changelog.dch may be the only complete copy: compare the two, keep the complete one, remove the .dch and run again" >&2
        else
            echo "$0: interrupted while writing $CURRENT/changelog; check it and run again" >&2
        fi
    else
        echo "$0: interrupted" >&2
    fi
    exit 130
}
trap on_signal INT TERM HUP

dch_in() {
    CURRENT="$1"
    shift
    echo "$CURRENT/changelog: writing"
    ln -sfn "$TOPDIR/$CURRENT" "$WORKDIR/debian"
    (cd "$WORKDIR" && exec dch --controlmaint --changelog debian/changelog "$@" </dev/null) &
    DCH_PID=$!
    wait "$DCH_PID"
    DCH_PID=""
}

for pkgdir in "${PKGDIRS[@]}"; do
    changelog="$pkgdir/changelog"
    top="$(dpkg-parsechangelog -l "$changelog" -SVersion)"
    top_upstream="${top#*:}"
    top_upstream="${top_upstream%-*}"
    if [ "$top_upstream" = "$VERSION" ]; then
        if [ "$(dpkg-parsechangelog -l "$changelog" -SDistribution)" = UNRELEASED ]; then
            dch_in "$pkgdir" --release --distribution unstable --force-distribution ""
            echo "$changelog: finalized the UNRELEASED $top entry for unstable"
        else
            echo "$changelog: already has $VERSION at the top, left unchanged"
        fi
        continue
    fi
    dch_in "$pkgdir" --newversion "$VERSION-1" \
        --distribution unstable --force-distribution "New upstream release."
    echo "$changelog: added $VERSION-1"
done
