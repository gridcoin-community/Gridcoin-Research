# Multiprocess (IPC) build

Gridcoin is moving toward a split between the GUI process and the node process
(RFC #2937). The wallet/node logic is being factored behind `interfaces::`
boundaries so those boundaries can eventually be driven over IPC instead of
in-process function calls.

This document covers the **build toolchain** for that work. Turning the flag on
does **not** by itself change any runtime behavior yet — it only wires in the
Cap'n Proto + libmultiprocess dependencies and the `mpgen` code generator so the
IPC layer can be built on top in later phases.

## What it pulls in

- **Cap'n Proto** — the serialization runtime plus the `capnp` / `capnpc-c++`
  code generators.
- **libmultiprocess** — the proxy runtime plus `mpgen`, the generator that turns
  an `interfaces` `.capnp` schema into client/server proxy classes.

Gridcoin does **not** vendor an in-tree copy of libmultiprocess (unlike Bitcoin
Core, which builds it from a git subtree). Both a native build and a depends
build therefore consume an *external* libmultiprocess:

- **Native:** a system / `/usr/local` install (`WITH_EXTERNAL_LIBMULTIPROCESS=ON`,
  the default and currently the only supported mode).
- **Depends:** built by the depends recipes described below.

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
with `SKIP_DEPS=false` sets them up). libmultiprocess is **not** packaged by any
distribution, so it must be built from source once:

```bash
git clone https://github.com/bitcoin-core/libmultiprocess
cmake -B libmultiprocess/build -S libmultiprocess
cmake --build libmultiprocess/build
sudo cmake --install libmultiprocess/build   # installs libmultiprocess + mpgen
```

CMake then resolves Cap'n Proto and libmultiprocess via `find_package(... CONFIG)`
and locates `mpgen` on `PATH` (or via `-DMPGEN_EXECUTABLE=...`).

## Depends build (cross-compile / static)

Build the depends tree with `MULTIPROCESS=1` to add the Cap'n Proto +
libmultiprocess packages:

```bash
make -C depends HOST=x86_64-w64-mingw32 MULTIPROCESS=1
```

This builds, for the target host:

| Package                   | Provides                                             |
| ------------------------- | ---------------------------------------------------- |
| `native_capnp`            | build-machine `capnp` / `capnpc-c++` generators      |
| `native_libmultiprocess`  | build-machine `mpgen` generator                      |
| `capnp`                   | target Cap'n Proto runtime library                   |
| `libmultiprocess`         | target libmultiprocess runtime library               |

The generated `toolchain.cmake` then sets `ENABLE_MULTIPROCESS=ON` and points
`MPGEN_EXECUTABLE` / `CAPNP_EXECUTABLE` / `CAPNPC_CXX_EXECUTABLE` at the tools it
built, so the subsequent project configure needs no extra flags:

```bash
cmake -B build --toolchain depends/x86_64-w64-mingw32/toolchain.cmake
```

### Version pinning

The Cap'n Proto version (`depends/packages/native_capnp.mk`) and the
libmultiprocess commit (`depends/packages/native_libmultiprocess.mk`, reused by
`libmultiprocess.mk`) **must** stay compatible: libmultiprocess's generated code
embeds a Cap'n Proto version check and the build fails with *"Version mismatch
between generated code and library headers"* if they diverge. The current pins
are Cap'n Proto **1.5.0** and libmultiprocess **3f221b5**.
