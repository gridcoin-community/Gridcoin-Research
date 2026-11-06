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
`node.sock`, in its IPC directory (`-ipcdir`, by default the data directory) and serves
the GUI over IPC (Cap'n Proto). The **GUI**, also started with `-multiprocess`, connects
to that socket instead of running the node in-process.

- **Connect-only.** The GUI does **not** start the daemon. Start the daemon first,
  then the GUI.
- **Same data directory.** Both processes must use the same `-datadir` (or, for the
  system service, the same `-ipcdir`); that is how the GUI finds `node.sock`.
- **Who may connect.** On Linux and the other POSIX systems the boundary is the
  operating system's peer check on the socket: the core admits a process of its own
  account, and with `-ipcattachgroup` also a member of that group (the system
  service); the GUI admits a core of its own account, or of the `-ipcserviceuser`
  account. The socket and the cookie are owner-only, or open to the attach group for
  the system service. The cookie, `ipc.cookie`, is a fresh random value for each run
  of the core: every process the peer check admits may read it, so it ties a connection
  to this run of the core rather than adding a second check of who connects. On Windows
  the owner-only permissions of the socket and the cookie are the guard.
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

A core started by hand like this has no stake-only autounlock. To start the core at
boot, and for autounlock, run it under the systemd units instead; see
[*Running the core as a service (Linux)*](#running-the-core-as-a-service-linux) below.

**Retrofitting an existing wallet** needs no migration: point the daemon at the data
directory you already use. Two things differ from a fresh install, and both are
covered in detail under [*RPC credentials*](#rpc-credentials) and
[*B. Retrofit*](#b-retrofit-an-existing-wallet-windows) below — the guidance is
platform-independent even though the procedure there is written for Windows:

* A data directory that already exists keeps its current permissions. We harden only
  what we create, so that a directory someone widened on purpose is not silently
  re-tightened. `node.sock` and `ipc.cookie` are owner-only either way. A file the core
  rewrites (the settings file, the configuration file, `peers.dat`, `banlist.dat`, a log after
  rotation, `wallet.dat` after encryption) keeps the permissions it had, and the core warns
  where they are wider than it would make them. The two exceptions are `ipc.cookie` and the
  autounlock file, which the core always writes with the narrowest permissions that work.
* A config with no `rpcuser`/`rpcpassword` gets generated ones appended, because the
  daemon starts an RPC listener and refuses to run without them. Older configs contain
  only `addnode=` lines, since the monolithic GUI never needed credentials.

Stop the monolithic wallet before starting the daemon on the same data directory —
they do not share it, and the second one to start gets a lock error.

#### The Flatpak

The Flatpak (`us.gridcoin.GridcoinResearch`) ships `gridcoinresearchd` alongside the
GUI. Inside the Flatpak's sandbox it has two uses:

* **The RPC command-line client**, for a core that also runs in the Flatpak:

  ```bash
  flatpak run --command=gridcoinresearchd us.gridcoin.GridcoinResearch getblockcount
  ```

* **The core of a multiprocess setup started by hand.** Start the daemon in a
  terminal and leave it running, in the foreground rather than with `-daemon`. Then
  start the GUI with `-multiprocess` from another terminal:

  ```bash
  flatpak run --command=gridcoinresearchd us.gridcoin.GridcoinResearch -multiprocess
  flatpak run us.gridcoin.GridcoinResearch -multiprocess
  ```

  Both use the Flatpak's own data directory, so neither needs `-datadir`.

The sandbox reaches neither a core run outside it nor the system service in `/run`.
The Flatpak therefore installs no systemd units and no gridcoin-service, and its
launcher offers no "Connect to ..." actions.

### Windows

Windows has the two ways of running the core in the background that Linux has, with the same meaning:

* **Same account.** The core runs as you, started at boot by the scheduled task `\Gridcoin\Core-Start`. This is
  for an administrator's own wallet, because only an administrator can register a task that starts at boot. The
  task uses the S4U logon, so Windows stores no password for it.
* **Service mode.** The core runs as the Windows service `gridcoinresearchd`, under its own account
  `NT SERVICE\gridcoinresearchd`. It suits any wallet holder: an administrator sets it up with
  `gridcoin-service.exe`, and the wallet's owner attaches the GUI as a member of `Gridcoin GUI Users`.

The plain GUI is unchanged. What each mode concedes, and the promise autounlock makes, are under
[`running-unattended.md` → *What autounlock promises*](running-unattended.md#what-autounlock-promises); choosing a
mode, the service mode and the runbooks are under [`running-unattended.md` → *Windows*](running-unattended.md#windows).
On Windows, any program running as you can obtain your wallet passphrase whenever same-account autounlock is set up; for protection against that, use the service mode.

In the same-account mode the IPC socket (`node.sock`) and the cookie (`ipc.cookie`) are created in your data
directory with owner-only NTFS permissions, so a GUI running as another account cannot open them. A core running as
`LocalSystem` would lock your own GUI out: **never run the same-account core as SYSTEM.**

The installer lays down PowerShell scripts in `…\GridcoinResearch\windows\` that register the task correctly, and
those are the supported route. Pick the procedure that matches your situation:

* **[A. Fresh install](#a-fresh-install-windows)**: no Gridcoin on this machine, or no existing data directory.
* **[B. Retrofit](#b-retrofit-an-existing-wallet-windows)**: you already run the ordinary (monolithic) Gridcoin
  wallet and want to move that same wallet to multiprocess.

Both end in the same place: a headless core started by Task Scheduler, and a GUI you attach and detach at will.
Registering the task needs an elevated PowerShell (Run as administrator); attaching the GUI needs a normal one.
Set the execution policy for the session first, since a fresh Windows install blocks `.ps1` files:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
cd "C:\Program Files\GridcoinResearch\windows"
```

> Set the policy in the session and call the scripts directly, as above. Do **not** wrap a call in
> `powershell.exe -ExecutionPolicy Bypass .\script -Arg "C:\Program Files\..."`: the outer shell re-parses the
> command line and splits the spaced path.

#### A. Fresh install (Windows)

1. **Run the installer** as an administrator. It places `gridcoinresearch.exe` (GUI),
   `daemon\gridcoinresearchd.exe`, `daemon\gridcoin-service.exe` and the `windows\` scripts.

   The installer does **not** create the data directory, and that is deliberate: the core creates it on its first
   start, owner-only. A directory created by anything else would inherit ordinary permissions instead.

2. **Add the Defender exclusions before the first sync.** Two entries, covering two different costs:

   ```powershell
   Add-MpPreference -ExclusionPath "$env:APPDATA\GridcoinResearch\blk*.dat"
   Add-MpPreference -ExclusionPath "$env:APPDATA\GridcoinResearch\txleveldb"
   ```

   **`blk*.dat`, the initial sync.** Defender re-scans the block files as they are written. On a 2-core VM syncing
   blocks 1-100,000 from a LAN peer this measured **605s without the exclusion and 172s with it**, with `MsMpEng`
   falling from 84% CPU to 6%. The wildcard is required: block files roll over at ~2GB, so a literal `blk0001.dat`
   stops covering the file being written after the first rollover.

   **`txleveldb`, every start after it.** The block index is LevelDB: many small random reads across many `.ldb`
   files, the access pattern a realtime scanner punishes most, because the cost tracks read count rather than
   bytes. On a Windows 11 VM with a mainnet wallet of 86,496 transactions, `blk*.dat` already excluded and the page
   cache warm, the time from daemon start to "Done loading" fell from **107.7s to 81.2s**. The IPC socket is not
   created until the node finishes loading, so until then a GUI has nothing to attach to.

   `wallet.dat` and `database/` are still fully scanned. Numbers in
   [`../contrib/windows/README.md`](../contrib/windows/README.md#windows-defender-exclusion-do-this-before-syncing).

3. **Register and start the core task**, elevated, as the wallet's own account:

   ```powershell
   .\Install-GridcoinCoreTask.ps1
   Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Core-Start'
   ```

   This registers **Core-Start** (at boot, through a retry launcher) and **Core-Stop** (on demand), for your own
   account, with no password asked for or stored. For a standard user's wallet the script refuses and says so: an
   administrator sets up the service mode instead
   ([`running-unattended.md` → *Service mode*](running-unattended.md#service-mode-the-windows-service)).

   Do **not** pass `-DataDir` for a default install. The daemon resolves the Windows default itself
   (`%APPDATA%\GridcoinResearch`) and creates it owner-only; an explicit `-datadir` takes the branch that
   *refuses* to create a missing directory. Pass `-DataDir` only for a custom location that exists already.

4. **Confirm the first start did what it should:**

   ```powershell
   $dd = "$env:APPDATA\GridcoinResearch"
   Get-Process gridcoinresearchd            # the core is up and stayed up
   icacls $dd                               # exactly your account + SYSTEM
   Select-String -Path "$dd\gridcoinresearch.conf" -Pattern '^rpcuser|^rpcpassword'
   ```

   Expect the data directory to list **only your account and `NT AUTHORITY\SYSTEM`**: the protected DACL drops the
   inherited entries. Expect the config to contain a `rpcuser` and a 43-44 character random `rpcpassword`,
   generated on that first run (see *RPC credentials* below). An elevated start runs as two processes: the core
   gives up the administrator rights by starting itself again without them, and `debug.log` says
   `Started without administrator rights: the process that started Gridcoin had them.`

   > The first multiprocess-capable start does a one-time `wallet.dat` rewrite and may exit. If
   > `Get-Process gridcoinresearchd` shows nothing immediately after, run the `Start-ScheduledTask … Core-Start`
   > line once more.

5. **Attach the GUI** from a normal (not elevated) PowerShell:

   ```powershell
   .\Start-GridcoinGui.ps1
   #   or: .\Start-GridcoinGui.ps1 -CreateShortcut   (Start menu entry)
   ```

   The script refuses an elevated console, so keep UAC on: with UAC off every console of an administrator is
   elevated. The GUI connects to the running core. It will not start one for you: if nothing is listening it says
   so and stops.

6. **For an unattended machine:** a Group Policy shutdown script for a clean database flush, and stake-only
   autounlock. Both are covered in [`running-unattended.md`](running-unattended.md#same-account-the-core-task).

#### B. Retrofit an existing wallet (Windows)

Your existing data directory, wallet, and blockchain are reused **in place**. There is no export/import step and no
resync. The differences from a fresh install are the two things that already exist on disk: the data directory and
the config file.

1. **Stop the wallet completely.** Exit the GUI and confirm nothing is left running:

   ```powershell
   Get-Process gridcoinresearch, gridcoinresearchd -ErrorAction SilentlyContinue
   ```

   Both the core and the GUI hold the data directory; starting the core while the old monolithic wallet is up gives
   you a data-directory lock error, not a merge.

2. **Back up `wallet.dat`** before changing how the wallet is launched.

3. **Install the new build and register the task**: steps A1 to A3 above, unchanged. An installer upgrade from the
   earlier task scripts replaces their tasks with the S4U ones and converts their autounlock file; see
   [`running-unattended.md`](running-unattended.md#same-account-the-core-task).

4. **Expect these two differences from a fresh install:**

   * **The data directory keeps the permissions it already has.** Hardening is applied only when *we* create the
     directory, so anyone who widened it on purpose does not have that silently reverted. A file the core rewrites
     keeps its permissions too. `node.sock`, `ipc.cookie` and the autounlock file are owner-only regardless, since
     those are created fresh with the narrowest permissions that work. The installer lists files with wider
     permissions in `UPGRADE-NOTICE.txt`, and the core warns at each start (`WARN: … the data directory … is not restricted to this
     account` in `debug.log`). If you want the directory itself tightened, do it yourself:

     ```powershell
     icacls "$env:APPDATA\GridcoinResearch" /inheritance:r /grant:r "${env:USERNAME}:(OI)(CI)F" "SYSTEM:(OI)(CI)F"
     ```

   * **RPC credentials are added to your existing config if it has none.** A config written by an older build
     contains only `addnode=` lines, since the monolithic GUI never needed RPC credentials. The daemon does need
     them, and rather than refusing to start it appends a generated `rpcuser` and random `rpcpassword`, then
     continues. Nothing already in your config is modified or removed. See *RPC credentials* below.

5. **Verify, then attach the GUI**: steps A4 and A5 above. On a retrofit, the `icacls` check shows your
   directory's original permissions rather than the hardened pair, as the note above says.

6. **Keep using the monolithic wallet whenever you like.** The same data directory works either way. Just do not
   run both at once: launch the GUI *without* `-multiprocess` while the core is still running and you get a
   data-directory lock message. Stop the core first with
   `Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Core-Stop'`.

#### RPC credentials

The daemon soft-sets `-server`, so a headless core always starts its RPC listener, and it refuses to run with an
empty `rpcpassword`. To keep that from turning into a core that will not boot, credentials are generated
automatically:

* when the first-run config is created (fresh install, and `gridcoin-service.exe install` for the service), and
* when an RPC server is being started and the existing config has no credentials (retrofit, or a hand-written
  config that omits them).

The password is 32 random bytes, base58-encoded. It exists so the loopback RPC endpoint is authenticated; you do not
need to remember it. Only missing keys are added; if you set `rpcuser` yourself, your name is kept and only the
password is appended.

If you do not want an RPC listener, set `server=0` in the config. That is honoured and the config is left
untouched, but on Windows it turns off the graceful stop: the `Core-Stop` task runs `gridcoinresearchd.exe stop`,
which is an RPC call, and without RPC the only way to stop the core is a hard kill with no database flush. (Autounlock
does not use RPC: the core reads its own autounlock file.) The listener binds to loopback only unless you add
`rpcallowip`.

On a retrofit, the credentials land in a config file whose permissions we do not change. If that file is readable
by other accounts, tighten it yourself:

```powershell
icacls "$env:APPDATA\GridcoinResearch\gridcoinresearch.conf" /inheritance:r /grant:r "${env:USERNAME}:F" "SYSTEM:F"
```

On a fresh install this is already handled: the config is created inside a data directory whose owner-only DACL it
inherits. When the core later rewrites the file to drop an obsolete key, the file keeps the permissions it had.

#### Reference

[`../contrib/windows/README.md`](../contrib/windows/README.md) documents every script, its arguments, the firewall
rule, the shutdown-flush Group Policy script, crash recovery, and autounlock.
[`running-unattended.md`](running-unattended.md#windows) is the task-oriented version, with the service mode and the
runbooks, for people who want the machine to look after itself.

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
- send the daemon `SIGTERM` (Linux), stop its unit with `systemctl stop` (Linux
  service), start the `Core-Stop` task (Windows, same account; ending the
  `Core-Start` task instead is a hard kill with no flush), or `Stop-Service
  gridcoinresearchd` (the Windows service; a stop over RPC or from the GUI is
  restarted).

If the GUI is connected when the daemon stops, it detects the lost connection and
exits cleanly on its own.

### Running the core as a service (Linux)

The packages, and `cmake --install` on Linux, install systemd units for two ways of
running the core unattended. [`running-unattended.md`](running-unattended.md) describes
both, how to set them up, what each protects and what it does not:

- **Same account** (`gridcoinresearchd@<user>.service`, set up with `sudo gridcoin-service
  enable-user <user>`): the core runs as you, with your data directory, as if you had
  started it by hand, and keeps running when you log out. This is for convenience,
  staking and autounlock; for higher security use the next mode.
- **Different account** (`gridcoinresearchd.service`, a wallet moved in with `sudo
  gridcoin-service enable <user>` from another account's login): the core runs as the
  `gridcoin` account in `/var/lib/gridcoinresearchd`, and members of `gridcoin-gui`
  attach the GUI with `-ipcdir=/run/gridcoinresearchd -ipcserviceuser=gridcoin`.

Both units are `Type=notify`: the core starts with `-nodaemon -nodaemonwait` and tells
systemd itself when it is ready, so `daemon=1` in a configuration file does not apply to
them. Stake-only autounlock is a socket-activated helper (`gridcoin-autounlock@<user>.socket`,
or `gridcoin-autounlock.socket` for the system service) that serves the passphrase only to
the main process of its core's unit; a core started by hand gets nothing from it. The helper runs
under the system account `gridcoin-autounlock` (testnet `gridcointestnet-autounlock`), which no desktop
process uses; the `sysusers.d` entry creates it.

The system service runs in a systemd sandbox (`ProtectSystem=strict`, `ProtectHome=yes`,
`PrivateTmp=`, `PrivateDevices=`, `SystemCallFilter=@system-service`,
`RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6 AF_NETLINK`, an empty capability
bounding set and the rest of the list in the unit). The sandbox is best effort, and two
points of it are easy to misread:

- **`@system-service` allows `execve`.** The core runs its notify commands
  (`blocknotify=` and the others) through `/bin/sh`, so the filter does not stop a
  compromised core from running programs as the service account. What it removes are
  system calls a node never needs: `ptrace` and the other debugging calls, mounting,
  loading kernel modules, raw I/O, swap and reboot. The address-family list keeps the
  core to Unix, IPv4, IPv6 and netlink sockets (netlink is for `getaddrinfo()` and
  interface enumeration; dropping it breaks DNS).
- **No `DynamicUser=`, `PrivateUsers=` or `PrivateNetwork=`.** A uid remap would stop the
  GUI opening the socket and the cookie, and a private network would cut the P2P and
  RPC network.

The per-user unit has no sandbox: it runs with your account's rights, as a core you
start by hand does.

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
declarative `systemd` `SystemCallFilter=` above does that job, with the policy
maintained by the OS instead of by this project.

#### Packagers

The units, the `sysusers.d` and `tmpfiles.d` entries, the autounlock helper and
`gridcoin-service` are generated from the templates in `contrib/init` by the daemon's
install step (`cmake/systemd_units.cmake`), for one network per build. The CMake
variables, described in [`cmake-options.md`](cmake-options.md#systemd-units-linux):

- `GRC_UNIT_NETWORK` (`main` or `test`): which network's names the units, accounts and
  tool get. A testnet build installs the daemon as `gridcointestnetd`.
- `INSTALL_SYSTEMD_UNITS` (`ON` on Linux): the units, the entries and `gridcoin-service`.
- `INSTALL_SYSTEMD_AUTOUNLOCK` (`ON`): the helper and its four units. They need systemd
  251 or later; build with `OFF` where it is older (Ubuntu 22.04), and `gridcoin-service`
  then refuses `autounlock enable`.
- `GRC_SYSTEMD_UNITDIR`, `GRC_SYSUSERSDIR`, `GRC_TMPFILESDIR`: where those go, relative to
  `CMAKE_INSTALL_PREFIX`.
- `GRC_UNIT_PYTHON3` (`/usr/bin/python3`): the interpreter the helper unit runs, as
  `python3 -I`.
- `GRC_DESKTOP_CORE_ACTIONS` (`ON`): the GUI launcher actions "Connect to your own core"
  and "Connect to the system service".

Ship the units, the helper and `gridcoin-service` from one build: they are made to work
together. Keep the daemon execute-only (mode 0711), as the install step makes it; that
mode makes the core non-dumpable from the start on hosts with Yama `ptrace_scope=0`. A
package's scripts run `systemd-sysusers` and `systemd-tmpfiles --create` for the installed
entries, create the system service's configuration file (`root:gridcoin`, mode 0640,
with generated RPC credentials), and on an upgrade from a package with the earlier units
run `gridcoin-service switch-over`; `debian/` does all of this and is the reference.

### Unattended stake-only autounlock (Windows)

The core unlocks itself; there is no helper process and no stored login password. Set autounlock up once in the
wallet (**Settings → Autounlock for staking...** in the GUI, or `setautounlock`), and a core started with
`-autounlockblob` (Core-Start and the Windows service start it so) reads the stored passphrase at each start and
unlocks the wallet for staking only. The passphrase is encrypted with DPAPI, with the machine's key in the
same-account mode and the service account's in the service mode, into `autounlock\passphrase.dpapi` in the data
directory, which only the core's account, SYSTEM and Administrators can read. A passphrase change re-encrypts it;
`clearautounlock` deletes it and locks the wallet. In the same-account mode every program running as you can
decrypt it; the service mode keeps it from desktop accounts. Setup, rotation and turning it off:
[`running-unattended.md` → *Autounlock setup (Windows)*](running-unattended.md#autounlock-setup-windows).

## The command-line client

`gridcoinresearchd <command>` sends one request to a running core and prints the reply. The
client identifies the program it connected to before it sends the request.

After it connects, and before it writes a byte, the client looks up the server side of its own
connection (both addresses and both ports) in the kernel's TCP table (`/proc/net/tcp` and
`/proc/net/tcp6` on Linux, `GetExtendedTcpTable` on Windows) and finds the account that owns the
listener. It sends the request only when that account is:

1. the account running the client;
2. for root (Windows: SYSTEM), the owner of the configuration file the client read, so that
   `sudo gridcoinresearchd -datadir=/home/alice/.GridcoinResearch stop` reaches alice's core; or
3. the account named by `rpcserviceuser=` in the configuration file. This is honoured only when
   root (Windows: SYSTEM or Administrators) owns both the configuration file and its directory,
   nobody else can write either, and no `includeconf` is in use. Set anywhere else (the command
   line, the settings file, a file you own) it is ignored with a warning.

On Windows the listener must also be the installed Gridcoin core: `gridcoinresearchd.exe` at the
client's own path, or `gridcoinresearch.exe` beside it (or one directory up from the installer's
`daemon\` directory).

Otherwise the client sends nothing and exits with an error such as:

```
error: refusing to send RPC credentials: the listener at 127.0.0.1:15715 is owned by 1002, not by 1000. Pass -rpcskiplistenercheck to send anyway (not recommended: what the client sends then goes to whatever program holds that address).
```

When the server side cannot be found or its owner cannot be read, the error is `refusing to send
RPC credentials: could not identify the owner of the listener at <address>.` A refusal is final:
`-rpcwait` does not retry it.

A connection found in this host's table is checked whatever its address. One that is not there
is taken to reach another machine, and the client sends, only when its address is none
of this host's: not loopback (127.0.0.0/8, `::1`), not `0.0.0.0` or `::`, and not an address of
one of this host's interfaces. Otherwise, or when this host's interface addresses cannot be read,
the client refuses. The check runs on Linux and Windows; on macOS and the BSDs the client has no
such check.

### Typing a passphrase

A passphrase given as a command-line argument is visible to other users of the machine
(`ps`, `/proc/<pid>/cmdline`) and stays in the shell history. The client reads it from standard
input instead, with echo off at a terminal, when told to:

```bash
# unlock for staking only, for 600 seconds
gridcoinresearchd -stdinwalletpassphrase walletpassphrase 600 true

# change the passphrase: the old one on the first line, the new one on the second
gridcoinresearchd -stdinwalletpassphrase walletpassphrasechange

# encrypt the wallet
gridcoinresearchd -stdinwalletpassphrase encryptwallet

# store the passphrase for staking-only autounlock (Windows), or turn that off
gridcoinresearchd -stdinwalletpassphrase setautounlock
gridcoinresearchd -stdinwalletpassphrase clearautounlock

# send one command with the passphrase for that request alone
gridcoinresearchd -stdinelevate sendtoaddress <address> <amount>
```

With `-stdinwalletpassphrase` the passphrases of `walletpassphrase`, `walletpassphrasechange`,
`encryptwallet`, `setautounlock` and `clearautounlock` are read one per line, in the order of the method's parameters, and the remaining
arguments (the timeout, the staking-only flag) stay on the command line. A terminal shows a prompt
on stderr (`Wallet passphrase> `) and, for a new passphrase (`encryptwallet`, and the second line of
`walletpassphrasechange`), asks twice and sends nothing if the two entries differ. From a pipe or a
file there are no prompts and no repeat: `printf '%s\n' "$PASSPHRASE" | gridcoinresearchd
-stdinwalletpassphrase walletpassphrase 600 true`. A line ends at the line feed; one carriage return
before it is dropped, and nothing else is trimmed. A passphrase given on the command line still
works and prints a warning on stderr.

With `-stdinelevate` the client reads one passphrase line and sends it with the request in the
`X-Gridcoin-Elevate` header (base64), so that one command may use the wallet's keys while the wallet
stays unlocked for staking only; `getwalletinfo` reports the wallet's `unlock_scope`. The option is
not used with the five passphrase methods above. The passphrase cannot be sent this way when it is
empty or when its encoded form is longer than 8000 bytes (a passphrase of more than 6000 bytes); the
client then says so and sends nothing, and `walletpassphrase` is the way to unlock.

### Administering the system service

Root, or an administrator, running the client against the system service reaches a listener owned
by the service account (`gridcoin`, or `gridcointestnet` for testnet; on Windows
`NT SERVICE\gridcoinresearchd`). Name that account in the service's configuration file; the
client honours the line while root owns that file and its directory and nobody else can write
either:

```
# /etc/gridcoin/gridcoin.conf
rpcserviceuser=gridcoin
```

```bash
sudo gridcoinresearchd -conf=/etc/gridcoin/gridcoin.conf -datadir=/var/lib/gridcoinresearchd getwalletinfo
```

Running the client as the service account works without that line, since the listener is then the
client's own: `sudo runuser -u gridcoin -- gridcoinresearchd -conf=/etc/gridcoin/gridcoin.conf
-datadir=/var/lib/gridcoinresearchd getwalletinfo`.

### A core in a Docker container

A client on the host talking to a core in a container through a published port connects to
`docker-proxy`, which runs as root, so a client run by any other account refuses. (Where Docker
forwards the port in the kernel instead, the host has no listener for the connection and the client
refuses as well.) Run the client inside the container
(`docker exec <container> gridcoinresearchd getblockcount`), or pass the override below.

### Skipping the check

`-rpcskiplistenercheck` sends without identifying the owner of the listener, and the client says
so on stderr. Not recommended: what the client sends, credentials included, then goes to whatever
program holds the RPC address.

### What protects the core's memory

Linux: the core marks itself non-dumpable as the first statement of `main`, so other programs of the
same user cannot attach a debugger, read `/proc/<pid>/mem`, `environ` or `fd`, or take its file
descriptors. The packaged service units set `LimitCORE=0`; for a core you start by hand run
`ulimit -c 0` first. On hosts with Yama `ptrace_scope=0` (Debian's default) the packages install the
daemon execute-only (0711), so it is non-dumpable from `execve`; a hand-started core from a readable
binary has a window of microseconds before that first statement. The core also refuses to start with a
tracer attached unless `-debugallowtracer` is given (see `doc/developer-notes.md`), and that option makes
the process dumpable again.

Passphrases move into `SecureString` at the earliest point, and the copies the RPC server, the CLI and
the GUI make are cleansed after use, but the HTTP path's stream buffers and JSON parser internals are
not. The protection therefore rests on non-dumpability and disabled core dumps, not on scrubbing.

Windows: a process running as you can read the memory of any other process running as you, the
monolithic wallet's included (see section 2 of `doc/multiprocess_design.md`). The Windows service's core
runs as its own account, whose memory a desktop account cannot read unless it is an administrator. macOS: the hardened
runtime (see the macOS section).

## Troubleshooting

- **The autounlock helper fails with `status=243/CREDENTIALS` and the journal shows
  `Failed to unseal secret using TPM2: Operation not permitted`.** The encrypted
  passphrase was sealed to a TPM PCR policy (the `systemd-creds` default binds
  PCR 7) and the machine's Secure Boot keys, dbx, or firmware have since changed,
  so the TPM refuses to unseal it. The credential cannot be recovered; encrypt the
  passphrase again with `--tpm2-pcrs=""` as in
  [`running-unattended.md` → *Autounlock setup*](running-unattended.md#autounlock-setup),
  then restart the core's unit. Unlock the wallet by hand in the meantime
  (`walletpassphrase` stake-only) — the core keeps running, it is just not staking.
- **Autounlock does not unlock at boot inside a systemd container** (the helper logs `credential unusable:
  missing`; a later restart of the core works). The container's root mount is private, so a unit started at boot
  does not see the credentials PID 1 mounts for it. Make `/` shared at boot, as a host's systemd does; see
  [`running-unattended.md`](running-unattended.md#troubleshooting).
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
