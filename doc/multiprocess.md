# Multiprocess mode (split GUI / node)

Gridcoin can run the GUI and the node (daemon) as **two separate processes** that
communicate over a local IPC socket, instead of the default **monolithic** mode
where the GUI runs the node in-process. This is RFC #2937.

Multiprocess mode is **opt-in at both build time and run time**:

- The binaries must be built with multiprocess support (`ENABLE_MULTIPROCESS`; see
  [Building with multiprocess support](#building-with-multiprocess-support) below).
- Each process must be started with the `-multiprocess` flag.

To run the core as an **unattended background service** (boot autostart + optional stake-only
autounlock) on Linux or Windows, see [running-unattended.md](running-unattended.md).

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

**Retrofitting an existing wallet** needs no migration: point the daemon at the data
directory you already use. Two things differ from a fresh install, and both are
covered in detail under [*RPC credentials*](#rpc-credentials) and
[*B. Retrofit*](#b-retrofit-an-existing-wallet-windows) below — the guidance is
platform-independent even though the procedure there is written for Windows:

* A data directory that already exists keeps its current permissions. We harden only
  what we create, so that a directory someone widened on purpose is not silently
  re-tightened. `node.sock` and `ipc.cookie` are owner-only either way.
* A config with no `rpcuser`/`rpcpassword` gets generated ones appended, because the
  daemon starts an RPC listener and refuses to run without them. Older configs contain
  only `addnode=` lines, since the monolithic GUI never needed credentials.

Stop the monolithic wallet before starting the daemon on the same data directory —
they do not share it, and the second one to start gets a lock error.

### Windows

The Windows daemon has no Unix-style `-daemon` background mode, and — importantly —
it must run **as the same Windows user account that runs the GUI**. The IPC socket
(`node.sock`) and the cookie (`ipc.cookie`) are created in that user's data
directory with owner-only NTFS permissions, so a GUI running as a different user
(or a daemon running as `LocalSystem`) cannot read them. This is the single mistake
that breaks a Windows multiprocess setup, so it is worth stating plainly: **never
run the core as SYSTEM/LocalSystem.**

You do not have to build the scheduled task by hand. The installer lays down
PowerShell scripts in `…\GridcoinResearch\windows\` that register it correctly,
and those are the supported route. Pick the procedure that matches your situation:

* **[A. Fresh install](#a-fresh-install-windows)** — no Gridcoin on this machine, or no
  existing data directory.
* **[B. Retrofit](#b-retrofit-an-existing-wallet-windows)** — you already run the
  ordinary (monolithic) Gridcoin wallet and want to move that same wallet to
  multiprocess.

Both end in the same place: a headless core started by Task Scheduler, and a GUI you
attach and detach at will. Everything below runs in an **elevated** PowerShell
(Run as administrator) — a boot-triggered task requires it.

Set the execution policy for the session first. A fresh Windows install blocks `.ps1`
files by default:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
cd "C:\Program Files\GridcoinResearch\windows"
```

> Set the policy in the session and call the scripts directly, as above. Do **not**
> wrap a call in `powershell.exe -ExecutionPolicy Bypass .\script -Arg "C:\Program
> Files\..."` — the outer shell re-parses the command line and splits the spaced path.

#### A. Fresh install (Windows)

1. **Run the installer.** It places `gridcoinresearch.exe` (GUI),
   `daemon\gridcoinresearchd.exe`, and the `windows\` scripts.

   The installer does **not** create the data directory, and that is deliberate: the
   binary creates it on first run and applies an owner-only DACL as it does so. A
   directory pre-created by anything else would inherit ordinary permissions instead.

2. **Add the Defender exclusion before the first sync.** `WriteBlockToDisk` opens,
   appends to and closes `blk*.dat` once per block, and Defender re-scans that
   multi-gigabyte file on every close. On a 2-core VM syncing blocks 1-100,000 from a
   LAN peer this measured **605s without the exclusion and 172s with it**, with
   `MsMpEng` falling from 84% CPU to 6%.

   ```powershell
   Add-MpPreference -ExclusionPath "$env:APPDATA\GridcoinResearch\blk*.dat"
   ```

   The wildcard is required: block files roll over at ~2GB, so a literal
   `blk0001.dat` stops covering the file being written after the first rollover.
   This excludes block data only -- `wallet.dat`, `database/` and `txleveldb` stay
   fully scanned. Skipping it breaks nothing; it just makes the initial sync ~3.5x
   slower, and the symptom (a wallet at low CPU while `MsMpEng` runs hot) looks like
   a wallet problem rather than a scanner one. Numbers in
   [`../contrib/windows/README.md`](../contrib/windows/README.md).

3. **Register and start the core task:**

   ```powershell
   .\Install-GridcoinCoreTask.ps1
   #   prompts for your Windows LOGIN password, so the task can start at boot
   Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Core-Start'
   ```

   This registers **Core-Start** (at boot, via a retry launcher) and **Core-Stop**
   (on demand), running as your own account.

   Do **not** pass `-DataDir` for a default install. The daemon resolves the Windows
   default itself (`%APPDATA%\GridcoinResearch`) and creates it owner-only; an
   explicit `-datadir` takes the branch that *refuses* to create a missing directory,
   so passing the default path is the one way to stop a fresh install from
   bootstrapping. Pass `-DataDir` only for a genuinely custom location, which the
   wallet's own chooser will have created already.

4. **Confirm the first start did what it should.** This is the whole hardening story
   in three checks:

   ```powershell
   $dd = "$env:APPDATA\GridcoinResearch"
   Get-Process gridcoinresearchd            # the core is up and stayed up
   icacls $dd                               # exactly your account + SYSTEM
   Select-String -Path "$dd\gridcoinresearch.conf" -Pattern '^rpcuser|^rpcpassword'
   ```

   Expect the data directory to list **only your account and `NT AUTHORITY\SYSTEM`**,
   with no `BUILTIN\Administrators` entry — the protected DACL drops the inherited
   one. Expect the config to contain a `rpcuser` and a 43–44 character random
   `rpcpassword`, generated on that first run (see *RPC credentials* below).

   > The first multiprocess-capable start does a one-time `wallet.dat` rewrite and may
   > exit. If `Get-Process gridcoinresearchd` shows nothing immediately after, run the
   > `Start-ScheduledTask … Core-Start` line once more.

5. **Attach the GUI:**

   ```powershell
   .\Start-GridcoinGui.ps1
   #   or: .\Start-GridcoinGui.ps1 -CreateShortcut   (Start-Menu entry)
   ```

   The GUI connects to the running core. It will not start one for you: if nothing is
   listening it says so and stops.

6. **Optional but recommended for an unattended machine:** a Group Policy shutdown
   script for a clean database flush, and stake-only autounlock. Both are covered in
   [`running-unattended.md`](running-unattended.md).

#### B. Retrofit an existing wallet (Windows)

Your existing data directory, wallet, and blockchain are reused **in place**. There is
no export/import step and no resync. The differences from a fresh install are the two
things that already exist on disk: the data directory and the config file.

1. **Stop the wallet completely.** Exit the GUI and confirm nothing is left running:

   ```powershell
   Get-Process gridcoinresearch, gridcoinresearchd -ErrorAction SilentlyContinue
   ```

   Both the core and the GUI hold the data directory; starting the core while the old
   monolithic wallet is up gives you a data-directory lock error, not a merge.

2. **Back up `wallet.dat`** before changing how the wallet is launched. Ordinary
   advice, but this is a good moment for it.

3. **Install the new build and register the task** — steps A1 and A2 above, unchanged.

4. **Expect these two differences from a fresh install:**

   * **The data directory keeps the permissions it already has.** Hardening is applied
     only when *we* create the directory. Yours already exists, so it is left exactly as
     configured — deliberately, so that anyone who widened it on purpose does not have
     that silently reverted. The IPC socket and cookie inside it are still owner-only
     regardless, since those are created fresh and protected explicitly. If you want the
     directory itself tightened, do it yourself:

     ```powershell
     icacls "$env:APPDATA\GridcoinResearch" /inheritance:r /grant:r "${env:USERNAME}:(OI)(CI)F" "SYSTEM:(OI)(CI)F"
     ```

     You will see a `WARN: … the data directory … is not restricted to this account` in
     `debug.log` until you do. That warning is the check working, not a failure.

   * **RPC credentials are added to your existing config if it has none.** A config
     written by an older build contains only `addnode=` lines — the monolithic GUI never
     needed RPC credentials, so nobody noticed they were missing. The daemon does need
     them, and rather than refusing to start it appends a generated `rpcuser` and random
     `rpcpassword`, then continues. Nothing already in your config is modified or
     removed. See *RPC credentials* below.

5. **Verify, then attach the GUI** — steps A3 and A4 above. On a retrofit, step A3's
   `icacls` check will show your directory's original permissions rather than the
   hardened pair; that is expected per the note above.

6. **Keep using the monolithic wallet whenever you like.** Multiprocess is not a
   one-way door — the same data directory works either way. Just do not run both at
   once: launch the GUI *without* `-multiprocess` and you get a data-directory lock
   modal if the core is still running. Stop the core first with
   `Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Core-Stop'`.

#### RPC credentials

The daemon soft-sets `-server`, so a headless core always starts its RPC listener, and
it refuses to run with an empty `rpcpassword`. To keep that from turning into a core
that will not boot, credentials are generated automatically in two places:

* when the first-run config is created (fresh install), and
* when an RPC server is being started and the existing config has no credentials
  (retrofit, or a hand-written config that omits them).

The password is 32 random bytes, base58-encoded. It is not something you need to
remember or replace — it exists so the loopback RPC endpoint is authenticated. Only
missing keys are added; if you set `rpcuser` yourself, your name is kept and only the
password is appended.

If you genuinely do not want an RPC listener, set `server=0` in the config. That is
honoured and the config is left untouched — but understand what it costs, because on
Windows it is more than it looks:

* **Stake-only autounlock stops working.** The helper talks to the core purely over
  RPC.
* **Graceful shutdown stops working.** The `Core-Stop` task runs
  `gridcoinresearchd.exe stop`, which is an RPC call. Without RPC the only way to stop
  the core is a hard kill, with no database flush — which is exactly the thing the
  shutdown-flush Group Policy script exists to prevent.

A core with `server=0` can still be driven by attaching the GUI, but on an unattended
machine it is not a configuration we would recommend. The listener binds to loopback
only unless you add `rpcallowip`.

On a retrofit, the credentials land in a config file whose permissions we do not
change. If that file is readable by other accounts, tighten it yourself:

```powershell
icacls "$env:APPDATA\GridcoinResearch\gridcoinresearch.conf" /inheritance:r /grant:r "${env:USERNAME}:F" "SYSTEM:F"
```

On a fresh install this is already handled: the config is created inside a data
directory whose owner-only DACL it inherits (POSIX equivalently gets mode 0600,
applied before the password is written).

#### Reference

[`../contrib/windows/README.md`](../contrib/windows/README.md) documents every script,
its arguments, the firewall rule, the shutdown-flush Group Policy script, crash
recovery, and the autounlock design. [`running-unattended.md`](running-unattended.md)
is the task-oriented version of the same material for people who want the machine to
look after itself.

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

### Hardening the service

The snippet above is deliberately minimal. A fully hardened packaged unit ships at
[`contrib/init/gridcoinresearchd.service`](../contrib/init/gridcoinresearchd.service);
prefer it for real deployments. Beyond the usual `ProtectSystem=strict` /
`ProtectHome` / `PrivateTmp` / `MemoryDenyWriteExecute` sandboxing it adds a syscall
allow-list and an address-family restriction — the OS-maintained equivalent of an
in-process seccomp sandbox:

```ini
SystemCallFilter=@system-service
SystemCallErrorNumber=EPERM
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6 AF_NETLINK
CapabilityBoundingSet=
NoNewPrivileges=true
```

Multiprocess-specific gotchas when hardening the unit:

- **Keep `AF_NETLINK`** in `RestrictAddressFamilies=`. It is needed for
  `getaddrinfo()` / local interface enumeration; dropping it breaks DNS and
  local-address discovery, not just the obvious `AF_UNIX`/`AF_INET*`.
- **Do not use `DynamicUser=` or `PrivateUsers=`.** They remap the uid, and the
  GUI could then no longer open the `0600` `node.sock` / `ipc.cookie` the daemon
  creates in the datadir.
- **Do not use `PrivateNetwork=`** — it severs the P2P/RPC network.
- If the datadir lives under `/home` (a personal, non-packaged install), set
  `ProtectHome=read-only` **plus** `ReadWritePaths=<datadir>` instead of
  `ProtectHome=true`, or the daemon cannot write the datadir / create the socket.
  The packaged unit uses `/var/lib/gridcoinresearchd` (a `StateDirectory=`), so it
  keeps the stronger `ProtectHome=true`.

For supervisors other than systemd (or launches with none), the daemon also applies
a best-effort in-process hardening at startup: `-nonewprivs` (default on, Linux)
sets `NO_NEW_PRIVS` and drops the capability bounding set, so the node and its
children can never gain privileges via a setuid binary. It is a no-op on other
platforms and secondary to the systemd sandbox above; disable with `-nonewprivs=0`.

**Why no in-process seccomp syscall sandbox?** An internal seccomp-bpf filter was
considered and deliberately not added. Bitcoin Core shipped one, had to exclude the
GUI process from it (the Qt/display syscall surface is too large and volatile), and
then [removed it entirely](https://github.com/bitcoin/bitcoin/pull/27896) as not
worth maintaining versus externally-maintained sandboxing. A hand-rolled in-tree BPF
policy is brittle — every glibc/kernel/library update can introduce a syscall the
filter has not allow-listed, turning a benign call into a `SIGSYS` crash. The
declarative `systemd` `SystemCallFilter=` above gets the same post-exploitation
confinement (no `execve` of a shell, no new socket families for exfiltration, no
`ptrace` into sibling processes) with the policy maintained by the OS instead of by
this project.

### Unattended stake-only autounlock (Linux)

An encrypted wallet must be unlocked after every start before it can stake, and
there is no native way to supply a passphrase at daemon startup. A packaged install
ships an **opt-in** companion unit,
[`contrib/init/gridcoinresearchd-autounlock.service`](../contrib/init/gridcoinresearchd-autounlock.service),
that keeps the wallet unlocked **for staking only**, with the passphrase held
host/TPM-bound in `systemd-creds` — never in a file the daemon reads, the unit, or
on a command line. It stays **external** to the wallet (the in-wallet auto-unlock was
removed years ago as insecure); it uses only the standard
`walletpassphrase <pass> <timeout> true` RPC via the
[`gridcoin_autounlock.py`](../contrib/wallettools/gridcoin_autounlock.py) helper.

The helper is Python 3 (standard library only). The daemon package *Recommends*
`python3` rather than depending on it, so on a minimal install
(`apt --no-install-recommends`) install it first: `sudo apt install python3`.

Set it up once, then enable:

```bash
# Encrypt the wallet passphrase, bound to this host (and its TPM, if present):
printf '%s' 'YOUR-WALLET-PASSPHRASE' | sudo systemd-creds encrypt \
    --name=wallet-passphrase - /etc/gridcoin/wallet-passphrase.cred
sudo chown gridcoin:gridcoin /etc/gridcoin/wallet-passphrase.cred
sudo chmod 0400 /etc/gridcoin/wallet-passphrase.cred

sudo systemctl enable --now gridcoinresearchd-autounlock.service
```

`systemctl enable` links the unit into `gridcoinresearchd.service.wants/`, so it runs
whenever the core starts (boot, restart, deploy) without modifying the core unit;
`BindsTo=` re-triggers it on every core restart. It runs as `gridcoin`, unlocks
stake-only (`walletpassphrase … true`, which still permits staking **and** automatic
beacon renewal but blocks spends), and exits — a `Type=oneshot`, not a resident poll.
A rejected passphrase, bad RPC credentials, or an unverifiable RPC listener park the
unit in `failed` (`RestartPreventExitStatus=2`) where they are visible, rather than
looping silently over a locked wallet.

**Security model.** Only a principal that can already run as the `gridcoin` user on
this host can recover the passphrase — the same bar as reading the wallet's own files.
Before sending anything, the helper verifies (via `/proc/net/tcp{,6}`) that the RPC
listener is owned by its own uid, so credentials are never handed to a local process
that squatted the RPC port during startup; because the core uses `-daemonwait`, the
port is already bound and served by the time the unit is ordered `After=` it, so this
gate is defence in depth. While unlocked, the passphrase lives in the core's memory
(unavoidable for unattended staking). Rotating the passphrase (or `rpcpassword`)
requires `systemctl restart gridcoinresearchd-autounlock.service`, as the credential
is read at unit start.

### Unattended stake-only autounlock (Windows)

The Windows equivalent is a pair of PowerShell scripts (no Python needed) that the installer
bundles into `…\GridcoinResearch\windows\` (source: [`contrib/windows/`](../contrib/windows/)).
Run **as the wallet user, from an elevated console** (DPAPI needs an interactive logon — it fails
over a remote/SSH session):

```powershell
.\Set-GridcoinAutounlock.ps1 -DataDir "$env:APPDATA\GridcoinResearch"
```

It prompts for the wallet passphrase (twice) and the account's login password, encrypts the
passphrase with **DPAPI (CurrentUser)** to an owner-only `<datadir>\autounlock\passphrase.cred`
(decryptable only by that account on that machine), and registers a resident
`\Gridcoin\Autounlock` task that runs `Invoke-GridcoinAutounlock.ps1`. The helper waits for the
core, unlocks **stake-only**, and self-heals across restarts. Its security contract mirrors the
Linux helper: a **native listener-ownership gate** (`Get-NetTCPConnection` → owning-PID → owner
SID) refuses to send credentials unless our own account owns every LISTEN socket on the RPC
port, closing the same startup-squat race. Because DPAPI CurrentUser is per-account, the task
must run as the account that ran setup. Remove with `.\Set-GridcoinAutounlock.ps1 -Remove`. See
[`contrib/windows/README.md`](../contrib/windows/README.md).

## Troubleshooting

- **"Could not connect to the Gridcoin daemon … node.sock: connection refused."**
  The daemon is not running yet, was not started with `-multiprocess`, is using a
  different `-datadir`, or (Windows) is running as a different user. Start the daemon
  first and confirm `IPC: serving …` appears in its `debug.log`.
- **Version/schema mismatch on connect.** The GUI and daemon are different builds.
  Use matching `gridcoinresearch` / `gridcoinresearchd` binaries. If only the commit
  differs (not the schema), the GUI still connects and shows a dismissible mixed-build
  warning banner (also logged); suppress it per instance with `-nobuildwarn`.
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
