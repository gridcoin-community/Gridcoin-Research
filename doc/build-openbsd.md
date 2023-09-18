OpenBSD build guide
===================
(updated for OpenBSD 7.0)

This guide describes how to build gridcoinresearchd, command-line utilities, and GUI on OpenBSD.

Preparation
-----------

Run the following as root to install the base dependencies for building:

```bash
pkg_add cmake vim
# or
pkg_add autoconf automake gmake libtool python

pkg_add boost curl libzip leveldb pkgconf
pkg_add qt5 libqrencode  # optional for the GUI
```

Resource limits
---------------

If the build runs into out-of-memory errors, the instructions in this section
might help.

The standard ulimit restrictions in OpenBSD are very strict:

    data(kbytes)         1572864

This is, unfortunately, in some cases not enough to compile some `.cpp` files in the project,
(see [Bitcoin#6658](https://github.com/bitcoin/bitcoin/issues/6658)).
If your user is in the `staff` group the limit can be raised with:

    ulimit -d 3000000

The change will only affect the current shell and processes spawned by it. To
make the change system-wide, change `datasize-cur` and `datasize-max` in
`/etc/login.conf`, and reboot.

### Building Gridcoin

**Important**: use `gmake`, not `make`. The non-GNU `make` will exit with a horrible error.

To configure with gridcoinresearchd:

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

To configure with GUI:

* With CMake:

  ```bash
  mkdir build && cd build
  cmake -DENABLE_GUI=ON
  ```

* With Autotools:

  ```bash
  ./autogen.sh
  ./configure --with-gui=yes \
  	MAKE=gmake
  ```

Build and run the tests:

* With CMake:

  ```bash
  cmake --build .  # use "-j N" here for N parallel jobs
  ctest .
  ```

* With Autotools:

  ```bash
  gmake  # use "-j N" here for N parallel jobs
  gmake check
  ```
