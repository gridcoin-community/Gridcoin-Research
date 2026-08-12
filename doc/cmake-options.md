# CMake Build Options Reference

This document details the CMake configuration options available for Gridcoin. These flags control which features are compiled, how dependencies are found, and how the final executable is linked.

## General Configuration

| Option | Default | Description |
| :--- | :--- | :--- |
| `CMAKE_BUILD_TYPE` | *Empty* | Controls optimization and debug symbols. Recommended values: `RelWithDebInfo` (Default/Dev), `Release` (Production), `Debug`. If left empty, no optimization is applied. |
| `ENABLE_GUI` | `OFF` | Builds the Qt-based graphical user interface (`gridcoinresearch`). If `OFF`, only the daemon (`gridcoinresearchd`) is built. |
| `ENABLE_TESTS` | `OFF` | Builds the unit test suite (`src/test/`). Recommended for all developers. |
| `ENABLE_DOCS` | `OFF` | Generates Doxygen documentation. |
| `STATIC_LIBS` | `OFF` | Forces the build system to look for static libraries (`.a`) instead of shared libraries (`.so`). Required for `depends` builds. |
| `ENABLE_PIE` | `OFF` | Enables Position Independent Executables (PIE) for hardening. Recommended for Linux production builds. |
| `ENABLE_DEBUG_LOCKORDER` | `OFF` | Enables run-time lock-order checking (`DEBUG_LOCKORDER`). Detects potential deadlocks by tracking lock acquisition order and logging inconsistencies to `debug.log`. Recommended with `Debug` build type. |

---

## Features & Dependencies

These options toggle specific functionality within the Gridcoin client.

| Option | Default | Why Use It? | Dependencies |
| :--- | :--- | :--- | :--- |
| `ENABLE_UPNP` | `ON` | **Universal Plug and Play.** Compiles in the ability to map a port on your router for incoming connections. Compiled in by default so the GUI setting is operative; it does nothing until enabled at runtime. | `miniupnpc` |
| `DEFAULT_UPNP` | `OFF` | Starts with UPnP already enabled. Off by default: the stock configuration makes 8 outbound connections, so there is no reason to ask the router to open an inbound port until the operator asks for one. Ignored when not listening. | `ENABLE_UPNP` |
| `ENABLE_QRENCODE` | `OFF` | **QR Codes.** Allows the GUI to display QR codes for wallet addresses. Convenient for mobile payments. | `libqrencode` |
| `USE_DBUS` | `OFF` | **Desktop Bus.** Enables OS notifications on Linux desktops (e.g., "Staked a block!"). | `QtDBus` |
| `USE_QT6` | `OFF` | Builds against Qt 6 instead of Qt 5. Recommended for modern Linux distributions. | `Qt6` |
| `ENABLE_MULTIPROCESS` | `OFF` | **Multiprocess (IPC) build (RFC #2937).** Builds against Cap'n Proto and the vendored libmultiprocess subtree so the `interfaces::` boundaries can be driven over IPC. Toolchain-only for now; see [multiprocess.md](multiprocess.md). | `CapnProto` (external); libmultiprocess is vendored in-tree |
| `WITH_EXTERNAL_LIBMULTIPROCESS` | `OFF` | Build the vendored in-tree libmultiprocess subtree (default). Set to `ON` to link an external (system / depends) libmultiprocess instead — mainly useful when developing libmultiprocess itself. | `ENABLE_MULTIPROCESS` |

---

## CPU & Assembly

| Option | Default | Description |
| :--- | :--- | :--- |
| `USE_ASM` | `ON` | Enable assembly routines (SHA-256 SSE4, secp256k1). |
| `USE_ASM_SCRYPT` | `USE_ASM` (non-Apple), `OFF` (Apple) | Enable scrypt x86/x86_64 assembly. Requires a GNU-compatible assembler; Apple's LLVM assembler is not compatible, so this defaults to `OFF` on macOS. The C++ fallback in `scrypt.cpp` is used when disabled. |
| `ENABLE_SSE41` | Auto-detected | Build SHA-256 code that uses SSE4.1 intrinsics. |
| `ENABLE_AVX2` | Auto-detected | Build SHA-256 code that uses AVX2 intrinsics. |
| `ENABLE_X86_SHANI` | Auto-detected | Build SHA-256 code that uses x86 SHA-NI intrinsics. |
| `ENABLE_ARM_SHANI` | Auto-detected | Build SHA-256 code that uses ARM SHA-NI intrinsics. |

---

## Advanced / Cross-Compilation

These options are primarily used by the `depends` system or advanced users.

| Option | Default | Description |
| :--- | :--- | :--- |
| `SYSTEM_XXD` | `OFF` | Uses the host system's `xxd` binary instead of building one. **Required** when cross-compiling (e.g., Linux -> Windows). |
| `SYSTEM_UNIVALUE` | `OFF` | Links against a system-installed `libunivalue` instead of the in-tree submodule. |
| `SYSTEM_SECP256K1` | `OFF` | Links against a system-installed `libsecp256k1`. |
| `SYSTEM_LEVELDB` | `OFF` | Links against a system-installed `libleveldb`. |
| `SYSTEM_BDB` | `OFF` | Links against a system-installed Berkeley DB. *Note: Gridcoin requires BDB 5.3, which is rare in modern distros.* |

## Example Configurations

### Standard Developer Build (Linux)
```bash
cmake -B build -DENABLE_GUI=ON -DENABLE_QRENCODE=ON -DUSE_DBUS=ON \
    -DENABLE_TESTS=ON -DENABLE_UPNP=ON \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
````

### Minimal GUI Developer Build (Linux)
```bash
cmake -B build -DENABLE_GUI=ON -DENABLE_TESTS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
````

### Minimal GUI Developer Build (Linux) for Detailed Debug
```bash
cmake -B build -DENABLE_GUI=ON -DENABLE_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
```

### Headless Server / Daemon Only

```bash
cmake -B build -DENABLE_GUI=OFF -DENABLE_UPNP=OFF -DCMAKE_BUILD_TYPE=Release
```

### Full Feature Release

```bash
cmake -B build \
    -DENABLE_GUI=ON -DENABLE_QRENCODE=ON -DUSE_DBUS=ON \
    -DENABLE_UPNP=ON \
    -DENABLE_PIE=ON \
    -DCMAKE_BUILD_TYPE=Release
```
