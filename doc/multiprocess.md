# Multiprocess (IPC) build

Gridcoin is moving toward a split between the GUI process and the node process
(RFC #2937). The wallet/node logic is being factored behind `interfaces::`
boundaries so those boundaries can eventually be driven over IPC instead of
in-process function calls.

This document covers the **build toolchain** for that work. Turning the flag on
does **not** by itself change any runtime behavior yet — it only wires in the
Cap'n Proto dependency and the libmultiprocess runtime + `mpgen` code generator
so the IPC layer can be built on top in later phases.

## What it pulls in

- **Cap'n Proto** — the serialization runtime plus the `capnp` / `capnpc-c++`
  code generators. This is an external dependency (system package, or the depends
  `capnp` recipe).
- **libmultiprocess** — the proxy runtime plus `mpgen`, the generator that turns
  an `interfaces` `.capnp` schema into client/server proxy classes. This is
  **vendored in-tree** as a git subtree at `src/ipc/libmultiprocess` and compiled
  by the Gridcoin build itself (like Bitcoin Core); it is not a system or depends
  runtime package.

`WITH_EXTERNAL_LIBMULTIPROCESS` (default `OFF`) can be set to `ON` to link an
external libmultiprocess instead of the subtree — mainly useful when developing
libmultiprocess itself.

## Native build

The flag is `ENABLE_MULTIPROCESS` (default `OFF`):

```bash
cmake -B build -DENABLE_MULTIPROCESS=ON -DENABLE_GUI=ON -DUSE_QT6=ON
cmake --build build -j "$(nproc)"
```

or through the build script:

```bash
./build_targets.sh TARGET=native ENABLE_MULTIPROCESS=true
```

### Native dependencies

`install_dependencies.sh` installs the system **Cap'n Proto** packages when
`ENABLE_MULTIPROCESS=true` is passed (so `build_targets.sh ... ENABLE_MULTIPROCESS=true`
with `SKIP_DEPS=false` sets them up). Nothing needs to be installed for
libmultiprocess — it is vendored and built from the subtree.

The libmultiprocess subtree requires **C++20** (the rest of Gridcoin is C++17);
`cmake/libmultiprocess.cmake` raises the standard to C++20 for the subtree's
translation units only.

## Depends build (cross-compile / static)

Build the depends tree with `MULTIPROCESS=1` to add the Cap'n Proto runtime and
the native code generators:

```bash
make -C depends HOST=x86_64-w64-mingw32 MULTIPROCESS=1
```

This builds, for the target host:

| Package                   | Provides                                                     |
| ------------------------- | ----------------------------------------------------------- |
| `native_capnp`            | build-machine `capnp` / `capnpc-c++` generators             |
| `native_libmultiprocess`  | build-machine `mpgen` (built from the in-tree subtree)       |
| `capnp`                   | target Cap'n Proto runtime library                          |

The libmultiprocess **runtime** is not a depends package — it is compiled in-tree
from the subtree, the same as in a native build. The generated `toolchain.cmake`
sets `ENABLE_MULTIPROCESS=ON` and points `MPGEN_EXECUTABLE` / `CAPNP_EXECUTABLE` /
`CAPNPC_CXX_EXECUTABLE` at the tools it built, so the subsequent project configure
needs no extra flags:

```bash
cmake -B build --toolchain depends/x86_64-w64-mingw32/toolchain.cmake
```

## Updating the vendored libmultiprocess

`src/ipc/libmultiprocess` is a git subtree. To update it:

```bash
git subtree pull --prefix src/ipc/libmultiprocess \
    https://github.com/bitcoin-core/libmultiprocess.git <commit> --squash
```

### Version pinning

The Cap'n Proto version (`depends/packages/native_capnp.mk`) must stay compatible
with the vendored libmultiprocess commit: libmultiprocess's generated code embeds
a Cap'n Proto version check and the build fails with *"Version mismatch between
generated code and library headers"* if they diverge. The current pins are Cap'n
Proto **1.5.0** and libmultiprocess **3f221b5**. When bumping the subtree, bump
`native_capnp.mk` (and the system package, which tracks the distro's Cap'n Proto)
to a compatible release.
