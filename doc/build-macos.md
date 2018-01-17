MacOS Build and Run Instructions

The commands in this guide should be executed in a Terminal application. The built-in one is located in /Applications/Utilities/Terminal.app.

Preparation:

Install the MacOS command line tools:

xcode-select --install

When the popup appears, click Install.

Then install Homebrew.

Install Dependencies:

brew install automake berkeley-db4 libtool boost --c++11 miniupnpc openssl pkg-config qt
If you want to build the disk image with make deploy (.dmg / optional), you need RSVG

brew install librsvg 	(to install RSVG)


To Build Gridcoin:

Clone the Gridcoin source code and cd into Gridcoin-Research with;

 git clone https://github.com/gridcoin/Gridcoin-Research
 cd Gridcoin-Research


Configure and build the headless gridcoin binaries as well as the GUI (if Qt is found).

You can disable the GUI build by passing --without-gui to configure.

./autogen.sh
./configure
make

make clean 	[cleans out previous builds!!!!!! Do this between versions]

The daemon binary is placed in src/ and the gui client is found in src/qt/ . Run the gui client for testnet for example with...

./src/qt/gridcoinresearch -testnet  (or)  
./src/qt/gridcoinresearch -printtoconsole -debug=true -testnet

It is recommended to build and run the unit tests:

make check

You can also create a .dmg that contains the .app bundle (optional):

make deploy

To have terminal give full readout:      make V=1 -j #number_of_cores_whatever >& build.log
