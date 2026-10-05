#!/usr/bin/env bash
export LC_ALL=C
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# update-translations.sh — Complete translation update pipeline.
#
# Full pipeline (default):
#   1. extract    — Rebuild src/qt/bitcoinstrings.cpp from non-Qt _("...") strings
#   2. lupdate    — Update bitcoin_en.ts (and all .ts files) from source code
#   3. tx-pull    — Pull latest translations from Transifex
#   4. postprocess — Convert .xlf → .ts, sanitize, drop empty languages
#   5. lupdate    — Re-run to merge new source strings into pulled translations
#   6. update-build — Regenerate .qrc and CMakeLists.txt locale list
#   7. tx-push    — Push updated bitcoin_en.ts source to Transifex
#   8. cleanup    — Remove temporary files
#
# Prerequisites:
#   - python3, xgettext (gettext package)
#   - lupdate, lconvert (qt6-l10n-tools or equivalent)
#   - tx (Transifex CLI v1.x — https://github.com/transifex/cli)
#   - ~/.transifexrc with valid API token
#
# Usage:
#   ./contrib/devtools/update-translations.sh              # full pipeline
#   ./contrib/devtools/update-translations.sh --no-transifex  # steps 1-2 only (no network)
#   ./contrib/devtools/update-translations.sh extract       # step 1 only
#   ./contrib/devtools/update-translations.sh lupdate       # step 2 only
#   ./contrib/devtools/update-translations.sh scan-list     # print the paths lupdate scans
#   ./contrib/devtools/update-translations.sh tx-pull       # steps 3-6 only
#   ./contrib/devtools/update-translations.sh tx-push       # step 7 only

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# ---------------------------------------------------------------------------
# Configuration — keep in sync with CMakeLists.txt project() and copyright
# ---------------------------------------------------------------------------
PACKAGE_NAME="Gridcoin"
COPYRIGHT_HOLDERS="The %s developers"
COPYRIGHT_HOLDERS_SUBSTITUTION="The Gridcoin developers"

LOCALE_DIR="${REPO_ROOT}/src/qt/locale"
SOURCE_LANG="bitcoin_en"

# Transifex resource ID
TX_RESOURCE="o:gridcoin:p:gridcoin:r:src-qt-locale-bitcoin-en-ts--development"

# Minimum non-numerus messages to keep a language (matches Gridcoin threshold)
MIN_NUM_NONNUMERUS_MESSAGES=10

# Directories under src/ to exclude from lupdate and extract_strings_qt.py
EXCLUDED_DIRS="bdb53|crc32c|leveldb|secp256k1|univalue|test|obj"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
err() { echo "ERROR: $*" >&2; exit 1; }

check_prereq() {
    command -v "$1" >/dev/null 2>&1 || err "'$1' not found. Please install $2."
}

# Find a Qt tool by trying common binary name variants.
find_qt_tool() {
    local tool="$1"
    for candidate in "${tool}-qt6" "${tool}6" "${tool}"; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            echo "${candidate}"
            return 0
        fi
    done
    return 1
}

# Read the Transifex API token from ~/.transifexrc.
get_tx_token() {
    local rc="${HOME}/.transifexrc"
    [ -f "${rc}" ] || err "$HOME/.transifexrc not found. See doc/translation_process.md."
    # Try 'token' first (new CLI format), then fall back to 'password' (old format).
    local token
    token=$(grep -E '^\s*token\s*=' "${rc}" | head -1 | sed 's/.*=\s*//' | tr -d '[:space:]')
    if [ -z "${token}" ]; then
        token=$(grep -E '^\s*password\s*=' "${rc}" | head -1 | sed 's/.*=\s*//' | tr -d '[:space:]')
    fi
    [ -n "${token}" ] || err "No API token found in ~/.transifexrc."
    echo "${token}"
}

# ---------------------------------------------------------------------------
# Step 1: extract non-Qt translatable strings → bitcoinstrings.cpp
# ---------------------------------------------------------------------------
do_extract() {
    check_prereq python3 "python3"
    check_prereq xgettext "gettext"

    echo "==> Step 1: Extracting _() translatable strings..."

    # Collect .cpp and .h files under src/, excluding vendored libraries,
    # test code, and generated translation files. Qt sources ARE included
    # because some use the _() macro (e.g. upgradeqt.cpp, bitcoin.cpp).
    local files
    files=$(find "${REPO_ROOT}/src" \( -name '*.cpp' -o -name '*.h' \) \
        | grep -v '/qt/locale/' \
        | grep -v '/test/' \
        | grep -v '/secp256k1/' \
        | grep -v '/leveldb/' \
        | grep -v '/bdb53/' \
        | grep -v '/univalue/' \
        | grep -v '/crc32c/' \
        | sort)

    # The script writes to qt/bitcoinstrings.cpp relative to cwd.
    cd "${REPO_ROOT}/src"

    PACKAGE_NAME="${PACKAGE_NAME}" \
    COPYRIGHT_HOLDERS="${COPYRIGHT_HOLDERS}" \
    COPYRIGHT_HOLDERS_SUBSTITUTION="${COPYRIGHT_HOLDERS_SUBSTITUTION}" \
        python3 "${REPO_ROOT}/share/qt/extract_strings_qt.py" ${files}

    echo "    Updated src/qt/bitcoinstrings.cpp"
}

# ---------------------------------------------------------------------------
# Step 2/5: lupdate — regenerate .ts files from source
# ---------------------------------------------------------------------------
# The paths lupdate scans, one per line: every directory under src/ except the
# excluded ones, with src/qt expanded one level so that src/qt/qml, the
# carried QML front end, is skipped (its strings are not catalogued in
# bitcoin_*.ts yet), then the .cpp and .h files directly in src/.
lupdate_scan_list() {
    local src_dir="${REPO_ROOT}/src"
    local dir dname entry
    for dir in "${src_dir}"/*/; do
        dname="$(basename "${dir}")"
        if [[ "${dname}" =~ ^(${EXCLUDED_DIRS})$ ]]; then
            continue
        fi
        if [ "${dname}" = "qt" ]; then
            for entry in "${dir}"*; do
                if [ "$(basename "${entry}")" = "qml" ]; then
                    continue
                fi
                if [ -d "${entry}" ]; then
                    echo "${entry}/"
                else
                    # Only files lupdate would pick up from a directory scan:
                    # an explicit file of another type is parsed as C++.
                    case "${entry##*.}" in
                        java|jui|ui|c|c++|cc|cpp|cxx|ch|h|h++|hh|hpp|hxx|js|mjs|qs|qrc) echo "${entry}" ;;
                    esac
                fi
            done
        else
            echo "${dir}"
        fi
    done
    for entry in "${src_dir}"/*.cpp "${src_dir}"/*.h; do
        echo "${entry}"
    done
}

do_lupdate() {
    local lupdate_bin
    lupdate_bin=$(find_qt_tool lupdate) || err "lupdate not found. Please install qt6-l10n-tools."

    echo "==> Running ${lupdate_bin} to update .ts files..."

    # The scanned paths, as lupdate_scan_list prints them.
    local -a scan_dirs=()
    mapfile -t scan_dirs < <(lupdate_scan_list)

    ${lupdate_bin} \
        -locations relative \
        "${scan_dirs[@]}" \
        -ts "${LOCALE_DIR}"/bitcoin_*.ts

    echo "    Updated .ts files in src/qt/locale/"
}

# ---------------------------------------------------------------------------
# Step 3: Pull translations from Transifex
# ---------------------------------------------------------------------------
do_tx_pull() {
    check_prereq tx "Transifex CLI (https://github.com/transifex/cli)"

    echo "==> Step 3: Pulling translations from Transifex..."
    cd "${REPO_ROOT}"
    tx pull --translations --force --all
    echo "    Translations pulled."
}

# ---------------------------------------------------------------------------
# Step 4: Post-process pulled translations (xlf → ts, sanitize)
# ---------------------------------------------------------------------------
do_postprocess() {
    echo "==> Step 4: Post-processing translations..."

    cd "${REPO_ROOT}"
    python3 "${SCRIPT_DIR}/postprocess-translations.py" \
        --locale-dir "${LOCALE_DIR}" \
        --source-lang "${SOURCE_LANG}" \
        --min-messages "${MIN_NUM_NONNUMERUS_MESSAGES}"
}

# ---------------------------------------------------------------------------
# Step 6: Update build system files (.qrc, CMakeLists.txt locale list)
# ---------------------------------------------------------------------------
do_update_build() {
    echo "==> Step 6: Updating build system files..."

    cd "${REPO_ROOT}"

    # Regenerate bitcoin_locale.qrc from the .ts files that survived
    # post-processing.
    python3 - <<PYEOF
import os, re

locale_dir = "${LOCALE_DIR}"
source_lang = "${SOURCE_LANG}"

# Gather all .ts files (including source).
entries = []
for f in sorted(os.listdir(locale_dir)):
    m = re.match(r'(bitcoin_(.*))\.ts$', f)
    if m:
        entries.append((f, m.group(1), m.group(2)))

# --- bitcoin_locale.qrc ---
with open("src/qt/bitcoin_locale.qrc", "w") as fh:
    fh.write('<!DOCTYPE RCC><RCC version="1.0">\n')
    fh.write('    <qresource prefix="/translations">\n')
    for (filename, basename, lang) in entries:
        fh.write(f'        <file alias="{lang}">locale/{basename}.qm</file>\n')
    fh.write('    </qresource>\n')
    fh.write('</RCC>\n')
print("    Updated src/qt/bitcoin_locale.qrc")

# --- CMakeLists.txt locale list ---
cmake_path = os.path.join(locale_dir, "CMakeLists.txt")
with open(cmake_path, "r") as fh:
    content = fh.read()

# Replace the TS_FILES list between "set(TS_FILES" and the closing ")".
ts_list = "\n".join(f"    {fn}" for (fn, _, _) in entries)
new_block = f"set(TS_FILES\n{ts_list}\n)"
content = re.sub(r'set\(TS_FILES\b.*?\)', new_block, content, flags=re.DOTALL)

with open(cmake_path, "w") as fh:
    fh.write(content)
print("    Updated src/qt/locale/CMakeLists.txt")
PYEOF
}

# ---------------------------------------------------------------------------
# Step 7: Push updated source to Transifex
# ---------------------------------------------------------------------------
do_tx_push() {
    echo "==> Step 7: Pushing updated source to Transifex..."

    local token
    token=$(get_tx_token)

    local source_file="${LOCALE_DIR}/${SOURCE_LANG}.ts"
    [ -f "${source_file}" ] || err "Source file not found: ${source_file}"

    # Use the REST API directly because 'tx push --source' fails when
    # .tx/config specifies .xlf format but the resource type is QT (.ts).
    local response
    response=$(curl -sS --fail-with-body -X POST \
        -H "Authorization: Bearer ${token}" \
        -F "resource=${TX_RESOURCE}" \
        -F "content=@${source_file}" \
        "https://rest.api.transifex.com/resource_strings_async_uploads") \
        || err "Transifex upload request failed. Response: ${response}"

    local upload_id
    upload_id=$(echo "${response}" \
        | python3 -c "import sys,json; print(json.load(sys.stdin)['data']['id'])")

    [ -n "${upload_id}" ] || err "Failed to parse Transifex upload response."
    echo "    Upload initiated (id: ${upload_id}). Waiting for completion..."

    # Poll until the async upload completes (typically < 30s).
    local status="pending"
    local attempts=0
    while [ "${status}" = "pending" ] || [ "${status}" = "processing" ]; do
        sleep 5
        attempts=$((attempts + 1))
        [ ${attempts} -le 12 ] || err "Transifex upload timed out after 60s."

        local result
        result=$(curl -sS --fail-with-body \
            -H "Authorization: Bearer ${token}" \
            "https://rest.api.transifex.com/resource_strings_async_uploads/${upload_id}") \
            || err "Transifex status poll failed. Response: ${result}"

        status=$(echo "${result}" | python3 -c "import sys,json; print(json.load(sys.stdin)['data']['attributes']['status'])")
    done

    if [ "${status}" = "succeeded" ]; then
        local details
        details=$(echo "${result}" | python3 -c "
import sys, json
d = json.load(sys.stdin)['data']['attributes']['details']
print(f\"created={d['strings_created']}, updated={d['strings_updated']}, deleted={d['strings_deleted']}\")
")
        echo "    Transifex source push succeeded (${details})."
    else
        local errors
        errors=$(echo "${result}" | python3 -c "
import sys, json
errs = json.load(sys.stdin)['data']['attributes']['errors']
print('; '.join(e.get('detail', str(e)) for e in errs) if errs else 'unknown error')
")
        err "Transifex source push failed: ${errors}"
    fi
}

# ---------------------------------------------------------------------------
# Step 8: Cleanup
# ---------------------------------------------------------------------------
do_cleanup() {
    echo "==> Step 8: Cleaning up..."

    # Remove the source .xlf file that tx pull may have downloaded.
    rm -f "${LOCALE_DIR}/${SOURCE_LANG}.xlf"

    echo "    Cleanup complete."
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
main() {
    local mode="${1:-all}"

    case "${mode}" in
        extract)
            do_extract
            ;;
        lupdate)
            do_lupdate
            ;;
        tx-pull)
            # Transifex pull + post-process + lupdate merge + build update
            do_tx_pull
            do_postprocess
            echo "==> Step 5: Re-running lupdate to merge source strings..."
            do_lupdate
            do_update_build
            do_cleanup
            ;;
        tx-push)
            do_tx_push
            ;;
        scan-list)
            # Only the paths: no closing message, so the output can be
            # passed to lupdate as arguments.
            lupdate_scan_list
            return 0
            ;;
        --no-transifex)
            # Local-only: extract + lupdate (no network access needed)
            do_extract
            echo "==> Step 2: Running lupdate..."
            do_lupdate
            ;;
        all)
            do_extract
            echo "==> Step 2: Running lupdate..."
            do_lupdate
            do_tx_pull
            do_postprocess
            echo "==> Step 5: Re-running lupdate to merge source strings..."
            do_lupdate
            do_update_build
            do_tx_push
            do_cleanup
            ;;
        *)
            echo "Usage: $0 [all|--no-transifex|extract|lupdate|tx-pull|tx-push|scan-list]" >&2
            echo "" >&2
            echo "Modes:" >&2
            echo "  all            Full pipeline (default)" >&2
            echo "  --no-transifex Extract + lupdate only (no network)" >&2
            echo "  extract        Rebuild bitcoinstrings.cpp only" >&2
            echo "  lupdate        Run lupdate only" >&2
            echo "  tx-pull        Pull from Transifex + post-process + lupdate + build update" >&2
            echo "  tx-push        Push source to Transifex only" >&2
            echo "  scan-list      Print the paths lupdate scans" >&2
            exit 1
            ;;
    esac

    echo "==> Done. Review changes with: git diff --stat src/qt/"
}

main "$@"
