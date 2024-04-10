Windows (MSYS2) Build Notes
===========================

This guide describes how to build gridcoinresearchd, command-line utilities, and GUI on Windows.

Preparing the Build
-------------------

First, install [MSYS2](https://www.msys2.org/). All commands below are supposed to be run in the **MSYS2 UCRT64** shell.

Run the following to install the base dependencies for building:

```bash
pacman -S make ninja pactoys
pacboy -S boost:p cmake:p curl:p libzip:p openssl:p toolchain:p
pacboy -S qrencode:p qt5-base:p qt5-tools:p  # optional for the GUI
```

To Build
--------

### 1. Configuration

To configure with gridcoinresearchd:

```bash
mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=OFF
```

To configure with GUI:

```bash
mkdir build && cd build
cmake .. -DENABLE_GUI=ON -DBUILD_SHARED_LIBS=OFF
```

> [!NOTE]
> See also: [CMake Build Options](cmake-options.md)

### 2. Compile

```bash
cmake --build .  # use "-j N" here for N parallel jobs
```

### 3. Test

```bash
cmake .. -DENABLE_TESTS=ON
cmake --build .
ctest .
```
