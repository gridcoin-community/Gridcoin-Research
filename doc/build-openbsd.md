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
### Building BerkeleyDB

It is recommended to use Berkeley DB 4.8. 
If you have to build it yourself, you can use [the installation script included
in contrib/](/contrib/install_db4.sh) like so:

```bash
./contrib/install_db4.sh `pwd`
```

from the root of the repository. Then set `BDB_PREFIX` for the next section:

```bash
export BDB_PREFIX="$PWD/db4"
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

Make sure `BDB_PREFIX` is set to the appropriate paths from the above steps.

To configure with gridcoinresearchd:
```bash
./configure --with-gui=no \
    BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" \
    BDB_CFLAGS="-I${BDB_PREFIX}/include" \
    MAKE=gmake
```

To configure with GUI:
```bash
./configure --with-gui=yes \
    BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" \
    BDB_CFLAGS="-I${BDB_PREFIX}/include" \
    MAKE=gmake
```

Build and run the tests:
```bash
gmake # use "-j N" here for N parallel jobs
gmake check
```
