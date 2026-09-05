#!/usr/bin/env bash
#
# Install Homebrew formulae on Intel macOS, source-building and caching any
# whose bottle Homebrew no longer ships.
#
# Homebrew's Tier 3 wind-down removed x86_64 macOS bottles wholesale, and as
# of Homebrew 6.0.21 (commit c0c92cc7b7, "Reduce Intel macOS to Tier 3") a
# plain `brew install` on Intel no longer raises "no bottle available!" -- it
# silently builds the formula from source. Left alone, that turns every CI
# run into a multi-hour source-build marathon with nothing carried between
# runs. This wrapper makes the cost once-per-formula-version instead:
#
#   1. DETECT: compute the full runtime+build dependency closure of the
#      requested formulae and, from `brew info --json=v2` (this brew's own
#      view), which of them lack a usable bottle (no `all` and no non-arm64
#      macOS tag). No error-message parsing -- the premise that brew names
#      casualties in an error died with 6.0.21.
#   2. RESTORE: untar cached kegs for bottle-less formulae whose cached
#      version matches the current one. `brew install --only-dependencies`
#      runs for each restored formula, because brew skips an up-to-date
#      formula BEFORE dependency expansion and would otherwise leave its
#      bottled dependencies uninstalled on the fresh Cellar.
#   3. BUILD: source-build the remaining bottle-less formulae in topological
#      order, stashing each keg (tmp+mv, version from the opt symlink) and
#      rewriting the manifest incrementally, so a run that dies after a
#      two-hour keg still saves what it built and the next run resumes.
#   4. INSTALL: run the ordinary install for the full request list (bottled
#      formulae from bottles; the source-built ones are already present).
#   5. PRUNE: drop cached tarballs that no longer correspond to a current
#      bottle-less formula in the closure, so kegs that regain their bottle
#      or leave the dependency tree stop being carried forever.
#
# The api-source staleness that can break brew's source builds ("<name>
# source code not found at .../api-source/...") gets the rm-and-retry the
# error message itself prescribes.
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

run_brew() {
    # shellcheck disable=SC2086  # BREW_ARCH is intentionally word-split
    ${BREW_ARCH} "${BREW}" "$@"
}

CELLAR="$(run_brew --cellar)" || exit 1
BREW_PREFIX="$(run_brew --prefix)" || exit 1
if [ -z "${CELLAR}" ] || [ ! -d "${BREW_PREFIX}" ]; then
    echo "ERROR: brew at ${BREW} is not functional (cellar='${CELLAR}')" >&2
    exit 1
fi

mkdir -p "${KEG_CACHE}" "${CELLAR}"

link_formula() {
    # Plain link first; --force only where required (keg-only formulae such
    # as openssl@3), and loudly, since force-linking plants headers and
    # libraries ahead of the SDK for every later build step.
    if ! run_brew link --overwrite "$1" >/dev/null 2>&1; then
        echo "keg-cache: NOTE: plain link of $1 failed; force-linking (keg-only?)"
        run_brew link --force --overwrite "$1"
    fi
}

installed_version() {
    # The opt symlink points at Cellar/<name>/<version> and always tracks the
    # active keg; `brew list --versions` prints in readdir order and lies
    # when two versions coexist (HOMEBREW_NO_INSTALL_CLEANUP).
    basename "$(readlink "${BREW_PREFIX}/opt/$1" 2>/dev/null)" 2>/dev/null
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
    local name="$1" version old tmp
    version="$(installed_version "${name}")"
    if [ -z "${version}" ]; then
        echo "keg-cache: cannot determine installed version of ${name}" >&2
        exit 1
    fi
    for old in "${KEG_CACHE}/${name}--"*.tar.gz; do
        [ -e "${old}" ] && rm -f "${old}"
    done
    echo "keg-cache: stashing ${name}--${version}"
    tmp="${KEG_CACHE}/.tmp.${name}.tar.gz"
    if ! tar -czf "${tmp}" -C "${CELLAR}" "${name}/${version}"; then
        rm -f "${tmp}"
        echo "keg-cache: tarring ${name} failed" >&2
        exit 1
    fi
    mv "${tmp}" "${KEG_CACHE}/${name}--${version}.tar.gz"
    write_manifest
}

brew_install_with_apisource_retry() {
    # Stream output while it happens -- a source build can run for hours, and
    # a captured command substitution would show a silent step the whole time
    # -- but keep a copy to detect the api-source staleness afterwards.
    local log status
    log="$(mktemp)"
    run_brew install "$@" </dev/null 2>&1 | tee "${log}"
    status="${PIPESTATUS[0]}"
    if [ "${status}" -ne 0 ] && grep -q "source code not found at .*api-source" "${log}"; then
        echo "keg-cache: clearing stale brew api-source cache and retrying"
        rm -rf "$(run_brew --cache)/api-source"
        run_brew install "$@" </dev/null
        status=$?
    fi
    rm -f "${log}"
    return "${status}"
}

# ---- 1. DETECT -------------------------------------------------------------

echo "=== resolving dependency closure of: $*"
CLOSURE="$(run_brew deps --union --topological --include-build "$@")" || exit 1
# Roots come after their dependencies; keep everything in topological order.
ALL_FORMULAE="$(printf '%s\n%s\n' "${CLOSURE}" "$(printf '%s\n' "$@")" | awk 'NF && !seen[$0]++')"

# One JSON query for the whole set: name, current version (with revision),
# and whether a usable bottle exists for this platform (an `all` bottle or
# any non-arm64 macOS tag).
# shellcheck disable=SC2086  # BREW_ARCH is intentionally word-split
INFO="$(printf '%s\n' "${ALL_FORMULAE}" | xargs ${BREW_ARCH} "${BREW}" info --json=v2 --formula)" || exit 1
NEED_SOURCE="$(printf '%s' "${INFO}" | /usr/bin/python3 -c '
import json, sys
data = json.load(sys.stdin)
for f in data["formulae"]:
    files = (f.get("bottle") or {}).get("stable", {}).get("files", {})
    usable = "all" in files or any(
        not tag.startswith("arm64") and tag != "x86_64_linux" for tag in files)
    if not usable:
        version = f["versions"]["stable"]
        if f.get("revision"):
            version += "_" + str(f["revision"])
        print(f["name"], version)
')" || exit 1

if [ -n "${NEED_SOURCE}" ]; then
    echo "=== formulae without a usable Intel bottle (topological order):"
    printf '%s\n' "${NEED_SOURCE}"
else
    echo "=== every formula in the closure still has a bottle"
fi

# ---- 2. RESTORE ------------------------------------------------------------

printf '%s\n' "${NEED_SOURCE}" | while read -r name version; do
    [ -n "${name}" ] || continue
    tarball="${KEG_CACHE}/${name}--${version}.tar.gz"
    [ -e "${tarball}" ] || continue
    if [ -d "${CELLAR}/${name}/${version}" ]; then
        continue
    fi
    echo "keg-cache: restoring ${name}--${version}"
    if ! tar -xzf "${tarball}" -C "${CELLAR}"; then
        # A truncated tarball from an interrupted save must not brick every
        # later run: treat it as absent and rebuild.
        echo "keg-cache: ${tarball} is unreadable; discarding it"
        rm -f "${tarball}"
        rm -rf "${CELLAR:?}/${name:?}/${version}"
    fi
done

# ---- 3. BUILD (and finish restores) ---------------------------------------

# Sequential on purpose: topological order guarantees each formula's
# bottle-less dependencies were built (or restored) by the time it starts.
# The list is fed through fd 3: brew (and anything else the loop body runs)
# reads stdin, and on the first cold run it ate the heredoc after one entry,
# leaving every remaining casualty to phase 4's silent unstashed builds.
while read -r name version <&3; do
    [ -n "${name}" ] || continue
    if [ -d "${CELLAR}/${name}/${version}" ]; then
        # Restored (or built earlier this run). Its bottled dependencies are
        # NOT implied by the keg: brew skips up-to-date formulae before
        # dependency expansion, so pull them explicitly.
        brew_install_with_apisource_retry --only-dependencies "${name}" || exit 1
        link_formula "${name}" || exit 1
        continue
    fi
    echo "=== source-building ${name} ${version} (no Intel bottle)"
    # --build-from-source is redundant on brew >= 6.0.21 (Intel installs
    # already fall back to source silently) but keeps this working if the
    # Tier 3 gate is ever tightened to raise for Rosetta prefixes too.
    brew_install_with_apisource_retry --build-from-source "${name}" || exit 1
    stash_keg "${name}"
    link_formula "${name}" || exit 1
done 3<<EOF_NEED
${NEED_SOURCE}
EOF_NEED

# ---- 4. INSTALL the full request list -------------------------------------

echo "=== installing the requested formulae"
brew_install_with_apisource_retry "$@" || exit 1

# ---- 5. PRUNE stale cache entries -----------------------------------------

for tarball in "${KEG_CACHE}"/*.tar.gz; do
    [ -e "${tarball}" ] || break
    name_ver="$(basename "${tarball}" .tar.gz)"
    if ! printf '%s\n' "${NEED_SOURCE}" | awk '{print $1 "--" $2}' | grep -qx "${name_ver}"; then
        echo "keg-cache: pruning stale ${name_ver}"
        rm -f "${tarball}"
    fi
done
write_manifest

echo "=== converged; cached source-built kegs:"
if [ -s "${KEG_CACHE}/manifest.txt" ]; then
    cat "${KEG_CACHE}/manifest.txt"
else
    echo "(none needed)"
fi
