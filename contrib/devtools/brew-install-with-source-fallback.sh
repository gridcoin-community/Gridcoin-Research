#!/usr/bin/env bash
#
# Install Homebrew formulae, source-building any whose bottle is missing.
#
# Homebrew is winding down x86_64 macOS bottles (Tier 3 platform demotion):
# formulae keep working but lose their Intel bottles one by one, and a plain
# `brew install` aborts on the first casualty. Rather than chase each demotion
# with a bespoke workflow change, this wrapper converges generically:
#
#   1. Pre-install any kegs carried in the cache directory from earlier runs.
#   2. Try the requested install. On success, done.
#   3. On "no bottle available" (or the api-source staleness that can break
#      brew's own source fall-back), source-build exactly the formula brew
#      named, stash its keg in the cache directory, and retry. Transitive
#      dependencies surface by name in brew's error output, so nothing has to
#      be enumerated in advance; the loop converges bottom-up.
#
# The cache directory holds one tarball per source-built keg plus manifest.txt
# (sorted name-version lines) for use as a CI cache key.
#
# usage: brew-install-with-source-fallback.sh <keg-cache-dir> <formula>...
# env:   BREW      brew executable (default: /usr/local/bin/brew)
#        BREW_ARCH prefix command, e.g. "arch -x86_64" (default: empty)

export LC_ALL=C
set -u

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <keg-cache-dir> <formula>..." >&2
    exit 2
fi

KEG_CACHE="$1"
shift

BREW="${BREW:-/usr/local/bin/brew}"
BREW_ARCH="${BREW_ARCH:-}"
CELLAR="$(${BREW_ARCH} "${BREW}" --cellar)"
MAX_ITERATIONS=20

mkdir -p "${KEG_CACHE}"

run_brew() {
    # shellcheck disable=SC2086  # BREW_ARCH is intentionally word-split
    ${BREW_ARCH} "${BREW}" "$@"
}

link_formula() {
    # --force covers keg-only formulae (openssl@3); on ordinary formulae it is
    # accepted with a notice. A link failure must fail the job here rather
    # than surface later as a configure mystery.
    run_brew link --force --overwrite "$1"
}

restore_cached_kegs() {
    local tarball name_ver name
    for tarball in "${KEG_CACHE}"/*.tar.gz; do
        [ -e "${tarball}" ] || return 0
        name_ver="$(basename "${tarball}" .tar.gz)"
        name="${name_ver%--*}"
        if [ -d "${CELLAR}/${name}" ]; then
            echo "keg-cache: ${name} already present, skipping ${name_ver}"
            continue
        fi
        echo "keg-cache: restoring ${name_ver}"
        tar -xzf "${tarball}" -C "${CELLAR}" || exit 1
        link_formula "${name}" || exit 1
    done
}

write_manifest() {
    local tarball
    : > "${KEG_CACHE}/manifest.txt"
    for tarball in "${KEG_CACHE}"/*.tar.gz; do
        [ -e "${tarball}" ] || break
        basename "${tarball}" .tar.gz >> "${KEG_CACHE}/manifest.txt"
    done
    sort -o "${KEG_CACHE}/manifest.txt" "${KEG_CACHE}/manifest.txt"
}

stash_keg() {
    local name="$1" version old
    version="$(run_brew list --versions "${name}" | awk '{print $NF}')"
    if [ -z "${version}" ]; then
        echo "keg-cache: cannot determine installed version of ${name}" >&2
        exit 1
    fi
    for old in "${KEG_CACHE}/${name}--"*.tar.gz; do
        [ -e "${old}" ] && rm -f "${old}"
    done
    echo "keg-cache: stashing ${name}--${version}"
    tar -czf "${KEG_CACHE}/${name}--${version}.tar.gz" -C "${CELLAR}" "${name}/${version}" || exit 1
    # Keep the manifest current after every stash, not only at convergence:
    # a run that builds a two-hour keg and then dies on a later formula must
    # still be able to save what it built.
    write_manifest
}

restore_cached_kegs

iteration=0
while :; do
    iteration=$((iteration + 1))
    if [ "${iteration}" -gt "${MAX_ITERATIONS}" ]; then
        echo "ERROR: no convergence after ${MAX_ITERATIONS} install attempts" >&2
        exit 1
    fi

    echo "=== install attempt ${iteration}: $*"
    output="$(run_brew install "$@" 2>&1)"
    status=$?
    printf '%s\n' "${output}"
    if [ "${status}" -eq 0 ]; then
        break
    fi

    # "qtbase: no bottle available!" -- the formula named is the casualty,
    # whether it is one we asked for or a transitive dependency.
    formula="$(printf '%s\n' "${output}" \
        | sed -n 's/^.*[Ee]rror: *\([A-Za-z0-9@._+-]*\): no bottle available.*/\1/p' | head -1)"
    if [ -z "${formula}" ]; then
        formula="$(printf '%s\n' "${output}" \
            | sed -n 's/^\([A-Za-z0-9@._+-]*\): no bottle available.*/\1/p' | head -1)"
    fi

    # Brew's own source fall-back can die on a stale api-source checkout
    # ("<name> source code not found at .../api-source/..."); the remedy it
    # suggests itself is clearing that cache and retrying.
    if [ -z "${formula}" ]; then
        formula="$(printf '%s\n' "${output}" \
            | sed -n 's/^.*[Ee]rror: *\([A-Za-z0-9@._+-]*\) source code not found at .*api-source.*/\1/p' | head -1)"
        if [ -n "${formula}" ]; then
            echo "keg-cache: clearing stale brew api-source cache"
            rm -rf "$(run_brew --cache)/api-source"
        fi
    fi

    if [ -z "${formula}" ]; then
        echo "ERROR: install failed for a reason other than a missing bottle" >&2
        exit "${status}"
    fi

    echo "=== ${formula}: no Intel bottle; building from source"
    run_brew install --build-from-source "${formula}" || {
        # A missing bottle for one of ${formula}'s own dependencies fails
        # here with the dependency's name in the output; loop and let the
        # next attempt catch it. Anything else keeps failing until the
        # iteration bound trips.
        echo "=== source build of ${formula} did not complete; retrying via the loop"
        continue
    }
    stash_keg "${formula}"
    link_formula "${formula}" || exit 1
done

write_manifest
echo "=== converged; cached source-built kegs:"
cat "${KEG_CACHE}/manifest.txt" 2>/dev/null || echo "(none needed)"
