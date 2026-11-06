# Gridcoin multiprocess on Windows — core task scripts

PowerShell scripts to run the split **multiprocess** Gridcoin core as a background
Scheduled Task on Windows, stop it gracefully, upgrade it, and launch the GUI. See
[`doc/multiprocess.md`](../../doc/multiprocess.md) for the split-mode model, and
[`doc/running-unattended.md`](../../doc/running-unattended.md#windows) for the step-by-step
walkthrough.

Windows has two background modes, with the same meaning as on Linux:

- **Same account**: these scripts. The core runs as you, from the scheduled task
  `\Gridcoin\Core-Start`. It is for an administrator's own wallet: only an administrator can
  register a task that starts at boot.
- **Service mode**: `daemon\gridcoin-service.exe`. The core runs as the Windows service
  `gridcoinresearchd` under its own account, for any wallet holder; an administrator sets it up
  and the wallet's owner attaches the GUI as a member of `Gridcoin GUI Users`
  ([`doc/running-unattended.md` → *Service mode*](../../doc/running-unattended.md#service-mode-the-windows-service)).

The plain GUI is unchanged. What each mode concedes is listed under
[*What autounlock promises*](../../doc/running-unattended.md#what-autounlock-promises).
On Windows, any program running as you can obtain your wallet passphrase whenever same-account autounlock is set up; for protection against that, use the service mode.

The Windows installer **bundles these scripts** into `…\GridcoinResearch\windows\`; run
them from there, the setup scripts in an **elevated** PowerShell and `Start-GridcoinGui.ps1` in
a normal one. (In a source checkout they live here in `contrib/windows/`.)

| Script | Purpose |
|---|---|
| `Install-GridcoinCoreTask.ps1` | Register the **Core-Start** (boot) and **Core-Stop** (graceful) tasks for your own account, S4U, with no stored password. Needs an elevated console. |
| `Uninstall-GridcoinCoreTask.ps1` | Remove them (`-All` also removes the shutdown script and the old autounlock helper). |
| `Update-GridcoinCore.ps1` | Installs a release only after its signed `SHA256SUMS` verifies against the pinned key. |
| `Test-GridcoinRelease.ps1` | Verifies a signed `SHA256SUMS` (also runnable on its own). |
| `release-keys.txt` | The pinned release signing key(s); ships with none. |
| `Set-GridcoinShutdownFlush.ps1` | **Opt-in**: configure/remove a Group Policy shutdown script for a reliable DB flush on OS shutdown. |
| `Start-GridcoinGui.ps1` | Launch the `-multiprocess` GUI from a normal (not elevated) console, for the task or, with `-Service`, for the Windows service (or drop a Start-Menu shortcut with `-CreateShortcut`). |
| `GridcoinCommon.ps1` | Functions the scripts share (dot-sourced; not run on its own). |
| `Invoke-GridcoinInstallStep.ps1` | The installer's steps: stop and restart the cores, switch over from the earlier tasks, harden a custom install directory, report wider permissions, the checks before uninstall. The installer runs it; it is not for use by hand. |
| `..\daemon\gridcoin-service.exe` | The service mode: `install`, `enable --user=<account>`, `disable`, `status`, `uninstall` (elevated; `help` lists the options). |

Autounlock is **opt-in** and is set up in the wallet itself (see [Autounlock](#autounlock-opt-in-stake-only-dpapi) below); there is no separate helper to run.

## Quick start

```powershell
# In an ELEVATED PowerShell. A fresh Windows install blocks .ps1 by default, so set the
# policy for this session and call the scripts directly -- do NOT wrap a call as
# `powershell.exe -ExecutionPolicy Bypass .\script -Arg "C:\Program Files\..."`, because
# the outer shell re-parses and splits the spaced path.
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
cd "C:\Program Files\GridcoinResearch\windows"

# Registered for the account you are running as (S4U: no password is asked for or stored). That
# account must be an administrator. For a standard-user wallet, an administrator sets up the
# service mode instead: gridcoin-service.exe install, then enable.
# Do NOT pass -DataDir for a default install: the daemon resolves the Windows default itself
# (CSIDL_APPDATA\GridcoinResearch) and creates it owner-only on first run, whereas an explicit
# -datadir takes the branch that refuses to create a missing directory. Pass -DataDir only for a
# genuinely custom location (which the wallet's own chooser will have created already).
.\Install-GridcoinCoreTask.ps1

Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Core-Start'   # start the core now
```

Then, from a normal (not elevated) console, attach the GUI:

```powershell
.\Start-GridcoinGui.ps1                       # task mode
.\Start-GridcoinGui.ps1 -Service -Network test  # the Windows service for testnet (gridcoin-service.exe enable prints this line)
```

The core then autostarts on every boot. To upgrade, from an elevated console, with the installer, `SHA256SUMS` and `SHA256SUMS.sig` of the release:

```powershell
.\Update-GridcoinCore.ps1 -Installer .\gridcoin-5.5.2.1-win64-setup.exe -Sums .\SHA256SUMS -Signature .\SHA256SUMS.sig
```

## Things to know

**Run as the wallet user, never SYSTEM, with no stored password.** The tasks are registered with the S4U logon, so Windows keeps no password for them; a password change on the account needs nothing re-registered. The IPC socket (`node.sock`) and cookie
(`ipc.cookie`) are created **owner-only** in the datadir (NTFS ACL), so on Windows
only that user can open them. (The Linux `SO_PEERCRED`/peer-UID check has no Windows
equivalent — AF_UNIX on Windows exposes no peer credentials — so the ACL is the
enforcement here.) A SYSTEM-owned core would lock the wallet user's own GUI out.
This also means **only that Windows user can attach a GUI** — a *different* user
running `gridcoinresearch.exe -multiprocess` gets *"could not connect to the
daemon"*. **That is the access control working, not a bug.**

**Graceful stop = start the Core-Stop task.** To stop the core, *start* the
`Core-Stop` task (`Start-ScheduledTask … Core-Stop`) — it runs `gridcoinresearchd
… stop`, an RPC stop that flushes the databases. **Never `Stop-ScheduledTask` the
`Core-Start` task**: Task Scheduler's stop is a hard `TerminateProcess` with no
flush, which risks database corruption. Shutdown, upgrade, and manual stop all go
through the one `Core-Stop` task.

**RPC credentials are required, and generated for you.** The daemon soft-sets
`-server`, so the core always starts its RPC listener (loopback only unless you add
`rpcallowip`) and refuses to run with an empty `rpcpassword`. On a fresh install the
credentials are written into the first-run config; on a **retrofit**, where the config
already exists but predates this and holds only `addnode=` lines, a generated
`rpcuser`/`rpcpassword` is appended to it and nothing else is touched. You do not need
to do anything, and there is nothing to memorise.

`Core-Stop` runs `gridcoinresearchd.exe stop`, which is also RPC, so `server=0` is a poor idea
here even though it is honoured: with RPC off the only remaining way to stop the core is a hard
kill with no database flush.

**The limits of an S4U task.** It has no network credentials and no access to your own DPAPI
keys. A data directory or backup directory on a network share is out of its reach, and so is an
AppData folder redirected to a network share: use a local data directory (`-DataDir`) or the
service mode. Some accounts cannot get an S4U logon (some Microsoft Entra-only accounts); for
them, use the service mode.

**Started elevated, the core and the GUI give up the administrator rights first.** They start
themselves again without them, so the files they create are yours, not Administrators'.
`debug.log` says `Started without administrator rights: the process that started Gridcoin had
them.`

**Keep UAC on.** `Start-GridcoinGui.ps1` refuses to run elevated, and with UAC off every console
an administrator opens is elevated.

**A file the core rewrites keeps the permissions it had**, and the core warns at start where
they are wider than it would make them. A file it creates new is owner-only; the IPC cookie and
the autounlock file are always written with the narrowest permissions that work. On an upgrade
the installer lists files of the default data directory with wider permissions in
`windows\UPGRADE-NOTICE.txt` (the finish page offers to open it) and changes nothing;
`gridcoinresearchd.exe -reportpermissions` runs the same check at any time.

## Windows Defender exclusion (do this before syncing)

**Exclude the block data and the block index from Defender before a first sync.** Two
entries, and note the wildcard on the first -- it matters:

```powershell
# elevated PowerShell
Add-MpPreference -ExclusionPath "$env:APPDATA\GridcoinResearch\blk*.dat"
Add-MpPreference -ExclusionPath "$env:APPDATA\GridcoinResearch\txleveldb"
```

The first covers the initial sync (numbers below). The second covers every startup
after it: the block index is LevelDB, whose many small random reads are the pattern a
realtime scanner punishes hardest, since the cost tracks read count rather than bytes.

Measured on a Windows 11 VM against a mainnet wallet of 86,496 transactions and a
4,055,586-entry block index, with `blk*.dat` already excluded in both cases and the
page cache warm in both, so the only variable is the `txleveldb` exclusion:

| phase | not excluded | excluded |
|---|---:|---:|
| load block chain (control -- excluded either way) | 47.1s | 42.8s |
| wallet spent-flag reconciliation (`ReadTxIndex` per candidate) | **37.5s** | **20.6s** |
| **daemon start to "Done loading"** | **107.7s** | **81.2s** |

The control barely moves, which is what makes the comparison readable. The same wallet
and build on Linux reaches "Done loading" in 25.1s, so this narrows the gap rather than
closing it -- the rest is ordinary Windows and VM I/O cost.

Measured on a 2-core Windows 11 VM syncing mainnet blocks 1..100,000 from a peer on
the same LAN, so neither WAN latency nor peer speed is involved:

| exclusion | 1 -> 100k | MsMpEng CPU | wallet CPU |
|---|---|---|---|
| none | 605s | 84% | 21% |
| `txleveldb` only | ~730s | 84% | 21% |
| `blk*.dat` | **172s** | **6%** | **71%** |
| whole data directory | 170s | - | - |

`blk*.dat` alone recovers all of it. The same run on Linux from the same peer takes
125-135s, so with the exclusion Windows lands at ~1.3x Linux -- the expected
filesystem difference -- against ~4.5x without it.

**Why the block files and not LevelDB.** The obvious suspect is `txleveldb`, which
creates, renames and deletes many small files. Excluding it changes nothing.
`WriteBlockToDisk` opens `blk*.dat` with `AppendBlockFile`, appends, and lets
`CAutoFile` close it -- once per block. Defender scans on close-after-modify, and
the file it re-examines grows into the gigabytes, so the cost rises with chain
height. That also explains why a long sync feels progressively worse.

**The wildcard is required, not tidiness.** `AppendBlockFile` rolls to a new file at
~2GB (`nCurrentBlockFile++`), so a mainnet node has `blk0001.dat`, `blk0002.dat`,
`blk0003.dat`, ... Excluding a literal `blk0001.dat` works until the first rollover
and then silently stops covering the file actually being written.

**Recognising it.** Low wallet CPU alongside high `MsMpEng` CPU in Task Manager is
the signature; the wallet looks slow but is simply waiting. Freshly built, unsigned
binaries attract the most aggressive scanning, so a developer build feels worse than
a signed release.

This excludes block data only -- no executables, and `wallet.dat`, `database/` and
`txleveldb` all remain fully scanned.

**The exclusion is required, not a stopgap.** `WriteBlockToDisk` used to reopen and
close `blk*.dat` for every block, and the obvious theory was that Defender scans on
close-after-modify, so caching the handle across blocks would remove the trigger.
The handle is now cached -- and it does not help: measured with the cache in place
and no exclusion, the same range took 674s with `MsMpEng` still at 70% CPU. Defender
scans the **writes**, through on-access protection, which no amount of holding the
file open can avoid. Do not drop this exclusion on the assumption that a code change
has made it unnecessary.

## Firewall (inbound P2P)

`Install-GridcoinCoreTask.ps1` adds an **inbound firewall rule for the daemon**
(`gridcoinresearchd.exe`). The headless core never triggers the interactive firewall
prompt the GUI gets on first run, and in multiprocess mode the **core**, not the GUI,
does the P2P — so without a rule Windows silently blocks inbound and the node is
outbound-only (it still syncs and stakes, just can't accept incoming peers). The rule is
program-based on the **Domain** and **Private** profiles (Public is excluded; add it with
`-FirewallProfile Domain,Private,Public`). Because it is program-based (any TCP to the
daemon), if you also set `rpcallowip` — which makes the RPC listener bind to all
interfaces instead of localhost — the RPC port becomes reachable through this rule too,
with only the RPC password between it and the network. Leave `rpcallowip` unset unless
you need it. Pass `-SkipFirewallRule` to manage the firewall yourself;
`Uninstall-GridcoinCoreTask.ps1 -All` removes the rule. `gridcoin-service.exe install`
adds the same kind of rule for the service.

## Clean database flush on OS shutdown

`Core-Stop` carries **no** OS-shutdown trigger. A scheduled task can't make Windows wait,
so an event-triggered (EventID-13) stop is best-effort and gets cut short mid-flush —
confirmed on real hardware, where reboots produced no logged graceful shutdown (the core
was effectively hard-killed, and only recovered clean thanks to LevelDB's crash-resilience).
BDB (the wallet) is less forgiving of a mid-write kill, so for a **guaranteed** flush use a
Group Policy **Computer → Shutdown** script, which Windows runs **synchronously and waits
for** (bounded by *Maximum wait time for Group Policy scripts*, default 600 s).

Configure it (opt-in, elevated):

```powershell
.\Set-GridcoinShutdownFlush.ps1   # -Remove to undo; -WaitSeconds N caps the wait (default 300)
```

It writes two small files into the machine shutdown script folder: `GridcoinShutdownFlush.cmd`
(one line that starts Windows PowerShell) and `GridcoinShutdownFlush.ps1`. At shutdown the `.ps1`
runs **as SYSTEM**, reads the process ID of the running **Core-Start** instance (the launcher, which
exits only after the core), starts the **Core-Stop** task and waits for that process to exit. SYSTEM
reads no file of the wallet user and runs no Gridcoin program: the task stops the core, as the
wallet user. The script then wires the `.cmd` into the local machine shutdown scripts and the
run-time state. **Validate by rebooting once** and confirming a full `Shutdown: … Final flush of
wallet database …` sequence in `debug.log` just before the restart. (We deliberately don't *also*
put a shutdown trigger on `Core-Stop` — the two would double-stop and race.)

**Domains.** Local and domain-GPO shutdown scripts **stack** — all run at shutdown,
additively, bounded collectively by the max-wait-time. But if a domain GPO enables **"Turn
off Local Group Policy Objects processing,"** this *local* script is ignored; in that case
deploy the same two files via a **domain GPO** instead.

## Crash recovery

`Core-Start` runs the core through a generated launcher (`core-autostart.cmd`) that
retries **once, after a 2-minute wait**, if the daemon exits non-zero — the analogue of
the Linux unit's `Restart=on-failure`. A *graceful* stop exits 0 and is not retried;
only a non-zero exit is. Each attempt's stderr is captured to
`%TEMP%\gridcoin-core-start.err.log` (deliberately NOT inside the data directory: cmd
evaluates the redirect before running the daemon, so writing there would force the
launcher to pre-create the data directory, and the daemon only applies its owner-only
DACL to a directory it creates itself).

The retry deliberately lives in the launcher rather than in the task: Task Scheduler's
own restart-on-failure does **not** fire on a program's non-zero exit (confirmed
on-device), so the `-RestartCount` it used to be registered with never actually helped.

## Autounlock (opt-in, stake-only, DPAPI)

Unattended **staking** on an encrypted wallet needs the wallet unlocked after every boot or
restart. Autounlock is opt-in and unlocks **for staking only**, never for spending. The core does
it itself: you set it up once in the wallet (the GUI's *Autounlock for staking* dialog, or the
`setautounlock` RPC; `clearautounlock` turns it off), and the core reads its own file at startup.
The Core-Start launcher passes `-autounlockblob`, which names that file.

- **Secret at rest = DPAPI (machine key).** The passphrase is encrypted with `CryptProtectData` and
  the machine's key, and written to `<datadir>\autounlock\passphrase.dpapi` (`testnet\autounlock`
  for testnet), readable by the account, SYSTEM and Administrators only. It is never stored in the
  wallet, on a command line, or in a log. **Every account on the machine can decrypt a file made
  with the machine key**, so only the file's access control list keeps it to the wallet's account:
  it protects the wallet from other accounts, not from a program running as the wallet's account.
  For a wallet that must resist its own account being compromised, use the service mode, where
  the file is encrypted for the service account and lives in the service's data directory.
- **Physical access.** DPAPI's machine and service secrets are kept in the registry hives on the
  system disk. Without BitLocker, a copy of the disk is enough to decrypt the stored passphrase.
- **No resident helper.** Earlier versions ran a resident PowerShell helper
  (`\Gridcoin\Autounlock`) from a scheduled task.
  `Install-GridcoinCoreTask.ps1` removes that task, ends the helper and converts the old
  `passphrase.cred` file (decrypted with your own DPAPI key, so it must run as you) into the new
  file; pass `-NoAutounlockMigration` to skip the conversion and set autounlock up again in the
  wallet instead.

**Changing the wallet passphrase** re-encrypts the stored passphrase: the core that makes the
change writes the file again. If it cannot, it removes the file and logs why, and the wallet
stays locked after the next start; set autounlock up again then. **Turning it off** (the same
dialog, or `clearautounlock`) deletes the file and locks the wallet. The runbooks, including a
suspected compromise, are in
[`doc/running-unattended.md`](../../doc/running-unattended.md#changing-the-passphrase-turning-autounlock-off-and-a-suspected-compromise-windows).

## Status

The S4U registration, the shutdown script, the guards and the switch-over from the earlier
tasks are tested on a Windows 11 machine before each release. The earlier
registration was validated on Windows 11 against a populated mainnet wallet (2026-08-12).

**Gotcha worth knowing:** to stop the core, start the `Core-Stop` task; never end `Core-Start`.
