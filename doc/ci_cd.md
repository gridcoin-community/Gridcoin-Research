# Gridcoin CMake CI/CD Infrastructure

This document outlines the modern Continuous Integration and Continuous Deployment (CI/CD) system for Gridcoin. The system has been overhauled to support **hermetic builds**, **cross-compilation**, and **local reproducibility**.

## Overview

The pipeline is split into four distinct workflows to separate concerns:

1.  **Production (`cmake_production.yml`)**: Builds shipping-quality artifacts (Linux Static, Windows, macOS). Handles Release Deployment.
2.  **Compatibility (`cmake_compatibility.yml`)**: Verifies compilation on disparate architectures (ARM64, ARMhf) and native Linux system libraries.
3.  **Quality (`cmake_quality.yml`)**: Runs sanitizers (ASan/UBSan), static analysis, and linters.
4.  **Distro Validation (`cmake_distros.yml`)**: Verifies build scripts, including dependencies, on 6+ major Linux distribution families.

-----

## 1\. Workflows

### Production (`cmake_production.yml`)

  * **Trigger:** Push to any branch, Pull Requests, Tags (`v*`, `5.*`).
  * **Goal:** Produce static, portable binaries using the `depends` system.
  * **Jobs:**
      * **Linux Static:** Compiles against `depends` (glibc compatibility). Includes Hermeticity Check (`ldd`) to ensure no system libraries are leaked.
      * **Windows Cross-Compile:** Compiles using MinGW-w64 on Ubuntu. Runs Unit Tests via `Wine64` (Headless). Generates NSIS Installer.
      * **macOS Native:** Compiles natively on macOS runners (ARM64) using Homebrew dependencies. Generates `.dmg` bundles.
  * **Artifacts:** Uploads `.tar.gz`, `.exe`, and `.dmg` files (retained for 5 days).

### Compatibility (`cmake_compatibility.yml`)

  * **Goal:** Ensure code correctness across different architectures and library versions.
  * **Jobs:**
      * **Linux Native:** Builds against system libraries (Ubuntu 24.04).
      * **Linux ARM64:** Cross-compiles for AArch64; runs tests via QEMU.
      * **Linux ARMhf:** Cross-compiles for ARMv7 (32-bit); runs tests via QEMU.

### Quality (`cmake_quality.yml`)

  * **Goal:** Enforce code standards and detect memory errors.
  * **Jobs:**
      * **Sanitizers:** Builds with Clang AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan).
      * **Lint:** Checks formatting (ClangFormat), shell scripts (ShellCheck), Python scripts, and git subtree integrity.

### Distro Validation (`cmake_distros.yml`)

  * **Goal:** Validate `install_dependencies.sh` and `build_targets.sh` on bare-metal environments.
  * **Matrix:** Fedora Latest, Arch Linux, OpenSUSE Leap & Tumbleweed, Debian Sid, Linux Mint.
  * **Mechanism:** Runs full native builds inside isolated Docker containers to ensure the build instructions work for end-users.

-----

## 2\. Local Execution (The "Killer Feature")

Developers can run the **exact same** CI pipelines locally using the provided wrapper script. This uses `act` (a Docker-based runner) to simulate GitHub Actions.

**Requirements:**

  * Docker
  * `act` (`sudo zypper install act` or `brew install act`)
  * `rsync` (Recommended for dirty tree support)

### Usage

The wrapper script `contrib/devtools/run-local-ci.sh` handles concurrency scaling, caching, and isolation automatically.

**Run the full Production pipeline:**

```bash
./contrib/devtools/run-local-ci.sh workflow=.github/workflows/cmake_production.yml
```

**Run a specific job (e.g., Windows only):**

```bash
./contrib/devtools/run-local-ci.sh \
  workflow=.github/workflows/cmake_production.yml \
  job=depends-builds \
  matrix=host:x86_64-w64-mingw32
```

**Validate support for a specific Distro:**

```bash
./contrib/devtools/run-local-ci.sh \
  workflow=.github/workflows/cmake_distros.yml \
  job=validate-distro \
  matrix=image:"archlinux:latest"
```

### Key Local Features

1.  **Auto-Scaling:** The script calculates `nproc / jobs` to prevent system freeze. If you run 6 distro builds on a 32-core machine, it limits each container to \~5 threads.
2.  **Isolation:** Builds run in a temporary copy (`/tmp/act-gridcoin-XXX`). Your local source tree and `build/` directories are **never** touched or polluted.
3.  **Caching:** Maps `~/.ccache` to the container, significantly speeding up subsequent runs.

-----

## 3\. Continuous Deployment (Release)

The `deploy` job in `cmake_production.yml` automates the release process.

  * **Trigger:** Pushing a tag (e.g., `git tag v5.4.9.99 && git push --tags`).
  * **Process:**
    1.  Waits for Linux, Windows, and macOS builds to succeed. Every job that uploads a
        `gridcoin-*` artifact must appear in the `deploy` job's `needs:` list --
        `download-artifact` collects whatever exists when it runs, so a producer left out
        of `needs:` races the release and its asset is silently dropped.
    2.  Downloads all artifacts.
    3.  **Flattens** them (moves files from subfolders to root).
    4.  Creates a **Draft Release** on GitHub with all binaries attached.

Checksums are not generated. The GitHub release page publishes a SHA-256 digest for every
attached asset, so a separate `SHA256SUMS.txt` duplicated information the platform already
provides.

-----

## 4\. Technical Implementation Notes

### Hermetic Cross-Compilation

To support Bitcoin-style `depends` builds with CMake, we explicitly inject the host toolchain paths.

  * **Problem:** CMake finds system `moc`/`rcc` (Qt5/Qt6) instead of the cross-compiled versions.
  * **Solution:** We pass `-DQT_HOST_PATH=<depends_dir>/native` and `-DQT_HOST_PATH_CMAKE_DIR=...`. This forces CMake to use the exact tools built by the depends system, ignoring the host OS environment.

### Windows Testing via Wine

We support running Windows Unit Tests (`gridcoin_tests.exe`) on Linux CI runners.

  * **Stability:** We install `wine64` and enforce `WINEARCH=win64` to prevent "Bad EXE format" errors.
  * **Headless:** We set `QT_QPA_PLATFORM=offscreen` to allow GUI tests to run without an X server.

### Distro Isolation

The `cmake_distros.yml` workflow uses a "Read-Only Mount + Internal Copy" strategy.

  * Host source is mounted `:ro`.
  * Source is copied to an internal `/work` directory.
  * This prevents race conditions where parallel containers (Fedora/Arch) try to write to the same `build/` directory on the host.
