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
 libdb4.8    | Berkeley DB      | Wallet storage (only needed when wallet enabled)
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

BerkeleyDB is required for the wallet.

Ubuntu and Debian have their own `libdb-dev` and `libdb++-dev` packages, but these will install
Berkeley DB 5.1 or later. This will break binary wallet compatibility with the distributed executables, which
are based on BerkeleyDB 4.8. If you do not care about wallet compatibility,
pass `--with-incompatible-bdb` to configure.

**For Ubuntu only:** db4.8 packages are available [here](https://launchpad.net/~bitcoin/+archive/bitcoin).

You can add the repository and install using the following commands:

**For Ubuntu 18.04**

    sudo apt-get install software-properties-common
    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get update
    sudo apt-get install libdb4.8-dev libdb4.8++-dev

**For Ubuntu 20.04+ or Debian 10/Raspberry Pi**

    For Ubuntu 20.04+ users the db4.8 is not available on the Bitcoin PPA. Use the script in contrib/install_db4.sh
    to compile and install db4.8. You can use the script in your build location. For example if your build
    location is Gridcoin-Research/ then `./contrib/install_db4.sh $PWD`. Once complete, when running `./configure`, you
    must tell it about the location of the compiled db4.8 which you can do with the export line given when install_db4.sh
    is finished in the form of `export BDB_PREFIX='/compiled/location'`. Then run:
    `./configure BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" BDB_CFLAGS="-I${BDB_PREFIX}/include"`
    followed by whatever other flags you want such as --without-gui.



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

BerkeleyDB is required for the wallet.

    sudo zypper install libdb-4_8-devel

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

    apk add autoconf automake boost-dev build-base curl-dev db-dev libtool libzip-dev miniupnpc-dev openssl-dev pkgconfig

**Note:** Alpine Linux only includes Berkeley DB version 5.3 in the package repositories, so we must
run _configure_ with the following option:

    ./configure --with-incompatible-bdb

To build the wallet with Berkeley DB version 4.8, we need to compile the library from source. See the
[README](../depends/README.md) in the depends directory for one option.

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
    ./configure --prefix=$PWD/depends/aarch64-linux-gnu --enable-reduce-exports LDFLAGS=-static-libstdc++
    make

For 32 bit configure with:

    ./configure --prefix=$PWD/depends/arm-linux-gnueabihf --enable-reduce-exports LDFLAGS=-static-libstdc++

For further documentation on the depends system see [README.md](../depends/README.md) in the depends directory.

