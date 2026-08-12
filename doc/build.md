# Gridcoin Build Guide

**CMake is the sole build system for Gridcoin.** The following table provides the current status for various build targets:

| Platform | Build System | Status | Documentation |
| :--- | :--- | :--- | :--- |
| **Linux (Native)** | CMake | **Stable** | This file |
| **Linux (Static)** | CMake (depends) | **Stable** | This file |
| **Linux Alpine (MUSL)** | CMake | **Experimental** | This file |
| **Windows (Cross)**| CMake (depends) | **Stable** | This file |
| **Windows via WSL**| CMake (depends) | **Stable** | [build-windows-wsl.md](build-windows-wsl.md) |
| **macOS** | CMake | **Stable** | [build-macos.md](build-macos.md) |
| **MSYS2** | CMake | *Deprecated* | [build-msys2.md](build-msys2.md) |
| **FreeBSD** | CMake | **Stable but not checked in CI/CD** | [build-freebsd.md](build-freebsd.md) |
| **OpenBSD** | CMake | **Stable but not checked in CI/CD** | [build-openbsd.md](build-openbsd.md) |

This document covers these build targets:

1.  **Linux Native:** Dynamic linking against system libraries (best for development & Linux distributions).
2.  **Linux Static:** Static linking against the `depends` system (best for portable release binaries).
3.  **Windows:** Cross-compilation using the `depends` system (for Windows release binaries).

## Prerequisites

  * **CMake:** 3.18 or later
  * **Compiler:** GCC (C++17 support required) or Clang. If your system compiler is not compliant, you will need to install
    a compiler that is C++17 compliant and use -DCMAKE_CXX_COMPILER=\< C++ compiler \> and -DCMAKE_C_COMPILER=\< C compiler \>
  * **Qt:** Version 5.15 or 6.x
  * **Boost:** Version 1.63 or later
  * Please refer to [link](build-dependencies.md) (build-dependencies.md) for packages that must be installed before building.

-----

## Quick Reference

| Target | Primary Use Case |
| :--- | :--- | :--- |
| **Linux Native** | Development, Package Maintainers, Enthusiasts that like to roll their own builds |
| **Linux Static** | Portable releases, especially useful for older distributions that can't meet native package dependencies |
| **Windows Cross-Compile** | Windows installer/Executable compiling from Linux host |
| **Windows via WSL** | Windows installer/Executable compiling from WSL running in Windows 10/11 |
| **macOS** | Development, Package Maintainers, Enthusiasts that like to roll their own builds |

We have created a fairly comprehensive and easy to use build script for these five major targets, ***build_targets.sh***, and its helper script, ***install_dependencies.sh***.

## Automatic Build Script

```
./build_targets.sh -h
Usage: ./build_targets.sh [OPTIONS]

Options:
  TARGET=<target>     Select build target. Options: native, depends, win64, all.
                      Default: all
  BUILD_TYPE=<type>   Set the CMake build type. Options: Release, Debug, RelWithDebInfo.
                      Default: RelWithDebInfo
  CLEAN_BUILD=<bool>  Force a clean build even if executables exist. Options: true, false.
                      Default: false
  SKIP_DEPS=<bool>    Skip installing system dependencies (step 1). Options: true, false.
                      Default: false
  USE_CCACHE=<bool>   Enable ccache compiler launcher. Options: true, false.
                      Default: false
  USE_QT6=<bool>      Use Qt6 for Linux Native build. Options: true, false.
                      Default: true (Set to false for Qt5)
  PARALLEL=<int>      Specify number of build threads to use (i.e. -j X).
                      Default: number of cpu threads reported by OS
  CC=<path>           Override C compiler for Native Linux build.
                      (e.g., CC=/usr/bin/gcc-13).
  CXX=<path>          Override C++ compiler for Native Linux build.
                      (e.g., CXX=/usr/bin/g++-13).
  --help, -h          Show this help message.
```

This script works for all five major targets. The "native" target (Linux Native) should work across all seven major distributions that we check in continuous integration testing on Github, including the automatic installation of all the necessary dependencies. The "depends" target (Linux Static) and the "win64" target (Windows Cross-Compile) works with Ubuntu latest, Fedora, and OpenSUSE, and probably the others as well. The macOS target should work with macOS 12 "Monterey" or newer.

A typical example of the use of this script by someone desiring a local build dynamically linked to the system libraries would be:

```
./build_targets.sh TARGET=native BUILD_TYPE=Release CLEAN_BUILD=true
```

After you build with this script with the appropriate target

* For installation, skip to the Install step in the target you selected below and follow that step for installation.
* For a more custom build, use the cmake detailed instructions below.

# Detailed Build Instructions for **Linux Native**, **Linux Static**, and **Windows Cross-Compile**

Please refer to [Link](cmake-options.md) (cmake-options.md) for a list of cmake configuration options.

To build the **split GUI/node (multiprocess)** variant — where the GUI and daemon run as separate processes that communicate over IPC — add `-DENABLE_MULTIPROCESS=ON` to a native build, or build the depends tree with `MULTIPROCESS=1` for a static/Windows build. See [multiprocess.md](multiprocess.md) for building **and running** in multiprocess mode (including the Windows requirement that the daemon run as the same user as the GUI), and [running-unattended.md](running-unattended.md) for running the core as a **background service that stakes unattended** (Linux systemd / Windows Task Scheduler, with optional stake-only autounlock).

Developers may want to use -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache to have ccache cache
the compilation at the ccache level. In many instances it is required to do an rm -rf of the build directory, and this will speed up
repeated compilations.

-----

## 1\. Linux Native Build (also works for Linux Alpine (MUSL))

This procedure uses your operating system's installed libraries (OpenSSL, Boost, Qt, etc.). It creates a dynamically linked executable.

### Step 1: Configuration

Run the following from the repository root:

Note that if your distribution has Qt6, you will need to add -DUSE_QT6. You can also leave out -DENABLE_QRENCODE and -DENABLE_UPNP if you do not use that functionality.

```bash
rm -rf build
cmake -B build \
    -DENABLE_GUI=ON \
    -DENABLE_QRENCODE=ON \
    -DUSE_DBUS=ON \
    -DENABLE_UPNP=ON \
    -DENABLE_PIE=ON \
    -DENABLE_DOCS=ON \
    -DENABLE_TESTS=ON \
    -DCMAKE_BUILD_TYPE=\< Release | RelWithDebInfo | Debug - see table below \>
```

### Step 2: Build & Test

```bash
# Build using all available CPU cores
cmake --build build -j $(nproc)

# Run the test suite
ctest --test-dir build
```

### Step 3: Install (Optional)

To install the binaries to your system (default: `/usr/local/bin`):

```bash
sudo cmake --install build
```

-----

## 2\. Linux Static Build (Depends System)

This procedure builds a portable binary that is statically linked against specific versions of libraries (Boost, Qt, BDB, etc.) provided by the Gridcoin `depends` system, but dynamically linked against the system `glibc`.

### Step 1: Build Dependencies

```bash
cd depends
make HOST=x86_64-pc-linux-gnu -j $(nproc)
cd ..
```

### Step 2: Configure

We use the toolchain file generated by the depends system and strictly define linker flags to ensure portability.

```bash
rm -rf build_linux_depends
export DEP_LIB=$(pwd)/depends/x86_64-pc-linux-gnu/lib

cmake -B build_linux_depends \
    --toolchain depends/x86_64-pc-linux-gnu/toolchain.cmake \
    -DENABLE_GUI=ON \
    -DUSE_QT6=ON \
    -DSTATIC_LIBS=ON \
    -DENABLE_UPNP=ON \
    -DENABLE_TESTS=ON \
    -DDEP_LIB="${DEP_LIB}" \
    -DCMAKE_CXX_FLAGS="-fPIE" \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -Wl,-Bdynamic" \
    -DCMAKE_BUILD_TYPE=\< Release | RelWithDebInfo | Debug - see table below \>
```

### Step 3: Build & Test

```bash
cmake --build build_linux_depends -j $(nproc)

ctest --test-dir build_linux_depends
```

### Step 4: Install (Optional)

To install the binaries to your system (default: `/usr/local/bin`):

```bash
sudo cmake --install build
```

-----

## 3\. Windows Cross-Compile Build

This procedure generates a Windows 64-bit executable (`.exe`) from a Linux host using the Mingw-w64 toolchain provided by the `depends` system.

Note that some distributions have the win32 threading model set by default. Gridcoin needs the posix threading model. To change this use

```
sudo update-alternatives --config x86_64-w64-mingw32-g++
sudo update-alternatives --config x86_64-w64-mingw32-gcc
```

and set the posix threading model for each before you get started. Note that the exact config will vary by distribution. The above is for Ubuntu.

### Step 1: Build Dependencies

```bash
cd depends
make HOST=x86_64-w64-mingw32 -j $(nproc)
cd ..
```

### Step 2: Configure

```bash
rm -rf build_win64

cmake -B build_win64 \
    --toolchain depends/x86_64-w64-mingw32/toolchain.cmake \
    -DENABLE_GUI=ON \
    -DUSE_QT6=ON \
    -DENABLE_UPNP=ON \
    -DENABLE_TESTS=ON \
    -DSYSTEM_XXD=ON \
    -DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/wine \
    -DCMAKE_EXE_LINKER_FLAGS="-static" \
    -DCMAKE_BUILD_TYPE=\< Release | RelWithDebInfo | Debug - see table below \>
```

### Step 3: Build & Test

```bash
cmake --build build_win64 -j $(nproc)

ctest --test-dir build_win64
```

### Step 4: Install (Create Installer Package for Windows)

```
cpack -G NSIS64 --config build_win64/CPackConfig.cmake -B build_win64
```

This installer file will be output to `build_win64/gridcoin-<release>-win64-setup.exe`.

-----

## CMake Options Quick Reference

If you are familiar with the legacy Autotools build system (`./configure`), this table maps common flags to their CMake equivalents.

| Feature | Autotools Flag | CMake Option |
| :--- | :--- | :--- |
| **Debug Build** | `--enable-debug` | `-DCMAKE_BUILD_TYPE=Debug` |
| **Release with Debug Info** | *(default)* | `-DCMAKE_BUILD_TYPE=RelWithDebInfo` |
| **Release Build** | *(default + strip)* | `-DCMAKE_BUILD_TYPE=Release` |
| **GUI** | `--with-gui` | `-DENABLE_GUI=ON` |
| **No GUI** | `--without-gui` | `-DENABLE_GUI=OFF` |
| **Multiprocess (split GUI/node)** | *(n/a)* | `-DENABLE_MULTIPROCESS=ON` (see [multiprocess.md](multiprocess.md)) |
| **UPnP** | `--with-miniupnpc` | `-DENABLE_UPNP=ON` |
| **QR Code** | `--with-qrencode` | `-DENABLE_QRENCODE=ON` |
| **Tests** | `--enable-tests` | `-DENABLE_TESTS=ON` |
| **DBus** | `--with-dbus` | `-DUSE_DBUS=ON` |
| **Hardening** | `--enable-hardening` | `-DENABLE_PIE=ON` |
| **Static Libs** | *(implicit in depends)* | `-DSTATIC_LIBS=ON` |

**Build type note:** The old Autotools default did not strip debug symbols and used `-O2` optimization, which corresponds to CMake's `-DCMAKE_BUILD_TYPE=RelWithDebInfo`. CMake's `-DCMAKE_BUILD_TYPE=Release` uses `-O2` and also strips debug symbols. Omitting `-DCMAKE_BUILD_TYPE` entirely results in no optimization and is not recommended.

**Common command equivalents:**

| Task | Old Command | New Command |
| :--- | :--- | :--- |
| **Run tests** | `make check` | `ctest --test-dir build` |
| **Verbose build** | `make V=1` | `cmake --build build --verbose` |

