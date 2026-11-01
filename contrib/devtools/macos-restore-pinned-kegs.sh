#!/usr/bin/env bash
#
# Restore pinned Homebrew kegs for Intel macOS from the project's S3 mirror.
#
# WHY THIS EXISTS
#
# Homebrew has wound x86_64 macOS down to Tier 3, and that costs us twice.
#
# COST ONE, TIME. qtbase, qttools, qtsvg, qttranslations, qtdeclarative and
# openssl@3 ship no Intel bottle at any macOS tag, so `brew install` builds them
# from source. That is hours per run, and it is why the Intel job reached the
# six-hour job ceiling without ever starting a compile.
#
# COST TWO, THE FLOOR. Homebrew sets no deployment target, so a binary's minimum
# is whatever machine produced it. A source build on the runner takes the
# RUNNER's macOS (15), and a formula that publishes a bottle for a newer tag
# hands over a newer binary because Homebrew prefers the newest. Both put a 15.0
# library inside a bundle that claims 14.0, and dyld then refuses to load it on
# macOS 14. This was found by measuring a shipped DMG -- pcre2 and xz had been
# source-building quietly, and zstd was pouring its sequoia bottle.
#
# So the pinned set is not only the slow formulae: it is every formula that
# would otherwise put a library needing macOS 15 INSIDE the app bundle. Other
# members of the closure are raised above 14.0 too and are deliberately not
# pinned, because they are never bundled and a library's minimum can only hurt
# a user from inside the bundle. macos-pinned-kegs.txt records which of the two
# reasons applies to each entry, and why the list is not `brew deps` output.
#
# None of this is taken on trust: both macOS CI jobs measure the finished
# bundle's real floor and fail if it exceeds what the bundle declares. That
# check, not this script, is what makes the declared floor true.
#
# KNOWN LIMITATION. The install list below is computed from the CURRENT formula
# definitions while the restored kegs are frozen at the manifest's versions, so
# the two can drift: a dependency rename or soname bump upstream would install
# libraries the kegs do not reference. That surfaces as a link failure or as the
# bundle check above, both loud, and the fix is to refresh the kegs -- which is
# the same maintainer action an upgrade needs anyway.
#
# The kegs are built once by a maintainer on a native Intel macOS 14 machine and
# uploaded to the same S3 bucket the depends system already uses as a source
# fallback. Versions are PINNED, as in depends: upgrades happen when someone
# rebuilds and re-uploads, not when Homebrew moves. Nothing here needs AWS
# credentials -- the objects are world-readable, and only publishing requires a
# signed request.
#
# Kegs contain absolute paths (/usr/local/Cellar, /usr/local/opt), so the build
# machine must use the Intel prefix and CI must run a native Intel runner.
# Building on macOS 14 and running on macOS 15 is the supported direction; the
# reverse is not.
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
# usage: macos-restore-pinned-kegs.sh <manifest> [extra-formula...]
#        extra-formula are additional bottled formulae the build needs. They are
#        installed in step 1 WITH the external closure, never afterwards: a later
#        `brew install` can pull a pinned formula forward through its own
#        dependencies (libevent depends on openssl@3), and doing that after the
#        prune and the verification below would defeat both.
# env:   BREW  brew executable (default: the one on PATH)

export LC_ALL=C
set -euo pipefail

if [ "$#" -lt 1 ]; then
    echo "usage: $0 <manifest> [extra-formula...]" >&2
    exit 2
fi

MANIFEST="$1"
shift
EXTRA=("$@")
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

NAMES=() VERSIONS=() HASHES=()
while read -r name version hash; do
    case "${name}" in ''|'#'*|built_on) continue ;; esac
    if [ -z "${hash}" ]; then
        echo "ERROR: ${name} has no sha256 in ${MANIFEST}" >&2
        exit 1
    fi
    NAMES+=("${name}")
    VERSIONS+=("${version}")
    HASHES+=("${hash}")
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

# EXTRA is filtered through the pinned set too, not just the closure. The workflow
# passes MACOS_BREW_DEPS verbatim and most of those names are now pinned, so
# without this `brew install` would fetch its own copy of a pinned formula
# moments before the restore replaces it: wasted work today, and on the day that
# formula's bottle disappears, the very source build the mirror exists to avoid.
install_list="${external}"
if [ "${#EXTRA[@]}" -gt 0 ]; then
    install_list="${install_list}
$(printf '%s\n' "${EXTRA[@]}" | grep -vxF -e "${pinned_list}" || true)"
fi

if [ -n "${install_list//[[:space:]]/}" ]; then
    echo "installing bottled formulae before any restore:"
    # shellcheck disable=SC2086  # deliberate word splitting, one formula per line
    printf '  %s\n' ${install_list}
    # shellcheck disable=SC2086  # deliberate word splitting of the formula list
    "${BREW}" install ${install_list}
else
    echo "nothing to install"
fi

# ---------------------------------------------------------------------------
# 2. Restore each pinned keg and prune every other version of it.
# ---------------------------------------------------------------------------
for i in "${!NAMES[@]}"; do
    name="${NAMES[$i]}"
    version="${VERSIONS[$i]}"
    want="${HASHES[$i]}"
    url="${S3_BASE}/${BUILT_ON}/${name}--${version}.tar.gz"

    echo "--- ${name} ${version}"
    tmp="$(mktemp)"
    if ! curl -fsSL "${url}" -o "${tmp}"; then
        rm -f "${tmp}"
        echo "ERROR: no mirrored keg at ${url}" >&2
        echo "       Build it on a native Intel macOS ${BUILT_ON%%-*} machine and upload" >&2
        echo "       it, or correct the version in ${MANIFEST}. Falling back to a" >&2
        echo "       source build here would take hours and is deliberately not done." >&2
        exit 1
    fi

    # Verify before extracting, not after: the point is to never unpack an
    # object we cannot vouch for into the tree that becomes a signed release.
    got="$(shasum -a 256 "${tmp}" | awk '{print $1}')"
    if [ "${got}" != "${want}" ]; then
        rm -f "${tmp}"
        echo "ERROR: sha256 mismatch for ${name} ${version}" >&2
        echo "       expected ${want}" >&2
        echo "       got      ${got}" >&2
        echo "       The mirrored object has changed. Do NOT update the manifest to" >&2
        echo "       match without establishing why." >&2
        exit 1
    fi

    # Remove any existing keg of this exact version first. tar overlays files but
    # does not delete ones the archive lacks, so extracting over a copy Homebrew
    # installed transitively would leave its files behind and the result would no
    # longer be the tree whose hash we just verified. The check at the end reads
    # the directory name, not its contents, so it would not notice.
    rm -rf "${CELLAR:?}/${name}/${version}"
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
