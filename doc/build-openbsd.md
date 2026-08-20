# OpenBSD build guide

This guide details how to build Gridcoin on OpenBSD. This is not checked in CI/CD.

Verified on **OpenBSD 7.8 (amd64)** against Gridcoin 5.5.1.5: both the monolithic and the
multiprocess builds compile, and the multiprocess GUI attaches to a separately started
daemon over IPC. Toolchain used: clang 19.1.7, CMake 3.31.8, Boost 1.87, Qt6 6.8.3.

## 1. Install Dependencies

Run the following commands as root (or using `doas`) to install the necessary build tools and libraries.

```ksh
# Basic build requirements and libraries
pkg_add git cmake boost curl libzip miniupnpc

# Multiprocess (GUI/node separation, -multiprocess) only:
# Cap'n Proto supplies the code generator and the runtime the IPC layer is built on.
# Not needed for a monolithic build.
pkg_add capnproto

# If you prefer sudo over the native doas (optional)
pkg_add sudo
```

### Note on Sudo vs. Doas

OpenBSD uses `doas` by default. If you prefer `sudo`:

1.  Ensure your user is in the `wheel` group:
    ```ksh
    # Replace 'jco' with your username
    usermod -G wheel jco
    ```
2.  Edit the sudoers file using `visudo` and find the line:
    `## Uncomment to allow members of group wheel to execute any command`

    Uncomment the following line, which should be similar to
    `%wheel ALL=(ALL) ALL`

    and save the file with `:wq!`

## 2\. Clone the Repository

```ksh
git clone [https://github.com/gridcoin-community/Gridcoin-Research.git](https://github.com/gridcoin-community/Gridcoin-Research.git)
cd Gridcoin-Research
git checkout master
```

-----

## 3\. Build Configuration

Choose **Option A** (Headless/Daemon) or **Option B** (GUI Wallet).

### Option A: Headless (Daemon only)

Use this configuration for servers or command-line only environments.

```ksh
# Configure
cmake -B build -DENABLE_GUI=off -DENABLE_TESTS=on -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_PIE=on -DENABLE_UPNP=on

# Build
# Replace <# of cpus> with your core count, e.g., -j4
cmake --build build -j <# of cpus>

# Install (Optional)
doas cmake --install build
```

### Option B: GUI Wallet (Qt6)

Use this configuration for a desktop environment.

**Additional GUI Dependencies:**

```ksh
# Qt6 framework
pkg_add qt6

# Desktop Environment extras (Optional - for a full XFCE experience)
pkg_add xfce xfce-extras

# VMware helper (Optional - install only if running OpenBSD in a VMware VM)
pkg_add vmwh
```

**System Services:**
Qt6 and modern GUI applications require the message bus (dbus) to be running.

```ksh
rcctl enable messagebus
rcctl start messagebus
```

**Build & Install:**

```ksh
# Configure
# Note: Remove -DENABLE_QRENCODE=on if you do not have qrencode installed
cmake -B build -DENABLE_GUI=on -DENABLE_TESTS=on -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUSE_QT6=on -DENABLE_PIE=on -DENABLE_QRENCODE=on -DENABLE_UPNP=on

# Build
cmake --build build -j <# of cpus>

# Install (Optional)
# This will install binaries and assets (icons, desktop files)
doas cmake --install build
```

### Option C: Multiprocess (GUI/node separation)

Add `-DENABLE_MULTIPROCESS=ON` to either configuration above. This requires the
`capnproto` package **and one OpenBSD-specific adjustment.**

> **Raise `ulimit -d` before building with multiprocess.** OpenBSD caps a process's data
> segment through `login.conf`; the `default` class sets `datasize-cur=1536M`. The
> generated Cap'n Proto proxy sources are large (one preprocessed translation unit is
> ~9 MB), and clang needs more than 1.5 GB of heap for them. Without the raise the build
> dies part way through with:
>
> ```
> LLVM ERROR: out of memory
> c++: error: clang frontend command failed with exit code 134
> ```
>
> This is not a Gridcoin error and not a shortage of RAM -- it is the per-process limit.
> Users in the `staff` login class (`datasize-max=infinity`) can raise it in-shell; check
> with `ulimit -d` and your class with `getent passwd $(id -un)`.

```ksh
# Raise the per-process data limit for this shell, then configure and build.
ulimit -d 4194304

cmake -B build -DENABLE_GUI=on -DENABLE_TESTS=on -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DUSE_QT6=on -DENABLE_PIE=on -DENABLE_UPNP=on -DENABLE_MULTIPROCESS=ON

# Use FEWER jobs than cores for the multiprocess build. Several of the generated IPC
# sources each need well over 1 GB; on a 4 GB machine -j4 will thrash or be killed.
# -j2 completed reliably on a 4 vCPU / 4 GB VM.
cmake --build build -j 2
```

To run it, start the daemon first and then attach the GUI; the GUI does not spawn a node:

```ksh
./build/bin/gridcoinresearchd -multiprocess &
./build/bin/gridcoinresearch -multiprocess
```

The daemon logs the peer-credential mechanism in force at startup; on OpenBSD it reads
`getpeereid (connections from another OS user are refused)`. OpenBSD does define
`SO_PEERCRED`, but with a different payload than Linux, so Gridcoin uses `getpeereid(3)`
here.

## Notes on hardening flags

OpenBSD's clang **accepts but does not implement** `-fstack-clash-protection`: the driver
takes the flag and the frontend then reports `argument unused during compilation` for it.

The build detects this and does not apply the flag, so you will see it reported at
configure time rather than as several hundred warnings during compilation:

```
-- Hardening: -fstack-clash-protection not applied (compiler accepts no working
   implementation for this target)
```

Be aware that this particular mitigation is therefore **unavailable on OpenBSD** -- that
is a property of the toolchain, not something the build can work around. Stack canaries
(`-fstack-protector-all`) and control-flow protection (`-fcf-protection=full`) are applied
normally.

## 4\. Running Gridcoin

### If you followed option A (Daemon only)

If you installed Gridcoin, you can launch from the terminal.

```sh
gridcoinresearchd
```

If you did not install, you can run directly from the build folder. Assuming you are still in the Gridcoin-Research repo directory,

```
./build/bin/gridcoinresearchd
```

### If you followed option B (GUI)

If you installed Gridcoin, you can launch from the terminal or use the Desktop menu.

```sh
gridcoinresearch
```

If you did not install, you can run directly from the build folder. Assuming you are still in the Gridcoin-Research repo directory,

```
./build/bin/gridcoinresearch
```

