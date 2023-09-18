UNIX BUILD NOTES
====================
Some notes on how to build Gridcoin in Unix.

(For BSD specific instructions, see build-\*bsd.md in this directory.)

Preparing the Build
-------------------

Install git:
* Ubuntu & Debian: `sudo apt install git`
* openSUSE: `sudo zypper install git`

Clone the repository and cd into it:

```bash
git clone https://github.com/gridcoin-community/Gridcoin-Research
cd Gridcoin-Research
git checkout master
```

Go to platform specific instructions for the required dependencies below.

To Build
--------

* With CMake:

  ```bash
  mkdir build && cd build
  # minimal configuration
  cmake ..
  # full GUI build with ccache
  cmake .. -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DENABLE_GUI=ON -DENABLE_QRENCODE=ON -DENABLE_UPNP=ON -DUSE_DBUS=ON
  ```

  ```bash
  cmake --build .  # use "-j N" for N parallel jobs
  cmake --install .  # optional
  ```

* With Autotools:

  ```bash
  ./autogen.sh
  ./configure
  make # use "-j N" for N parallel jobs
  make install # optional
  ```

Memory Requirements
-------------------

C++ compilers are memory-hungry. It is recommended to have at least 1.5 GB of
memory available when compiling Gridcoin. On systems with less, gcc can be
tuned to conserve memory with additional CXXFLAGS:

    export CXXFLAGS="--param ggc-min-expand=1 --param ggc-min-heapsize=32768"

Clang (often less resource hungry) can be used instead of gcc, which is used by default:

    export CXX=clang++
    export CC=clang

Dependency Build Instructions: Ubuntu & Debian
----------------------------------------------

Build requirements:

    sudo apt install build-essential cmake libboost-all-dev libcurl4-gnutls-dev libdb5.3++-dev libleveldb-dev libsecp256k1-dev libssl-dev libzip-dev pkgconf

Optional (see `ENABLE_UPNP / DEFAULT_UPNP` options for CMake and
`--with-miniupnpc / --enable-upnp-default` for Autotools):

    sudo apt install libminiupnpc-dev

Dependencies for the GUI: Ubuntu & Debian
-----------------------------------------

If you want to build Gridcoin with UI, make sure that the required packages for
Qt development are installed. Qt 5 is necessary to build the GUI.

To build with GUI pass `-DENABLE_GUI=ON` to CMake or `--with-gui=qt5` to the
configure script.

To build with Qt 5 you need the following:

    sudo apt install qtbase5-dev qttools5-dev

libqrencode (switch on by passing `-DENABLE_QRENCODE=ON` to CMake or
`--with-qrencode` to the configure script) can be installed with:

    sudo apt install libqrencode-dev

Dependency Build Instructions: OpenSUSE
---------------------------------------
Build requirements:

    sudo zypper install -t pattern devel_basis
    sudo zypper install cmake libtool automake autoconf pkg-config libopenssl-devel libevent-devel

Tumbleweed:

    sudo zypper install libboost_system1_*_0-devel libboost_filesystem1_*_0-devel libboost_test1_*_0-devel libboost_thread1_*_0-devel

Leap:

    sudo zypper install libboost_system1_61_0-devel libboost_filesystem1_61_0-devel libboost_test1_61_0-devel libboost_thread1_61_0-devel

If that doesn't work, you can install all boost development packages with:

Leap:

    sudo zypper install boost_1_61-devel

Optional (see `ENABLE_UPNP / DEFAULT_UPNP` options for CMake and
`--with-miniupnpc / --enable-upnp-default` for Autotools):

    sudo zypper install libminiupnpc-devel

Dependencies for the GUI: openSUSE
----------------------------------

If you want to build Gridcoin with UI, make sure that the required packages for
Qt development are installed. Qt 5 is necessary to build the GUI.

To build with GUI pass `-DENABLE_GUI=ON` to CMake or `--with-gui=qt5` to the
configure script.

To build with Qt 5 you need the following:

    sudo zypper install libQt5Gui5 libQt5Core5 libQt5DBus5 libQt5Network-devel libqt5-qttools-devel libqt5-qttools

libqrencode (switch on by passing `-DENABLE_QRENCODE=ON` to CMake or
`--with-qrencode` to the configure script) can be installed with:

    sudo zypper install qrencode-devel

Dependency Build Instructions: Alpine Linux
-------------------------------------------

Build requirements:

    apk add autoconf automake boost-dev build-base cmake curl-dev db-dev leveldb-dev libtool libsecp256k1-dev libzip-dev miniupnpc-dev openssl-dev pkgconf

Dependencies for the GUI: Alpine Linux
--------------------------------------

To build the Qt GUI on Alpine Linux, we need these dependencies:

    apk add libqrencode-dev qt5-qtbase-dev qt5-qttools-dev


Setup and Build Example: Arch Linux
-----------------------------------
This example lists the steps necessary to setup and build a command line only of the latest changes on Arch Linux:

    pacman -S git base-devel boost cmake db5.3 leveldb curl libsecp256k1

    git clone https://github.com/gridcoin/Gridcoin-Research.git
    git checkout master
    cd Gridcoin-Research/

    mkdir build && cd build
    cmake ..
    cmake --build .

ARM Cross-compilation
---------------------

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

