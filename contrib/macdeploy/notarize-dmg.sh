#!/usr/bin/env bash
export LC_ALL=C
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# notarize-dmg.sh — Submit signed DMGs to Apple for notarization and staple.
#
# CI notarizes and staples release DMGs itself (the "Notarize and Staple
# (best-effort)" step in .github/workflows/cmake_production.yml, on tag builds).
# That step is deliberately non-fatal: if credentials are absent, if Apple's
# notary does not finish inside the 45m wait, or if stapling fails after a
# successful notarization, it warns and publishes the signed DMG rather than
# failing the release. This script is how that DMG gets finished afterwards --
# it is the fallback those warnings point at, not a replacement for the job.
#
# Run it on a macOS machine with valid Apple Developer credentials.
#
# Usage:
#   ./contrib/macdeploy/notarize-dmg.sh <dmg-file> [<dmg-file> ...]
#
# Environment variables (or pass interactively):
#   APPLE_ID    — Apple ID email for notarytool
#   TEAM_ID     — Apple Developer Team ID
#   PASSWORD    — App-specific password (or keychain profile name with --keychain-profile)
#
# Example:
#   APPLE_ID=dev@example.com TEAM_ID=ABCDEF1234 PASSWORD=xxxx-xxxx-xxxx-xxxx \
#     ./contrib/macdeploy/notarize-dmg.sh gridcoin-5.5.0.0-macos-arm64.dmg

set -euo pipefail

err() { echo "ERROR: $*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Credentials
# ---------------------------------------------------------------------------
APPLE_ID="${APPLE_ID:-}"
TEAM_ID="${TEAM_ID:-}"
PASSWORD="${PASSWORD:-}"

if [ -z "$APPLE_ID" ]; then
    read -rp "Apple ID: " APPLE_ID
fi
if [ -z "$TEAM_ID" ]; then
    read -rp "Team ID: " TEAM_ID
fi
if [ -z "$PASSWORD" ]; then
    read -rsp "App-specific password: " PASSWORD
    echo
fi

[ -n "$APPLE_ID" ] || err "APPLE_ID is required."
[ -n "$TEAM_ID" ] || err "TEAM_ID is required."
[ -n "$PASSWORD" ] || err "PASSWORD is required."

# ---------------------------------------------------------------------------
# Process each DMG
# ---------------------------------------------------------------------------
[ $# -ge 1 ] || err "Usage: $0 <dmg-file> [<dmg-file> ...]"

for DMG in "$@"; do
    [ -f "$DMG" ] || err "File not found: $DMG"

    echo "=== Submitting: $DMG ==="
    SUBMIT_OUTPUT=$(xcrun notarytool submit "$DMG" \
        --apple-id "$APPLE_ID" \
        --password "$PASSWORD" \
        --team-id "$TEAM_ID" \
        --output-format json 2>&1) || {
        echo "Submit failed for $DMG:"
        echo "$SUBMIT_OUTPUT"
        continue
    }

    SUBMISSION_ID=$(echo "$SUBMIT_OUTPUT" | python3 -c "import sys,json; print(json.load(sys.stdin)['id'])")
    echo "  Submission ID: $SUBMISSION_ID"

    echo "=== Waiting for notarization ==="
    if xcrun notarytool wait "$SUBMISSION_ID" \
        --apple-id "$APPLE_ID" \
        --password "$PASSWORD" \
        --team-id "$TEAM_ID"; then

        echo "=== Fetching log ==="
        xcrun notarytool log "$SUBMISSION_ID" \
            --apple-id "$APPLE_ID" \
            --password "$PASSWORD" \
            --team-id "$TEAM_ID" || true

        echo "=== Stapling: $DMG ==="
        xcrun stapler staple "$DMG"
        xcrun stapler validate "$DMG"
        echo "=== Done: $DMG is notarized and stapled. ==="
    else
        echo "Notarization failed or timed out for $DMG."
        echo "Fetching log for details:"
        xcrun notarytool log "$SUBMISSION_ID" \
            --apple-id "$APPLE_ID" \
            --password "$PASSWORD" \
            --team-id "$TEAM_ID" || true
    fi

    echo
done
