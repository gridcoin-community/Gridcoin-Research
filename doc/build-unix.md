UNIX BUILD NOTES
====================
Some notes on how to build Gridcoin in Unix.

(for OpenBSD specific instructions, see [build-openbsd.md](build-openbsd.md))

Note
---------------------
Always use absolute paths to configure and compile gridcoin and the dependencies,
for example, when specifying the path of the dependency:

	../dist/configure --enable-cxx --disable-shared --with-pic --prefix=$BDB_PREFIX

Here BDB_PREFIX must be an absolute path - it is defined using $(pwd) which ensures
the usage of the absolute path.

Preparing the Build
--------------------

Install git:
Ubuntu & Debian: `sudo apt-get install git`
openSUSE: `sudo zypper install git`
Clone the repository and cd into it:

```bash
git clone https://github.com/gridcoin-community/Gridcoin-Research
git checkout master
cd Gridcoin-Research
```
Go to platform specific instructions for the required dependencies below.

To Build
---------------------

```bash
./autogen.sh
./configure
make -j$(nproc --all) # core count by default, could be changed to the desired amount of threads
make install # optional
```

Or, to keep the source directory clean:
```bash
./autogen.sh
mkdir build
cd build
../configure
make -j$(nproc --all) # core count by default, could be changed to the desired amount of threads
```

This will build gridcoinresearch (Qt client) as well if the dependencies are met.

Dependencies
---------------------

These dependencies are required:

 Library     | Purpose          | Description
 ------------|------------------|----------------------
 libssl      | Crypto           | Random Number Generation, Elliptic Curve Cryptography
 libboost    | Utility          | Library for threading, data structures, etc
 libevent    | Networking       | OS independent asynchronous networking
 miniupnpc   | UPnP Support     | Firewall-jumping support
 libdb4.8    | Berkeley DB      | Wallet storage (only needed when wallet enabled)
 qt          | GUI              | GUI toolkit (only needed when GUI enabled)
 libqrencode | QR codes in GUI  | Optional for generating QR codes (only needed when GUI enabled)

For the versions used in the release, see [release-process.md](release-process.md) under *Fetch and build inputs*.

Memory Requirements
--------------------

C++ compilers are memory-hungry. It is recommended to have at least 1.5 GB of
memory available when compiling Gridcoin. On systems with less, gcc can be
tuned to conserve memory with additional CXXFLAGS:


    ./configure CXXFLAGS="--param ggc-min-expand=1 --param ggc-min-heapsize=32768"

Dependency Build Instructions: Ubuntu & Debian
----------------------------------------------
Build requirements:

    sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils

Options when installing required Boost library files:

1. On at least Ubuntu 14.04+ and Debian 7+ there are generic names for the
individual boost development packages, so the following can be used to only
install necessary parts of boost:

        sudo apt-get install libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev libboost-iostreams-dev libcurl4-gnutls-dev

2. If that doesn't work, you can install all boost development packages with:

        sudo apt-get install libboost-all-dev

BerkeleyDB is required for the wallet.

**For Ubuntu only:** db4.8 packages are available [here](https://launchpad.net/~bitcoin/+archive/bitcoin).
You can add the repository and install using the following commands:

    sudo apt-get install software-properties-common
    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get update
    sudo apt-get install libdb4.8-dev libdb4.8++-dev

Ubuntu and Debian have their own libdb-dev and libdb++-dev packages, but these will install
BerkeleyDB 5.1 or later, which break binary wallet compatibility with the distributed executables which
are based on BerkeleyDB 4.8. If you do not care about wallet compatibility,
pass `--with-incompatible-bdb` to configure.

See the section "Disable-wallet mode" to build Gridcoin without wallet.

Optional (see --with-miniupnpc and --enable-upnp-default):

    sudo apt-get install libminiupnpc-dev

Dependencies for the GUI: Ubuntu & Debian
-----------------------------------------

If you want to build gridcoinresearch, make sure that the required packages for Qt development
are installed. Qt 5 is necessary to build the GUI.
To build without GUI pass `--without-gui` to configure.

To build with Qt 5 (recommended) you need the following:

    sudo apt-get install libqt5gui5 libqt5core5a libqt5charts5-dev libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler

libqrencode (enabled by default, switch off by passing `--without-qrencode` to configure) can be installed with:

    sudo apt-get install libqrencode-dev

Once these are installed, they will be found by configure and a gridcoinresearch executable will be
built by default.

Dependency Build Instructions: OpenSUSE
----------------------------------------------
Build requirements:

    sudo zypper install -t pattern devel_basis
    sudo zypper install libtool automake autoconf pkg-config libopenssl-devel libevent-devel

Options when installing required Boost library files:

1. On at least openSUSE Leap there are generic names for the
individual boost development packages, so the following can be used to only
install necessary parts of boost:

Tumbleweed:

        sudo zypper install libboost_system1_*_0-devel libboost_filesystem1_*_0-devel libboost_chrono1_*_0-devel libboost_program_options1_*_0-devel libboost_test1_*_0-devel libboost_thread1_*_0-devel

Leap:

        sudo zypper install libboost_system1_61_0-devel libboost_filesystem1_61_0-devel libboost_chrono1_61_0-devel libboost_program_options1_61_0-devel libboost_test1_61_0-devel libboost_thread1_61_0-devel

2. If that doesn't work, you can install all boost development packages with:

Leap:

        sudo zypper install boost_1_61-devel

BerkeleyDB is required for the wallet.

      sudo zypper install libdb-4_8-devel

Optional (see --with-miniupnpc and --enable-upnp-default):

    sudo zypper install libminiupnpc-devel

Dependencies for the GUI: openSUSE
-----------------------------------------

If you want to build gridcoinresearch, make sure that the required packages for Qt development
are installed. Qt 5 is necessary to build the GUI.
To build without GUI pass `--without-gui` to configure.

To build with Qt 5 (recommended) you need the following:

    sudo zypper install libQt5Gui5 libQt5Core5 libQt5Charts5 libQt5DBus5 libQt5Network-devel libqt5-qttools-devel libqt5-qttools

Additionally for Tumbleweed:

    sudo zypper install libQt5Charts5-designer

libqrencode (enabled by default, switch off by passing `--without-qrencode` to configure) can be installed with:

    sudo zypper install qrencode-devel

Once these are installed, they will be found by configure and a gridcoinresearch executable will be
built by default.


Dependency Build Instructions: Alpine Linux
----------------------------------------------

Build requirements:

    apk add autoconf automake boost-dev build-base curl-dev db-dev libtool libzip-dev miniupnpc-dev openssl-dev pkgconfig

**Note:** Alpine Linux only includes Berkeley DB version 5.3 in the package repositories, so we must
run _configure_ with the following option:

    ./configure --with-incompatible-bdb

To build the wallet with Berkeley DB version 4.8, we need to compile the library from source. See the
[README](../depends/README.md) in the depends directory for one option.

Dependencies for the GUI: Alpine Linux
-----------------------------------------

To build the Qt GUI on Alpine Linux, we need these dependencies:

    apk add libqrencode-dev protobuf-dev qt5-qtbase-dev qt5-qtcharts-dev qt5-qtsvg-dev qt5-qttools-dev


Setup and Build Example: Arch Linux
-----------------------------------
This example lists the steps necessary to setup and build a command line only of the latest changes on Arch Linux:

    pacman -S git base-devel boost libevent python
    git clone https://github.com/gridcoin/Gridcoin-Research.git
    git checkout master
    cd Gridcoin-Research/
    ./autogen.sh
    ./configure --without-gui --without-miniupnpc
    make check

Note:
Enabling wallet support requires either compiling against a Berkeley DB newer than 4.8 (package `db`) using `--with-incompatible-bdb`,
or building and depending on a local version of Berkeley DB 4.8. The readily available Arch Linux packages are currently built using
`--with-incompatible-bdb` according to the [PKGBUILD](https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=gridcoinresearch).
As mentioned above, when maintaining portability of the wallet between the standard Gridcoin distributions and independently built
node software is desired, Berkeley DB 4.8 must be used.


ARM Cross-compilation
-------------------
These steps can be performed on, for example, an Ubuntu VM. The depends system
will also work on other Linux distributions, however the commands for
installing the toolchain will be different.

Make sure you install the build requirements mentioned above.
Then, install the toolchain and curl for 64 bit:

    sudo apt-get install g++-aarch64-linux-gnu

And for 32 bit:

    sudo apt-get install g++-arm-linux-gnueabihf curl

To build executables for 64 bit ARM:

    cd depends
    make HOST=aarch64-linux-gnu NO_QT=1
    cd ..
    ./configure --prefix=$PWD/depends/aarch64-linux-gnu --enable-glibc-back-compat --enable-reduce-exports LDFLAGS=-static-libstdc++
    make

For 32 bit configure with:

    ./configure --prefix=$PWD/depends/arm-linux-gnueabihf --enable-glibc-back-compat --enable-reduce-exports LDFLAGS=-static-libstdc++

For further documentation on the depends system see [README.md](../depends/README.md) in the depends directory.

Building on FreeBSD
--------------------

(Updated as of FreeBSD 11.0)

Clang is installed by default as `cc` compiler, this makes it easier to get
started than on [OpenBSD](build-openbsd.md). Installing dependencies:

    pkg install autoconf automake libtool pkgconf
    pkg install boost-libs openssl libevent
    pkg install gmake

You need to use GNU make (`gmake`) instead of `make`.
(`libressl` instead of `openssl` will also work)

For the wallet (optional):

    pkg install db5

This will give a warning "configure: WARNING: Found Berkeley DB other
than 4.8; wallets opened by this build will not be portable!", but as FreeBSD never
had a binary release, this may not matter. If backwards compatibility
with 4.8-built Gridcoin is needed follow the steps under "Berkeley DB" above.

Then build using:

    ./autogen.sh
    ./configure --with-incompatible-bdb BDB_CFLAGS="-I/usr/local/include/db5" BDB_LIBS="-L/usr/local/lib -ldb_cxx-5"
    gmake

*Note on debugging*: The version of `gdb` installed by default is [ancient and considered harmful](https://wiki.freebsd.org/GdbRetirement).
It is not suitable for debugging a multi-threaded C++ program, not even for getting backtraces. Please install the package `gdb` and
use the versioned gdb command e.g. `gdb7111`.
