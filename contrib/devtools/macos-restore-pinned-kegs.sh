#!/usr/bin/env bash
#
# Restore pinned Homebrew kegs for Intel macOS from the project's S3 mirror.
#
# WHY THIS EXISTS
#
# Homebrew has wound x86_64 macOS down to Tier 3. Five of the formulae this
# build needs -- qtbase, qttools, qtsvg, qttranslations, qtdeclarative -- plus
# openssl@3 now ship NO Intel bottle at any macOS tag, so `brew install` builds
# them from source. That is hours per run, and it is why the Intel job reached
# the six-hour job ceiling without ever starting a compile.
#
# Everything else we need still has a `sonoma` Intel bottle, and Homebrew pours
# those on a newer macOS quite happily (measured: libevent in 9s on Sequoia), so
# only the six bottle-less formulae are mirrored. Those bottles are built on
# macOS 14 and carry minos 14.0, which is the same floor as the mirrored kegs.
#
# The kegs are built once on the project's macOS 14 Intel VM and uploaded to the
# same S3 bucket the depends system already uses as a source fallback. Versions
# are PINNED, as in depends: upgrades happen when someone rebuilds and re-uploads,
# not when Homebrew moves. Nothing here needs AWS credentials -- the objects are
# world-readable, and only publishing requires a signed request.
#
# Kegs contain absolute paths (/usr/local/Cellar, /usr/local/opt), so the VM must
# use the Intel prefix and CI must run a native Intel runner. Building on macOS 14
# and running on macOS 15 is the supported direction; the reverse is not.
#
# WHAT IT DOES, AND WHY IN THIS ORDER
#
#   1. Install the external dependency closure -- the closure of the pinned set
#      MINUS the pinned set itself. Naming a pinned formula here is what breaks
#      the pin: `brew install --only-dependencies qttools` will happily upgrade
#      qtbase, because qtbase is both pinned and a dependency.
#   2. Restore each pinned keg, then PRUNE every other version of it. Step 1 (or
#      an unrelated preinstalled package -- pipx did this) can drag a pinned
#      formula forward transitively, leaving two versions in the Cellar. Pruning
#      leaves `brew link` nothing to choose between.
#   3. Link, then `brew pin`, so a later `brew upgrade` cannot undo any of it.
#
# usage: macos-restore-pinned-kegs.sh <manifest>
# env:   BREW  brew executable (default: the one on PATH)

export LC_ALL=C
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <manifest>" >&2
    exit 2
fi

MANIFEST="$1"
BREW="${BREW:-$(command -v brew || true)}"
S3_BASE="${S3_BASE:-https://gridcoin-depends-backup-sources.s3.us-east-1.amazonaws.com/homebrew-kegs}"

if [ -z "${BREW}" ] || [ ! -x "${BREW}" ]; then
    echo "ERROR: no usable brew (BREW='${BREW}')" >&2
    exit 1
fi
if [ ! -f "${MANIFEST}" ]; then
    echo "ERROR: manifest not found: ${MANIFEST}" >&2
    exit 1
fi

CELLAR="$("${BREW}" --cellar)"
PREFIX="$("${BREW}" --prefix)"
mkdir -p "${CELLAR}"

# The mirror is keyed by what the kegs were BUILT on, which is what governs
# compatibility -- not by what they are restored onto.
BUILT_ON="$(grep -E '^built_on[[:space:]]' "${MANIFEST}" | awk '{print $2}')"
if [ -z "${BUILT_ON}" ]; then
    echo "ERROR: manifest has no 'built_on' line" >&2
    exit 1
fi

NAMES=() VERSIONS=()
while read -r name version; do
    case "${name}" in ''|'#'*|built_on) continue ;; esac
    NAMES+=("${name}")
    VERSIONS+=("${version}")
done < <(sed -E 's/#.*//' "${MANIFEST}")

if [ "${#NAMES[@]}" -eq 0 ]; then
    echo "ERROR: manifest lists no formulae" >&2
    exit 1
fi

echo "pinned (${BUILT_ON}): ${NAMES[*]}"

# ---------------------------------------------------------------------------
# 1. External dependencies only.
# ---------------------------------------------------------------------------
pinned_list="$(printf '%s\n' "${NAMES[@]}")"
external="$("${BREW}" deps --union "${NAMES[@]}" | grep -vxF -e "${pinned_list}" || true)"

if [ -n "${external}" ]; then
    echo "installing external dependencies:"
    # shellcheck disable=SC2086  # deliberate word splitting, one formula per line
    printf '  %s\n' ${external}
    # shellcheck disable=SC2086  # deliberate word splitting of the formula list
    "${BREW}" install ${external}
else
    echo "no external dependencies to install"
fi

# ---------------------------------------------------------------------------
# 2. Restore each pinned keg and prune every other version of it.
# ---------------------------------------------------------------------------
for i in "${!NAMES[@]}"; do
    name="${NAMES[$i]}"
    version="${VERSIONS[$i]}"
    url="${S3_BASE}/${BUILT_ON}/${name}--${version}.tar.gz"

    echo "--- ${name} ${version}"
    tmp="$(mktemp)"
    if ! curl -fsSL "${url}" -o "${tmp}"; then
        rm -f "${tmp}"
        echo "ERROR: no mirrored keg at ${url}" >&2
        echo "       Build it on the macOS ${BUILT_ON} Intel VM and upload it, or" >&2
        echo "       correct the version in ${MANIFEST}. Falling back to a source" >&2
        echo "       build here would take hours and is deliberately not done." >&2
        exit 1
    fi
    tar -xzf "${tmp}" -C "${CELLAR}"
    rm -f "${tmp}"

    for d in "${CELLAR}/${name}"/*; do
        [ -e "${d}" ] || continue
        if [ "$(basename "${d}")" != "${version}" ]; then
            echo "    pruning $(basename "${d}") (pulled in transitively)"
            rm -rf "${d}"
        fi
    done
done

# ---------------------------------------------------------------------------
# 3. Link, then pin.
# ---------------------------------------------------------------------------
for name in "${NAMES[@]}"; do
    # Plain link first; --force only where required (keg-only formulae such as
    # openssl@3), since force-linking plants headers ahead of the SDK.
    if ! "${BREW}" link --overwrite "${name}" >/dev/null 2>&1; then
        "${BREW}" link --overwrite --force "${name}" >/dev/null
    fi
    "${BREW}" pin "${name}" >/dev/null 2>&1 || true
done

# ---------------------------------------------------------------------------
# Verify, rather than assume. A wrong version here is a silently mis-targeted
# release build, so this is a hard failure.
# ---------------------------------------------------------------------------
fail=0
for i in "${!NAMES[@]}"; do
    name="${NAMES[$i]}"
    version="${VERSIONS[$i]}"
    present=""
    for d in "${CELLAR}/${name}"/*; do
        [ -e "${d}" ] || continue
        present="${present:+${present} }$(basename "${d}")"
    done
    linked="$(basename "$(readlink "${PREFIX}/opt/${name}" 2>/dev/null || echo none)")"
    printf '%-16s cellar=[%s] linked=%s\n' "${name}" "${present}" "${linked}"
    [ "${present}" = "${version}" ] || { echo "  ERROR: cellar should hold only ${version}" >&2; fail=1; }
    [ "${linked}" = "${version}" ]  || { echo "  ERROR: linked version should be ${version}" >&2; fail=1; }
done
exit "${fail}"
