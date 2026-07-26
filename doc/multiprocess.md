# Multiprocess mode (split GUI / node)

Gridcoin can run the GUI and the node (daemon) as **two separate processes** that
communicate over a local IPC socket, instead of the default **monolithic** mode
where the GUI runs the node in-process. This is RFC #2937.

Multiprocess mode is **opt-in at both build time and run time**:

- The binaries must be built with multiprocess support (`ENABLE_MULTIPROCESS`; see
  [Building with multiprocess support](#building-with-multiprocess-support) below).
- Each process must be started with the `-multiprocess` flag.

For the internal architecture, see [multiprocess_design.md](multiprocess_design.md).

## Why run split?

- The node keeps running when you close the GUI — attach and detach the GUI on
  demand.
- Run the node headless (or as a service) and only open the GUI when you need it.
- Crash isolation: a GUI problem doesn't take the node (and its wallet and staking)
  down with it.

## How it works (in brief)

When started with `-multiprocess`, the **daemon** listens on an `AF_UNIX` socket,
`node.sock`, in its data directory and serves the GUI over IPC (Cap'n Proto). The
**GUI**, also started with `-multiprocess`, connects to that socket instead of
running the node in-process.

- **Connect-only.** The GUI does **not** start the daemon. Start the daemon first,
  then the GUI.
- **Same data directory.** Both processes must use the same `-datadir`; that is how
  the GUI finds `node.sock`.
- **Authentication.** On start, the daemon writes a random cookie, `ipc.cookie`, in
  the data directory (owner-readable only); the GUI reads it to authenticate. The
  socket and the cookie are created with owner-only permissions.
- **Matching builds.** The GUI and daemon must be the same build — the connection
  handshake rejects mismatched IPC schema/protocol versions. Use `gridcoinresearch`
  and `gridcoinresearchd` from the same release.
- **Separate logs.** The GUI writes its own log, `debug_gui.log`, alongside the
  node's `debug.log` (override with `-guilogfile`). The two may not share a file.

Availability: Linux, and Windows 10 (1803+) / Windows 11, which provide `AF_UNIX`.

## Running

### Linux

1. **Start the daemon** with `-multiprocess`:

   ```bash
   gridcoinresearchd -multiprocess -datadir=/path/to/data -daemon
   ```

   Wait until it is serving — the node's `debug.log` shows a line like:

   ```
   IPC: serving GUI connections on unix:/path/to/data/node.sock
   ```

2. **Start the GUI** with `-multiprocess` and the **same** data directory:

   ```bash
   gridcoinresearch -multiprocess -datadir=/path/to/data
   ```

   It connects to the running daemon. If no daemon is listening, the GUI reports
   that it could not connect — it will not start one for you.

(You can omit `-datadir` on both if you use the default data directory.)

### Windows

The Windows daemon has no Unix-style `-daemon` background mode, and — importantly —
it must run **as the same Windows user account that runs the GUI**. The IPC socket
(`node.sock`) and the cookie (`ipc.cookie`) are created in that user's data
directory with owner-only NTFS permissions, so a GUI running as a different user
(or a daemon running as `LocalSystem`) cannot read them.

The practical setup today:

1. Create a **Scheduled Task** (Task Scheduler) that runs
   `gridcoinresearchd.exe -multiprocess` **as your user account** — for example,
   triggered *At log on* of your account, with *Run only when user is logged on*.
   Do **not** configure it to run as `SYSTEM` / `LocalSystem`.
2. Start the GUI normally with `-multiprocess` (same account, same data directory).

Do not run the daemon as a conventional Windows **service** under `LocalSystem`:
the GUI, running as you, would not share the data directory or the socket's ACL.

> **Future work:** the Windows installer may offer to enable multiprocess mode and
> create this scheduled task for you at install time. Until then, set it up manually
> as above.

## Stopping

The key mental model: **the GUI and the daemon are independent processes.**

- **Closing the GUI does not stop the daemon** — that is the whole point of split
  mode. The node keeps running (staking, syncing, serving RPC).
- The command `gridcoinresearchd ... stop` stops the **daemon**, not the GUI. (In
  monolithic mode `stop` stops everything; in split mode it stops only the node.)

### Stop only the GUI (leave the node running)

- Close the GUI window, **or**
- Linux: send it `SIGTERM` — `kill -TERM <gui-pid>`. The GUI installs a handler that
  turns `SIGTERM`/`SIGINT` into a clean shutdown.
- Windows: close the window, or `taskkill /IM gridcoinresearch.exe` (a graceful
  close request; avoid `/F`, which is a hard kill).

### Stop the node

- `gridcoinresearchd -multiprocess -datadir=/path/to/data stop` (RPC), **or**
- send the daemon `SIGTERM` (Linux) / stop its scheduled task (Windows).

If the GUI is connected when the daemon stops, it detects the lost connection and
exits cleanly on its own.

### Running the daemon as a service (Linux)

You can run the daemon under `systemd` (or any supervisor) with `-multiprocess` and
attach the GUI on demand. A minimal unit:

```ini
[Unit]
Description=Gridcoin daemon (multiprocess)
After=network-online.target

[Service]
User=<your-user>
ExecStart=/usr/local/bin/gridcoinresearchd -multiprocess -datadir=/path/to/data -printtoconsole
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Run the daemon under the **same user** whose account will run the GUI (so the data
directory and socket permissions line up), then start the GUI with `-multiprocess`.
Stopping the service stops the node (a connected GUI then exits); to stop just the
GUI, close it or `SIGTERM` it as above.

## Troubleshooting

- **"Could not connect to the Gridcoin daemon … node.sock: connection refused."**
  The daemon is not running yet, was not started with `-multiprocess`, is using a
  different `-datadir`, or (Windows) is running as a different user. Start the daemon
  first and confirm `IPC: serving …` appears in its `debug.log`.
- **Version/schema mismatch on connect.** The GUI and daemon are different builds.
  Use matching `gridcoinresearch` / `gridcoinresearchd` binaries. If only the commit
  differs (not the schema), the GUI still connects and just logs a mixed-build
  warning; suppress it per instance with `-nobuildwarn`.
- **"The wallet in this data directory appears to have changed…"** The GUI binds, per
  data directory, to the wallet the daemon is serving (a per-wallet identity kept in
  `wallet.dat`). It shows this prompt when a *different* wallet answers — e.g. you
  replaced or restored `wallet.dat`. Choose **Trust this wallet** to accept and
  remember the new one, or **Quit**. The check is chain-independent: a resync,
  re-genesis, or blockchain reset does **not** trigger it. For instances where you
  swap wallets deliberately (or start the GUI unattended), pass `-autotrustidentity`
  to re-bind silently instead of prompting. Note: the daemon itself always serves
  whatever `wallet.dat` is in its datadir — this prompt only stops the *GUI* from
  silently showing you a different wallet.
- **First multiprocess-capable load rewrites `wallet.dat`.** The one-time wallet
  identity (a random tag, not key material) is written on first load. If your
  `wallet.dat` is a symlink (e.g. shared across instances), back up the symlink
  *target*.
- **The GUI won't share a log with the node.** In `-multiprocess` mode the GUI uses
  `debug_gui.log`; set `-guilogfile` to a distinct path if you have overridden the
  node's `-debuglogfile`.

# Building with multiprocess support

The runtime behavior above is available only in binaries built with the
`ENABLE_MULTIPROCESS` option. Turning the flag on wires in the Cap'n Proto
dependency and the libmultiprocess runtime + `mpgen` code generator that the IPC
layer is built on.

## What it pulls in

- **Cap'n Proto** — the serialization runtime plus the `capnp` / `capnpc-c++` code
  generators. This is an external dependency (system package, or the depends
  `capnp` recipe).
- **libmultiprocess** — the proxy runtime plus `mpgen`, the generator that turns an
  `interfaces` `.capnp` schema into client/server proxy classes. This is **vendored
  in-tree** as a git subtree at `src/ipc/libmultiprocess` and compiled by the
  Gridcoin build itself (like Bitcoin Core); it is not a system or depends runtime
  package.

`WITH_EXTERNAL_LIBMULTIPROCESS` (default `OFF`) can be set to `ON` to link an
external libmultiprocess instead of the subtree — mainly useful when developing
libmultiprocess itself.

## Native build

The flag is `ENABLE_MULTIPROCESS` (default `OFF`):

```bash
cmake -B build -DENABLE_MULTIPROCESS=ON -DENABLE_GUI=ON -DUSE_QT6=ON
cmake --build build -j "$(nproc)"
```

or through the build script:

```bash
./build_targets.sh TARGET=native ENABLE_MULTIPROCESS=true
```

### Native dependencies

`install_dependencies.sh` installs the system **Cap'n Proto** packages when
`ENABLE_MULTIPROCESS=true` is passed (so `build_targets.sh ... ENABLE_MULTIPROCESS=true`
with `SKIP_DEPS=false` sets them up). Nothing needs to be installed for
libmultiprocess — it is vendored and built from the subtree.

The libmultiprocess subtree requires **C++20** (the rest of Gridcoin is C++17);
`cmake/libmultiprocess.cmake` raises the standard to C++20 for the subtree's
translation units only.

## Depends build (cross-compile / static)

Build the depends tree with `MULTIPROCESS=1` to add the Cap'n Proto runtime and the
native code generators:

```bash
make -C depends HOST=x86_64-w64-mingw32 MULTIPROCESS=1
```

This builds, for the target host:

| Package                   | Provides                                                     |
| ------------------------- | ----------------------------------------------------------- |
| `native_capnp`            | build-machine `capnp` / `capnpc-c++` generators             |
| `native_libmultiprocess`  | build-machine `mpgen` (built from the in-tree subtree)       |
| `capnp`                   | target Cap'n Proto runtime library                          |

The libmultiprocess **runtime** is not a depends package — it is compiled in-tree
from the subtree, the same as in a native build. The generated `toolchain.cmake`
sets `ENABLE_MULTIPROCESS=ON` and points `MPGEN_EXECUTABLE` / `CAPNP_EXECUTABLE` /
`CAPNPC_CXX_EXECUTABLE` at the tools it built, so the subsequent project configure
needs no extra flags:

```bash
cmake -B build --toolchain depends/x86_64-w64-mingw32/toolchain.cmake
```

## Updating the vendored libmultiprocess

`src/ipc/libmultiprocess` is a git subtree. To update it:

```bash
git subtree pull --prefix src/ipc/libmultiprocess \
    https://github.com/bitcoin-core/libmultiprocess.git <commit> --squash
```

### Version pinning

The Cap'n Proto version (`depends/packages/native_capnp.mk`) must stay compatible
with the vendored libmultiprocess commit: libmultiprocess's generated code embeds a
Cap'n Proto version check and the build fails with *"Version mismatch between
generated code and library headers"* if they diverge. The current pins are Cap'n
Proto **1.5.0** and libmultiprocess **3f221b5**. When bumping the subtree, bump
`native_capnp.mk` (and the system package, which tracks the distro's Cap'n Proto)
to a compatible release.
