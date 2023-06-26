# macOS Build Instructions and Notes

**Updated for MacOS [11.2](https://www.apple.com/macos/big-sur/)**

This guide describes how to build gridcoinresearchd, command-line utilities, and GUI on macOS

**Note:** The following is for Intel Macs only!

## Preparation
The commands in this guide should be executed in a Terminal application.
macOS comes with a built-in Terminal located in:

```
/Applications/Utilities/Terminal.app
```
### 1. Xcode Command Line Tools

The Xcode Command Line Tools are a collection of build tools for macOS.
These tools must be installed in order to build Gridcoin from source.

To install, run the following command from your terminal:

```shell
xcode-select --install
```

Upon running the command, you should see a popup appear.
Click on `Install` to continue the installation process.

### 2. Homebrew Package Manager

Homebrew is a package manager for macOS that allows one to install packages from the command line easily.
While several package managers are available for macOS, this guide will focus on Homebrew as it is the most popular.
Since the examples in this guide which walk through the installation of a package will use Homebrew, it is recommended that you install it to follow along.
Otherwise, you can adapt the commands to your package manager of choice.

To install the Homebrew package manager, see: https://brew.sh

Note: If you run into issues while installing Homebrew or pulling packages, refer to [Homebrew's troubleshooting page](https://docs.brew.sh/Troubleshooting).

### 3. Install Required Dependencies

The first step is to download the required dependencies.
These dependencies represent the packages required to get a barebones installation up and running.
To install, run the following from your terminal:

```shell
brew install cmake
# or
brew install automake libtool

brew install berkeley-db@5 boost curl leveldb libzip openssl pkgconf secp256k1
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:/usr/local/opt/openssl@3/lib/pkgconfig"
```

### 4. Clone Gridcoin repository

`git` should already be installed by default on your system.
Now that all the required dependencies are installed, let's clone the Gridcoin repository to a directory.
All build scripts and commands will run from this directory.

``` bash
git clone https://github.com/gridcoin-community/Gridcoin-Research
cd Gridcoin-Research
git checkout master
```

### 5. Install Optional Dependencies

#### GUI Dependencies

###### Qt

Gridcoin includes a GUI built with the cross-platform Qt Framework.
To compile the GUI, we need to install `qt@5`.
Skip if you don't intend to use the GUI.

``` bash
brew install qt@5
```

Ensure that the `qt@5` package is installed, not the `qt` package.
If 'qt' is installed, the build process will fail.
if installed, remove the `qt` package with the following command:

``` bash
brew uninstall qt
```

Note: Building with Qt binaries downloaded from the Qt website is not officially supported.
See the notes in [Bitcoin#7714](https://github.com/bitcoin/bitcoin/issues/7714).

###### qrencode

The GUI can encode addresses in a QR Code. To build in QR support for the GUI, install `qrencode`.
Skip if not using the GUI or don't want QR code functionality.

``` bash
brew install qrencode
```
---

#### Port Mapping Dependencies

###### miniupnpc

miniupnpc may be used for UPnP port mapping.
Skip if you do not need this functionality.

``` bash
brew install miniupnpc
```

---

#### Test Suite Dependencies

There is an included test suite that is useful for testing code changes when developing.
To run the test suite (recommended), you will need to have the following packages installed:

* With CMake:

  ```bash
  brew install vim
  ```

* With Autotools:

  ```bash
  brew install python
  ```

---

#### Deploy Dependencies

You can deploy a `.dmg` containing the Gridcoin application using `make deploy`.
This command depends on a couple of python packages, so it is required that you have `python` installed.

Ensuring that `python` is installed, you can install the deploy dependencies by running the following commands in your terminal:

``` bash
brew install librsvg
```

``` bash
pip3 install ds_store mac_alias
```

## Build Gridcoin

1. Prepare the assembly code (requires Perl):

   ```shell
   pushd src
   ../contrib/nomacro.pl
   popd
   ```

2. Configure and build the Gridcoin binaries:

   You can enable the GUI build by passing `-DENABLE_GUI=ON` to CMake or
   `--with-gui=qt5` to the configure script.

   * With CMake:

     ```shell
     mkdir build && cd build

     cmake ..
     # or, to configure with GUI
     cmake .. -DENABLE_GUI=ON -DQt5_DIR=/usr/local/opt/qt5/lib/cmake/Qt5

     cmake --build .
     ```
   * With Autotools:

     ```shell
     ./autogen.sh
     ./configure
     make
     ```

3. It is recommended to build and run the unit tests:

   * With CMake:

     ```shell
     cmake .. -DENABLE_TESTS=ON
     cmake --build .
     ctest .
     ```

   * With Autotools:

     ```shell
     make check
     ```

4. You can also create a  `.dmg` that contains the `.app` bundle (optional):

   * With Autotools:

     ```shell
     make deploy
     ```

## Running

The daemon binary is placed in _src/_ and the GUI client is found in _src/qt/_.
For example, to run the GUI client for production or testnet:

**Important:** [Please read this before using testnet.](https://gridcoin.us/wiki/testnet)

```shell
./src/qt/gridcoinresearch
./src/qt/gridcoinresearch -testnet
./src/qt/gridcoinresearch -printtoconsole -debug -testnet
```

The first time you run Gridcoin, it will start downloading the blockchain. This process could
take several hours.

By default, blockchain and wallet data files will be stored in:

``` bash
/Users/${USER}/Library/Application Support/GridcoinResearch
```

Before running, you may create an empty configuration file:
```shell
mkdir -p "/Users/${USER}/Library/Application Support/GridcoinResearch"

touch "/Users/${USER}/Library/Application Support/GridcoinResearch/gridcoinresearch.conf"

chmod 600 "/Users/${USER}/Library/Application Support/GridcoinResearch/gridcoinresearch.conf"
```

You can monitor the download process by looking at the debug.log file:
```shell
tail -f $HOME/Library/Application\ Support/GridcoinResearch/debug.log
```
