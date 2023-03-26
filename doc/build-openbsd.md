OpenBSD build guide
======================
(updated for OpenBSD 7.0)

This guide describes how to build gridcoinresearchd, command-line utilities, and GUI on OpenBSD.

Preparation
-------------

Run the following as root to install the base dependencies for building:

```bash
pkg_add git gmake libtool libevent boost libzip
pkg_add qt5 libqrencode # (optional for enabling the GUI)
pkg_add autoconf # (select highest version, e.g. 2.71)
pkg_add automake # (select highest version, e.g. 1.16)
pkg_add python # (select highest version, e.g. 3.10)
```

Resource limits
-------------------

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

Preparation:
```bash

# Replace this with the autoconf version that you installed. Include only
# the major and minor parts of the version: use "2.71" for "autoconf-2.71p1".
export AUTOCONF_VERSION=2.71

# Replace this with the automake version that you installed. Include only
# the major and minor parts of the version: use "1.16" for "automake-1.16.3".
export AUTOMAKE_VERSION=1.16

./autogen.sh
```

To configure with gridcoinresearchd:
```bash
./configure --with-gui=no \
    MAKE=gmake
```

To configure with GUI:
```bash
./configure --with-gui=yes \
    MAKE=gmake
```

Build and run the tests:
```bash
gmake # use "-j N" here for N parallel jobs
gmake check
```
