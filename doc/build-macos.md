MacOS Build and Run Instructions
================================
The commands in this guide should be executed in a Terminal application.
The built-in one is located in /Applications/Utilities/Terminal.app.

Preparation
-----------
Install the OS X command line tools:

    xcode-select --install

When the popup appears, click:

    Install

Then install [Homebrew](https://brew.sh).

Dependencies
------------

    brew install automake berkeley-db4 libtool boost --c++11 miniupnpc openssl pkg-config qt libqrencode

To build .app and .dmg files with, make deploy, you will need RSVG installed.

    brew install librsvg

NOTE: Building with Qt4 is no longer supported. Build with Qt5.

Build Gridcoin
--------------

1. Clone the Gridcoin source code and cd into "Gridcoin-Research".

    	 git clone https://github.com/gridcoin/Gridcoin-Research
    	 cd Gridcoin-Research

2.  Build Gridcoin:

    Configure and build the headless gridcoin binaries as well as the GUI (if Qt is found).

    Clean out previous builds!!!!!! Do this between version compiles:
    
    	 make clean
    
    You can disable the GUI build by passing `--without-gui` to configure.

        ./autogen.sh
        ./configure 
        make
        
    To have terminal give full readout if desired:

    	 make V=1 -j #number_of_cores_whatever >& build.log

    The daemon binary is placed in src/ and the gui client is found in src/qt/. 
    Run the gui client for production or testnet for examples with:
    
    	 ./src/qt/gridcoinresearch
         ./src/qt/gridcoinresearch -testnet
         ./src/qt/gridcoinresearch -printtoconsole -debug=true -testnet

3.  It is recommended to build and run the unit tests:

        make check

4. You can also create an .app and .dmg that can be found in "Gridcoin-Reasearch": 

        make deploy
     
5. Testnet operating info is found at [Using-Testnet](http://wiki.gridcoin.us/OS_X_Guide#Using_Testnet).
   To open the app in testnet mode:

        open -a  /your/path/to/gridcoinresearch.app --args -testnet
