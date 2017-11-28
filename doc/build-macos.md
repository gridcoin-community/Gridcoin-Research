Mac OS X Build Instructions and Notes
====================================
The commands in this guide should be executed in a Terminal application.
The built-in one is located in `/Applications/Utilities/Terminal.app`.

Preparation
-----------
Install the OS X command line tools:

`xcode-select --install`

When the popup appears, click `Install`.

Then install [Homebrew](https://brew.sh).

Dependencies
----------------------

    brew install automake berkeley-db4 libtool boost --c++11 miniupnpc openssl pkg-config qt

If you want to build the disk image with `make deploy` (.dmg / optional), you need RSVG

    brew install librsvg

NOTE: Building with Qt4 is still supported, however, could result in a broken UI. Building with Qt5 is recommended.

Build Gridcoin
------------------------

1. Clone the Gridcoin source code and cd into `Gridcoin-Research`

        git clone https://github.com/gridcoin/Gridcoin-Research
        cd Gridcoin-Research

2.  Build Gridcoin:

    Configure and build the headless gridcoin binaries as well as the GUI (if Qt is found).

    You can disable the GUI build by passing `--without-gui` to configure.

        ./autogen.sh
        ./configure
	make

    The daemon binary is placed in src/ and the gui client is found in src/qt/ . Run the gui client for testnet for example with
    	
	./gridcoinresearch -testnet

3.  It is recommended to build and run the unit tests:

        make check

4.  You can also create a .dmg that contains the .app bundle (optional):

        make deploy


