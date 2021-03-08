# macOS Build Instructions and Notes

The commands in this guide should be executed in a Terminal application.
The built-in one is located at:
```
/Applications/Utilities/Terminal.app
```

## Preparation
Install the macOS command line tools:

```shell
xcode-select --install
```

When the popup appears, click `Install`.

Then install [Homebrew](https://brew.sh).

## Dependencies
```shell
brew install automake libtool boost miniupnpc openssl pkg-config python qt@5 qrencode libzip
```

If you run into issues, check [Homebrew's troubleshooting page](https://docs.brew.sh/Troubleshooting).

If you want to build the disk image with `make deploy` (.dmg / optional), you need RSVG:
```shell
brew install librsvg
```

...as well as [`macdeployqtplus`](../contrib/macdeploy/README.md) dependencies:
```shell
pip3 install ds_store mac_alias
```

#### Berkeley DB

It is recommended to use Berkeley DB 4.8. If you have to build it yourself,
you can use [this](/contrib/install_db4.sh) script to install it
like so:

```shell
./contrib/install_db4.sh .
```

from the root of the repository.

Also, the Homebrew package could be installed:

```shell
brew install berkeley-db4
```

## Build Gridcoin

1.  Clone the Gridcoin source code:
    ```shell
    git clone https://github.com/gridcoin-community/Gridcoin-Research
    cd Gridcoin-Research
    git checkout master
    ```

2.  Build Gridcoin:

    Prepare the assembly code (requires Perl):
    ```shell
    cd src/
    ../contrib/nomacro.pl
    cd ..
    ```

    Configure and build the headless Gridcoin binaries as well as the GUI (if Qt is found).
    ```shell
    ./autogen.sh
    ./configure
    make
    ```
    You can disable the GUI build by passing `--without-gui` to configure.

3.  It is recommended to build and run the unit tests:
    ```shell
    make check
    ```

4.  You can also create a  `.dmg` that contains the `.app` bundle (optional):
    ```shell
    make deploy
    ```

5.  Testnet participation info is found at [Using Testnet](http://wiki.gridcoin.us/OS_X_Guide#Using_Testnet).

    To open the app in testnet mode:
    ```shell
    open -a /your/path/to/gridcoinresearch.app --args -testnet
    ```

## Running

The daemon binary is placed in _src/_ and the GUI client is found in _src/qt/_.
For example, to run the GUI client for production or testnet:

```shell
./src/qt/gridcoinresearch
./src/qt/gridcoinresearch -testnet
./src/qt/gridcoinresearch -printtoconsole -debug -testnet
```

Before running, you may create an empty configuration file:
```shell
mkdir -p "/Users/${USER}/Library/Application Support/GridcoinResearch"

touch "/Users/${USER}/Library/Application Support/GridcoinResearch/gridcoinresearch.conf"

chmod 600 "/Users/${USER}/Library/Application Support/GridcoinResearch/gridcoinresearch.conf"
```

The first time you run Gridcoin, it will start downloading the blockchain. This process could
take several hours.

You can monitor the download process by looking at the debug.log file:
```shell
tail -f $HOME/Library/Application\ Support/GridcoinResearch/debug.log
```

## Notes
* Building with downloaded Qt binaries is not officially supported. See the notes in [#7714](https://github.com/bitcoin/bitcoin/issues/7714).
