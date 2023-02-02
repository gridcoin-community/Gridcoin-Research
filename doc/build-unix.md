UNIX BUILD NOTES
====================
Some notes on how to build Gridcoin in Unix.

(For BSD specific instructions, see build-*bsd.md in this directory.)

Note
---------------------
Always use absolute paths to configure and compile Gridcoin and the dependencies,
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
cd Gridcoin-Research
git checkout master
```
Go to platform specific instructions for the required dependencies below.

To Build
---------------------

```bash
./autogen.sh
./configure
make # use "-j N" for N parallel jobs
make install # optional
```

Or, to keep the source directory clean:
```bash
./autogen.sh
mkdir build
cd build
../configure
make # use "-j N" for N parallel jobs
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
 qt          | GUI              | GUI toolkit (only needed when GUI enabled)
 libqrencode | QR codes in GUI  | Optional for generating QR codes (only needed when GUI enabled)
 libzip      | Zip Compression  | For Zip Compression and Decompression for snapshot and scraper related functions

For the versions used in the release, see [release-process.md](release-process.md) under *Fetch and build inputs*.

Memory Requirements
--------------------

C++ compilers are memory-hungry. It is recommended to have at least 1.5 GB of
memory available when compiling Gridcoin. On systems with less, gcc can be
tuned to conserve memory with additional CXXFLAGS:


    ./configure CXXFLAGS="--param ggc-min-expand=1 --param ggc-min-heapsize=32768"

Alternatively, clang (often less resource hungry) can be used instead of gcc, which is used by default:

    ./configure CXX=clang++ CC=clang


Dependency Build Instructions: Ubuntu & Debian
----------------------------------------------
Build requirements:

    sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils libzip-dev libfreetype-dev

**For Ubuntu 18.04 gcc8 is also required**

    sudo apt-get install gcc-8 g++-8

Now, you can either build from self-compiled [depends](/depends/README.md) or install the required dependencies:

        sudo apt-get install libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev libboost-iostreams-dev libcurl4-gnutls-dev

Optional (see --with-miniupnpc and --enable-upnp-default):

    sudo apt-get install libminiupnpc-dev

Dependencies for the GUI: Ubuntu & Debian
-----------------------------------------

If you want to build Gridcoin with UI, make sure that the required packages for Qt development
are installed. Qt 5 is necessary to build the GUI.
To build without GUI pass `--without-gui` to configure.

To build with Qt 5 you need the following:

    sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler

libqrencode (enabled by default, switch off by passing `--without-qrencode` to configure) can be installed with:

    sudo apt-get install libqrencode-dev

Once these are installed, they will be found by configure and a gridcoinresearch executable will be
built by default.

Dependency Build Instructions: OpenSUSE
----------------------------------------------
Build requirements:

    sudo zypper install -t pattern devel_basis
    sudo zypper install libtool automake autoconf pkg-config libopenssl-devel libevent-devel

Tumbleweed:

        sudo zypper install libboost_system1_*_0-devel libboost_filesystem1_*_0-devel libboost_test1_*_0-devel libboost_thread1_*_0-devel

Leap:

        sudo zypper install libboost_system1_61_0-devel libboost_filesystem1_61_0-devel libboost_test1_61_0-devel libboost_thread1_61_0-devel

If that doesn't work, you can install all boost development packages with:

Leap:

    sudo zypper install boost_1_61-devel

Optional (see --with-miniupnpc and --enable-upnp-default):

    sudo zypper install libminiupnpc-devel

Dependencies for the GUI: openSUSE
-----------------------------------------

If you want to build gridcoinresearch, make sure that the required packages for Qt development
are installed. Qt 5 is necessary to build the GUI.
To build without GUI pass `--without-gui` to configure.

To build with Qt 5 you need the following:

    sudo zypper install libQt5Gui5 libQt5Core5 libQt5DBus5 libQt5Network-devel libqt5-qttools-devel libqt5-qttools

libqrencode (enabled by default, switch off by passing `--without-qrencode` to configure) can be installed with:

    sudo zypper install qrencode-devel

Once these are installed, they will be found by configure and a gridcoinresearch executable will be
built by default.


Dependency Build Instructions: Alpine Linux
----------------------------------------------

Build requirements:

    apk add autoconf automake boost-dev build-base curl-dev libtool libzip-dev miniupnpc-dev openssl-dev pkgconfig

Dependencies for the GUI: Alpine Linux
-----------------------------------------

To build the Qt GUI on Alpine Linux, we need these dependencies:

    apk add libqrencode-dev protobuf-dev qt5-qtbase-dev qt5-qtsvg-dev qt5-qttools-dev


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
    ./configure --prefix=$PWD/depends/aarch64-linux-gnu --enable-reduce-exports LDFLAGS=-static-libstdc++
    make

For 32 bit configure with:

    ./configure --prefix=$PWD/depends/arm-linux-gnueabihf --enable-reduce-exports LDFLAGS=-static-libstdc++

For further documentation on the depends system see [README.md](../depends/README.md) in the depends directory.

