---
name: gridcoin-gui-wsl
description: Build and run the Gridcoin Qt wallet GUI on Windows using WSL2 + WSLg, for iterating on the src/qt UI. Use when a Windows contributor asks to build, run, launch, or screenshot the Gridcoin GUI (gridcoinresearch). Windows-only — Linux/macOS contributors should build natively per doc/build.md.
---

# Build & run the Gridcoin GUI on Windows (WSL2 + WSLg)

Native Windows (MSVC) builds are not supported. On Windows the fast path is to
build a **Linux-native Qt6 GUI inside WSL2** and let **WSLg** display the window
on the Windows desktop. This skill automates the build → run → iterate loop for
UI work. It complements [doc/build-windows-wsl.md](../../../doc/build-windows-wsl.md).

**Scope: Windows 10/11 only.** On Linux/macOS, build natively (see doc/build.md);
this skill does not apply.

## Prerequisites (one-time)

- Windows 10/11 with **WSL2 + WSLg** — check `wsl --version` shows a WSLg version.
  If WSL isn't installed: run `wsl --install` in an admin PowerShell, then reboot.
- **Clone the repo INSIDE the WSL filesystem** (e.g. `~/Gridcoin-Research` on
  ext4), NOT under `/mnt/c` or OneDrive — building from `/mnt/c` is very slow and
  can hit file-permission issues.
- Build dependencies + the **Qt6 Wayland plugin** (see Setup).

## Conventions (for the assistant running this skill)

- Run shell steps in the user's **default** WSL distro: `wsl bash -lc '<cmd>'`
  (or `wsl -d <distro> …` if they name one — discover with `wsl -l -q`). Never
  hardcode a distro name.
- **Repo path:** default `$HOME/Gridcoin-Research`; if the user's clone is
  elsewhere, take the path as an argument and substitute. Always use `$HOME` /
  `$USER` — never a hardcoded username.
- **Editing GUI source from a Windows editor:** derive the Windows path with
  `wslpath -w "$REPO/src/qt"` (gives the `\\wsl.localhost\<distro>\…` UNC path).
  Don't hardcode it.
- **Build dir:** `build-gui/` (kept separate from any `build/` the user uses for
  tests). **Binary:** `build-gui/bin/gridcoinresearch` (CMake emits to `bin/`).

## Setup (first time)

1. Install build deps — easiest is the helper script once:
   `./build_targets.sh TARGET=native USE_CCACHE=true` (installs system deps,
   configures, builds). Or install Qt6/Boost/etc. per doc/build-dependencies.md.
2. **Install the Qt6 Wayland platform plugin** — REQUIRED for the window to render
   under WSLg (see Rendering note). Run in a real WSL terminal (sudo prompts for a
   password):
   ```bash
   sudo apt install -y qt6-wayland
   ```
   Verify (Ubuntu path; varies by distro):
   ```bash
   ls /usr/lib/x86_64-linux-gnu/qt6/plugins/platforms/ | grep wayland   # expect libqwayland-*.so
   ```

## Configure + build (fast dev loop)

Configure `build-gui/` once (GUI on, tests off, ccache); then incremental rebuilds
are seconds. Reconfigure only if it's not already a GUI build:
```bash
REPO=$HOME/Gridcoin-Research   # or the user's clone path
wsl bash -lc "cd $REPO && grep -q '^ENABLE_GUI:BOOL=ON' build-gui/CMakeCache.txt 2>/dev/null || \
  cmake -B build-gui -DENABLE_GUI=ON -DUSE_QT6=ON -DENABLE_TESTS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
wsl bash -lc "cd $REPO && cmake --build build-gui -j\$(nproc)"
```
Run the build in the background — a cold build is several minutes; incremental
builds (after editing `src/qt/...`) are seconds thanks to ccache.

## Run (WSLg)

Launch as the **foreground (`exec`) process of a background task** so the command
returns immediately while the GUI keeps running. Use native Wayland + an isolated
datadir + regtest so the window comes up instantly and offline:
```bash
wsl bash -lc "cd $REPO && mkdir -p ~/gc-devdata && \
  exec env QT_QPA_PLATFORM=wayland ./build-gui/bin/gridcoinresearch -datadir=\$HOME/gc-devdata -regtest"
```
- `QT_QPA_PLATFORM=wayland` is REQUIRED (see Rendering note).
- `-regtest` gives a private, offline chain (no sync → instant idle UI); `-datadir`
  keeps it off your real wallet. For real-network behaviour use `-testnet`. Plain
  mainnet will sync ~4M blocks and starve the UI — avoid it for dev.
- Don't use `nohup … &` and let the command return — WSL reaps the backgrounded
  GUI when the `wsl` invocation exits. The foreground-`exec`-of-a-background-task
  pattern keeps it alive for the session.

## Rendering note (WSLg quirks — read if the window is blank)

- **Blank/white window** usually means it's rendering through XWayland. Ensure
  `qt6-wayland` is installed and `QT_QPA_PLATFORM=wayland` is set.
- WSLg's compositor tends to reliably render only the **first** Qt client after a
  fresh WSL start. If a relaunch comes up blank, reset and start clean:
  ```bash
  wsl --shutdown            # closes ALL WSL — save other WSL work first
  # then bring WSL back and wait for the compositor before launching:
  wsl bash -lc 'until [ -S "$XDG_RUNTIME_DIR/wayland-0" ]; do sleep 1; done; echo ready'
  ```
  Launch the GUI as the first client after that.
- These behaviours are environment-dependent (WSL/WSLg/GPU/driver). If your setup
  renders fine on relaunch, you can skip the reset step.

## Optional: real Windows .exe / installer

Cross-compile a Windows build from WSL:
```bash
./build_targets.sh TARGET=win64 BUILD_TYPE=RelWithDebInfo USE_CCACHE=true
```
First run compiles the `depends` system (slow). Output: `build_win64/bin/gridcoinresearch.exe`;
build the NSIS installer via the cpack step in doc/build-windows-wsl.md. On Ubuntu
22.04 you must install a newer MinGW toolchain first (see that doc); 24.04+ works
out of the box.
