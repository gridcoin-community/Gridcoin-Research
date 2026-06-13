#!/usr/bin/env bash
# Push a generated source package into an OBS package, triggering its build.
# osc must already be configured (oscrc with apiurl + credentials) beforehand.
#
# Usage: push-to-obs.sh <obs_project> <obs_package> [SRCDIR]   (default SRCDIR=./obs-out)
export LC_ALL=C
set -euo pipefail

PROJECT="$1"; PACKAGE="$2"; SRCDIR="${3:-$PWD/obs-out}"

command -v osc >/dev/null || { echo "::error::osc not found"; exit 1; }
shopt -s nullglob
dscs=( "$SRCDIR"/*.dsc )
[ ${#dscs[@]} -gt 0 ] || { echo "::error::no .dsc in $SRCDIR"; exit 1; }

VER=$(basename "${dscs[0]}" | sed -E 's/.*_([^_]+)\.dsc$/\1/')

WC=$(mktemp -d)
trap 'rm -rf "$WC"' EXIT
( cd "$WC"
  osc checkout "$PROJECT" "$PACKAGE"
  cd "$PROJECT/$PACKAGE"

  # Replace any existing source files so version bumps land cleanly.
  shopt -s nullglob
  for f in *.dsc *.orig.tar.* *.debian.tar.*; do
    osc rm --force "$f" >/dev/null 2>&1 || rm -f "$f"
  done
  cp "$SRCDIR"/*.dsc "$SRCDIR"/*.orig.tar.* "$SRCDIR"/*.debian.tar.* .

  osc addremove
  sha="${GITHUB_SHA:-}"
  osc commit -m "CI: ${PACKAGE} ${VER} (${GITHUB_REF_NAME:-manual}@${sha:0:8})"
)
echo "Pushed ${PACKAGE} ${VER} to ${PROJECT}"
