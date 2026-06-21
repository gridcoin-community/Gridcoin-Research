# OBS packaging CI

Builds Gridcoin Research `.deb` packages on the [openSUSE Build Service](https://build.opensuse.org)
(OBS) straight from this repo, via GitHub Actions. OBS does the compiling, signing, and apt-repo
publishing across whatever distros/arches you configure on the OBS side; GitHub just hands it a
ready-made source package.

**This is opt-in and inert by default.** With no `OBS_PROJECT` variable set, the workflows do
nothing — merging them changes nothing for a repo that hasn't configured OBS.

## What's here

| File | Role |
|------|------|
| `../../debian/` | Mainnet (stable) Debian packaging — `gridcoinresearchd` / `gridcoinresearch-qt` |
| `../../debian-testnet/` | Testnet packaging — same source, **renamed** to `gridcointestnetd` / `gridcointestnet-qt` (via `dh-exec`) so a testnet build coexists with mainnet |
| `../../.github/workflows/obs-build.yml` | Reusable engine: generate `3.0 (quilt)` source → push to OBS |
| `../../.github/workflows/OBS-mainnet.yml` | Trigger: mainnet release tags (`*.*.*.0`) |
| `../../.github/workflows/OBS-testnet.yml` | Trigger: testnet release tags (`*-testnet`) |
| `build-source-package.sh` | orig-from-tree → `dpkg-source -b` (zero source patches; the delta is `debian/` only) |
| `push-to-obs.sh` | `osc` commit of the source package into the OBS package |

For testnet, the engine activates `debian-testnet/` by mangling it into `debian/` before building,
then drops the inactive sibling so the quilt source stays clean.

## How it runs

A deliberate **release tag** (never a raw push) triggers the matching workflow, which calls the
engine. The engine checks out the tagged ref, builds the quilt source package, and `osc commit`s it
to the OBS package. OBS then builds + auto-publishes a signed apt repo per its repo/arch matrix.

Mainnet and testnet are **two OBS packages** (e.g. `gridcoinresearch` and `gridcointestnet`) — same
source, different packaging — so they publish independently.

## Activating it (for a fork or upstream)

1. **OBS side:** create a project and two packages (`gridcoinresearch`, `gridcointestnet`); add your
   distro/arch repositories. (A namespace-agnostic project `_meta` template lives outside the repo.)
2. **GitHub variable:** set `OBS_PROJECT` to your OBS project name (e.g. `home:you:gridcoin` or
   `gridcoin-community:gridcoin`). *Without this, the workflows are a no-op.*
3. **GitHub secrets:** `OBS_USER` / `OBS_PASS` — an OBS account credential with commit rights to the
   project (source upload needs login auth; OBS tokens only *trigger*, they can't upload).
4. **Build:** push a `*.*.*.0` tag for mainnet, or a `*-testnet` tag for testnet.

> Note: GitHub only shows the `workflow_dispatch` "Run workflow" button for workflows on the repo's
> **default branch**. On a feature branch, use a tag (or a temporary `push:` trigger) to test.

## Notes

- **No `~distro` version suffix** — each OBS repository is its own apt path, so the canonical version
  (e.g. `5.5.1.0-3`) coexists across distros cleanly.
- **No `-dbgsym` packages** — disabled in `rules` (`dh_strip --no-automatic-dbgsym`) to keep the
  user-facing repo uncluttered; rebuild locally with symbols when needed.
- **Source format is `3.0 (quilt)`** — keeps upstream/packaging separated and preserves the
  packaging-only `-N` revision (e.g. a `-3` packaging fix over upstream `5.5.1.0`).
