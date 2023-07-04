Building on FreeBSD
--------------------

**Updated for FreeBSD [12.2](https://www.freebsd.org/releases/12.2R/announce.html)**

This guide describes how to build gridcoinresearchd, command-line utilities, and GUI on FreeBSD.

Preparing the Build
--------------------

Install the required dependencies the usual way you [install software on FreeBSD](https://www.freebsd.org/doc/en/books/handbook/ports.html) - either with `pkg` or via the Ports collection. The example commands below use `pkg` which is usually run as `root` or via `sudo`. If you want to use `sudo`, and you haven't set it up: [use this guide](http://www.freebsdwiki.net/index.php/Sudo%2C_configuring) to setup `sudo` access on FreeBSD.

#### General Dependencies

```bash
pkg install cmake
# or
pkg install autoconf automake gmake libtool

pkg install boost-libs curl db5 leveldb libzip openssl pkgconf secp256k1
```

---
#### GUI Dependencies

```bash
pkg install qt5 libqrencode
```

---
#### Test Suite Dependencies

There is an included test suite that is useful for testing code changes when developing.
To run the test suite (recommended), you will need to have the following packages installed:

* With CMake:

  ```bash
  pkg install vim
  ```

* With Autotools:

  ```bash
  pkg install python3
  ```

Clone the repository and cd into it:

``` bash
git clone https://github.com/gridcoin-community/Gridcoin-Research
cd Gridcoin-Research
git checkout master
```

To Build
---------------------
### 1. Configuration

There are many ways to configure Gridcoin, here are a few common examples:

##### Wallet Support, No GUI:
This configuration does not enable the GUI.

* With CMake:

  ```bash
  mkdir build && cd build
  cmake ..
  ```

* With Autotools:

  ```bash
  ./autogen.sh
  ./configure --with-gui=no \
  	MAKE=gmake
  ```

### 2. Compile

* With CMake:

  ```bash
  cmake --build . # use "-j N" for N parallel jobs
  ctest . # Run tests
  ```

* With Autotools:

  ```bash
  gmake # use "-j N" for N parallel jobs
  ```

### 3. Test

* With CMake:

  ```bash
  cmake .. -DENABLE_TESTS=ON
  cmake --build .
  ctest .
  ```

* With Autotools:

  ```bash
  gmake check
  ```
