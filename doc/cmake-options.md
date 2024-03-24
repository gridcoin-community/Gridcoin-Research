## CMake Build Options

You can use GUI (`cmake-gui`) or TUI (`ccmake`) to browse and toggle all
available options:

```bash
mkdir build && cd build
cmake ..
ccmake .
```

### Common configurations

* Build with GUI, QR code support and DBus support:

  `cmake .. -DENABLE_GUI=ON -DENABLE_QRENCODE=ON -DUSE_DBUS=ON`

* Build with UPnP:

  `cmake .. -DENABLE_UPNP=ON -DDEFAULT_UPNP=ON`

* Enable PIE and disable assembler routines:

  `cmake .. -DENABLE_PIE=ON -DUSE_ASM=OFF`

* Build a static binary:

  `cmake .. -DSTATIC_LIBS=ON -DSTATIC_RUNTIME=ON`

* Build tests and docs, run `lupdate`:

  `cmake .. -DENABLE_DOCS=ON -DENABLE_TESTS=ON -DLUPDATE=ON`

* Build with system libraries:

  `cmake .. -DSYSTEM_BDB=ON -DSYSTEM_LEVELDB=ON -DSYSTEM_SECP256K1=ON -DSYSTEM_UNIVALUE=ON -DSYSTEM_XXD=ON`
