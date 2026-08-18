# FreeBSD Build Guide

This guide details how to build Gridcoin on FreeBSD (14.x+).

Verified on **FreeBSD 15.0-RELEASE (amd64)** against Gridcoin 5.5.1.5: both the
monolithic and the multiprocess builds compile, and the multiprocess GUI attaches to a
separately started daemon over IPC. Toolchain used: clang 19.1.7, CMake 3.31.9,
Boost 1.88, Qt6 6.9.3.

## 1. System Preparation (First Run Only)

**Hostname Setup:**
FreeBSD requires the FQDN in the configuration. If you are setting this OS up fresh, then replace `hostname` and `domainname` with your desired names.

```sh
sudo sysrc hostname="hostname.domainname"
sudo hostname "hostname.domainname"
````

*Note: Ensure you add this hostname to `/etc/hosts` next to 127.0.0.1 and ::1 to prevent sudo delays.*

**Enable Desktop Services (VMware/XFCE):**
Required for mouse integration (`evdev`) and GUI permissions in a virtualized environment.

```sh
# Enable mouse/keyboard integration
echo "kern.evdev.rcpt_mask=6" | sudo tee -a /etc/sysctl.conf
sudo sysctl kern.evdev.rcpt_mask=6

# Enable DBus and VMware services
sudo sysrc dbus_enable="YES"
sudo sysrc hald_enable="YES"
sudo sysrc vmware_guest_dmp_enable="YES"
sudo sysrc vmware_guest_vmmemctl_enable="YES"
sudo sysrc vmware_guest_vmblock_enable="YES"
sudo sysrc vmware_guest_vmhgfs_enable="YES"
sudo sysrc vmware_guest_vmsync_enable="YES"
sudo sysrc vmware_guestd_enable="YES"
```

## 2\. Install Dependencies

Run the following commands to install the necessary build tools and libraries.

```sh
# Update package catalog
sudo pkg update

# Basic build requirements
# Note: 'boost-all' is required to ensure headers are found.
sudo pkg install git cmake boost-all libzip curl pkgconf miniupnpc

# Install Qt6 for the GUI Wallet
sudo pkg install qt6

# Multiprocess (GUI/node separation, -multiprocess) only:
# Cap'n Proto supplies the code generator and the runtime the IPC layer is built on.
# Not needed for a monolithic build.
sudo pkg install capnproto
```

> **Note on the Cap'n Proto package and FreeBSD 15.0-RELEASE.** `pkg` prints a warning
> that a kernel bug may prevent the library working as intended on 15.0-RELEASE, with a
> fix expected in 15.1. Gridcoin's IPC was exercised on 15.0-RELEASE regardless -- the
> daemon serves on the socket and the GUI attaches and runs -- so the warning did not
> prevent multiprocess from working here. It is reproduced so you know why `pkg` shows
> it, not because it is known to break Gridcoin.

## 3\. Clone the Repository

```sh
git clone [https://github.com/gridcoin-community/Gridcoin-Research.git](https://github.com/gridcoin-community/Gridcoin-Research.git)
cd Gridcoin-Research
git checkout master
```

-----

## 4\. Build Configuration

Choose **Option A** (Headless/Daemon)  or **Option B** (GUI Wallet).

### Option A: Headless (Daemon only)

Use this for servers or jails where no X11 is present.

```sh
cmake -B build \
-DENABLE_GUI=off \
-DENABLE_TESTS=on \
-DCMAKE_BUILD_TYPE=RelWithDebInfo \
-DENABLE_PIE=on \
-DENABLE_UPNP=on

cmake --build build -j$(sysctl -n hw.ncpu)

sudo cmake --install build
```

### Option B: GUI Wallet (Qt6) - Recommended

**Configure:**

> **`-DBoost_USE_DEBUG_RUNTIME=OFF` is no longer required.** Earlier revisions of this
> guide told you to pass it, and described it as accepting the system's release Boost
> "even when building with debug symbols" -- with `Release` offered as an alternative.
> That description was wrong: the failure had nothing to do with the build type, and
> `Release`, `RelWithDebInfo` and `Debug` all failed identically. CMake's FindBoost
> defaults `Boost_USE_DEBUG_RUNTIME` to `TRUE` (an MSVC-only concept), and Boost's own
> CMake config files honour it everywhere, so a distribution shipping only
> release-runtime variants -- FreeBSD's -- had every variant rejected with "No suitable
> build variant has been found". The tree now sets it `OFF` up front, so no flag is
> needed. Passing it explicitly is still honoured if you have a reason to.

```sh
cmake -B build \
-DENABLE_GUI=on \
-DENABLE_TESTS=on \
-DCMAKE_BUILD_TYPE=RelWithDebInfo \
-DUSE_QT6=on \
-DENABLE_PIE=on \
-DENABLE_UPNP=on
```

### Option C: Multiprocess (GUI/node separation)

Add `-DENABLE_MULTIPROCESS=ON` to either configuration above; it requires the
`capnproto` package. FreeBSD needs no other change -- its default per-process data limit
is large (`ulimit -d` reports 32 GB), so the memory-hungry generated Cap'n Proto sources
compile without adjustment.

```sh
cmake -B build \
-DENABLE_GUI=on \
-DENABLE_TESTS=on \
-DCMAKE_BUILD_TYPE=RelWithDebInfo \
-DUSE_QT6=on \
-DENABLE_PIE=on \
-DENABLE_UPNP=on \
-DENABLE_MULTIPROCESS=ON

cmake --build build -j$(sysctl -n hw.ncpu)
```

To run it, start the daemon first and then attach the GUI; the GUI does not spawn a node:

```sh
./build/bin/gridcoinresearchd -multiprocess &
./build/bin/gridcoinresearch -multiprocess
```

The daemon logs the peer-credential mechanism in force at startup; on FreeBSD it reads
`getpeereid (connections from another OS user are refused)`.

**Build:**
Use `sysctl` to automatically detect core count for parallel compilation. Pay attention to memory usage. General 1 GB per core is required, so if you have a smaller amount of memory, you may want to substitute your RAM in GB - 1 GB as the number of CPUs.

```sh
cmake --build build -j$(sysctl -n hw.ncpu)
```

**Install (Optional):**

```sh
sudo cmake --install build
```

## 5\. Running Gridcoin

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
