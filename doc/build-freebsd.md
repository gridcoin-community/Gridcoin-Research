Building on FreeBSD
--------------------

**Updated for FreeBSD [12.2](https://www.freebsd.org/releases/12.2R/announce.html)**

This guide describes how to build gridcoinresearchd, command-line utilities, and GUI on FreeBSD.

Preparing the Build
--------------------

Install the required dependencies the usual way you [install software on FreeBSD](https://www.freebsd.org/doc/en/books/handbook/ports.html) - either with `pkg` or via the Ports collection. The example commands below use `pkg` which is usually run as `root` or via `sudo`. If you want to use `sudo`, and you haven't set it up: [use this guide](http://www.freebsdwiki.net/index.php/Sudo%2C_configuring) to setup `sudo` access on FreeBSD.
#### General Dependencies
```bash
pkg install autoconf automake boost-libs git gmake libevent libtool pkgconf db5 openssl libzip

```
---
#### GUI Dependencies
```bash
pkg install qt5 libqrencode
```

---
#### Test Suite Dependencies
There is an included test suite that is useful for testing code changes when developing.
To run the test suite (recommended), you will need to have Python 3 installed:

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
This explicitly enables wallet support and disables the GUI.
```bash
./autogen.sh
./configure --with-gui=no --with-incompatible-bdb \
    BDB_LIBS="-ldb_cxx-5" \
    BDB_CFLAGS="-I/usr/local/include/db5" \
    MAKE=gmake
```

##### No Wallet or GUI
``` bash
./autogen.sh
./configure --without-wallet --with-gui=no MAKE=gmake
```

### 2. Compile
**Important**: Use `gmake` (the non-GNU `make` will exit with an error).

```bash
gmake # use "-j N" for N parallel jobs
gmake check # Run tests if Python 3 is available
```
