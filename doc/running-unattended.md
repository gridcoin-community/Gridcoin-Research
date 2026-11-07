# Running Gridcoin unattended (background service + stake-only autounlock)

This guide walks through running the Gridcoin **multiprocess** core as a background service that **stakes
unattended**, on **Linux** (systemd) and **Windows** (a scheduled task or a Windows service), including the
optional **stake-only autounlock** for an encrypted wallet.

It is a task-oriented walkthrough. For the multiprocess model itself, see [`multiprocess.md`](multiprocess.md);
for the Windows script reference, see [`../contrib/windows/README.md`](../contrib/windows/README.md).

## Is this for you?

Use this if you want the wallet to **run headless and keep staking** across reboots without leaving the GUI
open. In multiprocess mode the **core** (`gridcoinresearchd`) runs as a background service and owns the wallet
and the blockchain; the **GUI** (`gridcoinresearch`) is a separate program you open on demand and it *attaches*
to the running core. See [`multiprocess.md`](multiprocess.md) for the split model.

Staking on an **encrypted** wallet additionally needs the wallet unlocked after every start. The optional
autounlock does that, **for staking only**, with the passphrase stored encrypted on the machine (by `systemd-creds`
on Linux, by DPAPI on Windows): never in the wallet, on a command line, or in a log. It is opt-in and off by
default. What it promises, and what it does not, is the next section.

---

## What autounlock promises

> **Autounlock never leaves you weaker than unlocking for staking only yourself, by typing the passphrase after
> each start of the core.**

A wallet unlocked for staking only stakes, claims its research rewards and renews its beacon. Other operations
that use the wallet's keys or move coins need the passphrase again, for that one operation.
`getwalletinfo` reports the state as `unlock_scope`: `locked`, `staking_only` or `full`.

A full unlock is outside this promise. While the wallet is unlocked fully (**Unlock Wallet** in the GUI with
**For staking only** cleared, or `walletpassphrase` without its staking-only flag), anything that can reach the
core, over RPC or the GUI's connection, can spend until the wallet is locked again. Autounlock never unlocks fully.

The measure is the plain GUI wallet you unlock for staking yourself. Whoever can run programs as the account the
core runs as can do to an autounlocked wallet what they could already do to that wallet, and nothing more. What
that leaves depends on the mode.

### When the core runs as you (same-account mode, and the plain GUI)

Any program running as your account, malware included, can:

- stop the node, or lock the wallet;
- copy the encrypted `wallet.dat` and guess passphrases offline;
- capture the passphrase whenever you type it, into the GUI or into the command-line client;
- redirect staking rewards by editing your own configuration file or settings (side stakes), as with the plain
  GUI wallet;
- widen the node's network exposure by editing your own configuration file (`rpcallowip`, `rpcbind`, `proxy`,
  `upnp`);
- relabel address-book entries, so that a later send picks the wrong address. Check the destination address in
  the send confirmation, not only its label.

Someone who exploits a flaw in the core over the network runs with the core's account, which in this mode is
yours; the core holds the keys however it is run.

Without the passphrase, none of them can move coins, use the wallet's keys or obtain the passphrase through RPC or
the GUI's connection to the core.

### When the core runs as its own account (different-account mode)

The core runs as a dedicated account, `gridcoin`, and the desktop accounts lose access to its files, its memory
and its processes. A desktop user attaches the GUI through membership of the attach group, `gridcoin-gui`.
**Attach group = node operator plus wallet viewer.** A member, and any program running in a member's desktop
account, can:

- stop the node (the service starts it again);
- hold the core's single GUI connection;
- manage peers (`addnode`, `setban`), and change the settings that need no passphrase, such as staking on or off
  and the reserve balance;
- see balances and history;
- guess passphrases online, at the rate the core allows: each failure makes the next attempt wait longer, up to
  30 seconds;
- capture the passphrase when the member types it into the GUI.

A member cannot read the wallet file, the core's memory, its configuration file (which holds the RPC password)
or the stored passphrase. Every operation that moves value, changes where rewards go, widens the node's network
exposure, or moves a file in or out of the core asks for the passphrase, once per operation, whoever asks.

An **unencrypted** wallet has none of this "look, not spend" protection: every member can spend from it.

### Whoever holds the machine's disk

Unattended unlock means the machine can unlock the wallet by itself, so whoever holds its disk can do the same,
unless the disk is encrypted (LUKS, BitLocker, FileVault) or a TPM binds the secret. Typing the passphrase has no
such exposure.

On Linux, `systemd-creds` encrypts the stored passphrase with the host key `/var/lib/systemd/credential.secret`,
and on a machine with a TPM 2.0 chip with a key from that chip as well. **Without a TPM, the host key and the
credential file are enough to decrypt it**, so a copy of the disk is enough. With a TPM, the credential decrypts only on
this machine. Root on the machine can decrypt it either way: nothing here protects against root.

On Windows, DPAPI encrypts the stored passphrase with secrets of the machine (the same-account mode) or of the
service account (the service mode), and those secrets are kept in the registry hives on the system disk. **Without
BitLocker, a copy of the disk is enough to decrypt it.** An administrator or SYSTEM on the machine can decrypt it
either way.

---

## Linux (systemd)

### Choosing a mode

| Mode | Core runs as | IPC socket and cookie | Who can attach a GUI | Autounlock |
|---|---|---|---|---|
| Plain GUI (one process) | you | none | none | not offered |
| **Same account**: the per-user unit `gridcoinresearchd@<user>.service` (the default) | you | your data directory, owner-only | you | yes |
| Same account: `gridcoinresearchd -multiprocess` started by hand | you | your data directory, owner-only | you | no |
| **Different account**: the system service `gridcoinresearchd.service` (opt-in) | `gridcoin` | `/run/gridcoinresearchd` | members of `gridcoin-gui` | yes |

- **Same account is for convenience: staking without the GUI open, and autounlock. It adds no isolation from your
  own session; for higher security, use the different-account mode.** The per-user core runs as if you had started
  it by hand: your home, your groups, your configuration file, notify scripts, BOINC data and backup directory
  work as they do then, and switching back to the plain GUI is a matter of stopping the unit.
- **A file the core rewrites keeps the permissions it had.** That covers the settings file, the configuration file,
  `peers.dat`, `banlist.dat`, the log after a rotation and `wallet.dat` after encryption. The core warns at start
  where they are wider than it would make them, and never changes them itself. A file it creates new is owner-only.
  The IPC cookie and the autounlock file are the exceptions: each is written afresh with the narrowest
  permissions that work.
- **A core started by hand has no autounlock.** The helper that holds the passphrase serves only the main process
  of the unit it belongs to.
- **Supplementary groups ride along.** A per-user core has every group your account has, as any program of yours
  does; systemd cannot drop them. If your account is in a group that is equivalent to root or close to it
  (`docker`, `lxd`, `incus`, `libvirt`, `disk`, `wheel`, `sudo`, `adm`, `systemd-journal`; `enable-user` warns
  about these), use the different-account mode, or keep the desktop account out of those groups.
- **Mainnet and testnet in one account share trust.** A testnet core running as the account that holds a mainnet
  wallet can read and change that wallet's files. Run testnet as another account (the testnet system service runs
  as `gridcointestnet`) if you would not trust a test build with your mainnet wallet.
- **The system service's sandbox is best effort.** `ProtectSystem=strict`, `ProtectHome=yes`, `PrivateTmp=`,
  `PrivateDevices=`, a system-call filter and a list of address families narrow what that core can reach. They do
  not hide abstract Unix sockets, an X server's for example (whose own access control still applies), and sockets
  elsewhere in the file system, under `/var` say, stay reachable as far as their own permissions let the
  `gridcoin` account in. The per-user unit has no such sandbox.
- **Older systemd.** Before systemd 257, `PrivatePIDs=` is ignored with a log line, and the rest of each unit
  applies. Autounlock needs systemd 251 or later; Ubuntu 22.04 builds leave it out.
- **The core's memory.** The core marks itself non-dumpable as its first step, so other programs of its account
  cannot read its memory or attach a debugger. On hosts with Yama `ptrace_scope=0` (Debian's default) the packages
  install the daemon execute-only (mode 0711), which makes it non-dumpable from the moment it starts. A core you
  build and start yourself from a readable binary is open to programs of the same account for the moment before it
  does that itself. See [`multiprocess.md` → *What protects the core's memory*](multiprocess.md#what-protects-the-cores-memory).

The rest of this section uses the mainnet names and a user called `alice`. On testnet the names are:

| | Mainnet | Testnet |
|---|---|---|
| Daemon | `gridcoinresearchd` | `gridcointestnetd -testnet` |
| Service tool | `gridcoin-service` | `gridcointestnet-service` |
| Per-user unit | `gridcoinresearchd@<user>.service` | `gridcointestnetd@<user>.service` |
| System service | `gridcoinresearchd.service` | `gridcointestnetd.service` |
| Service account, attach group | `gridcoin`, `gridcoin-gui` | `gridcointestnet`, `gridcointestnet-gui` |
| Service configuration file | `/etc/gridcoin/gridcoin.conf` | `/etc/gridcoin-testnet/gridcoin.conf` |
| Service data directory | `/var/lib/gridcoinresearchd` | `/var/lib/gridcointestnetd` (its `testnet/`) |
| Service IPC directory | `/run/gridcoinresearchd` | `/run/gridcointestnetd` |
| Helper units | `gridcoin-autounlock@<user>.socket`, `gridcoin-autounlock.socket` | `gridcointestnet-autounlock@<user>.socket`, `gridcointestnet-autounlock.socket` |
| Credential, per user | `/etc/credstore.encrypted/gridcoinresearch-<user>.wallet-passphrase` | `/etc/credstore.encrypted/gridcointestnet-<user>.wallet-passphrase` |
| Credential, system service | `/etc/credstore.encrypted/gridcoinresearch.wallet-passphrase` | `/etc/credstore.encrypted/gridcointestnet.wallet-passphrase` |

### Same account: the per-user unit

```bash
sudo gridcoin-service enable-user alice
```

This writes the drop-in `/etc/systemd/system/gridcoinresearchd@alice.service.d/50-gridcoin-service.conf` with the
core's command line, reloads systemd, and enables and starts `gridcoinresearchd@alice.service`. It refuses a name
that is not a regular login account (unknown, uid 0, primary group 0, or a system account below `UID_MIN`), and
it refuses while the system service is enabled: the two conflict, and starting one stops the other. Then attach
the GUI:

```bash
gridcoinresearch -multiprocess
```

or choose **Connect to your own core** in the desktop menu's actions for Gridcoin.

The core uses its default data directory, `~/.GridcoinResearch`. For another one pass `--datadir=<dir>`: an
absolute path to an existing directory the user owns, with no symbolic link in it and none of whitespace, quotes,
backslash, `$`, `%` or `;`. Start the GUI with the same `-datadir`. Other paths, such as `backupdir=` and
`boincdatadir=`, go in the configuration file as usual: the per-user core reads them as a core started by hand
does.

`systemctl enable gridcoinresearchd@alice` on its own is not enough. Until the tool has written its drop-in, the
instance stops at once with `gridcoinresearchd@alice.service: run gridcoin-service enable-user alice first` and
exit status 78, and is not restarted.

- **Stop the core:** `sudo systemctl stop gridcoinresearchd@alice.service`. It starts again at boot.
- **Restarts:** the unit restarts the core after a crash (`Restart=on-failure`), not after a clean stop. A stop
  asked for from the GUI or with `gridcoinresearchd stop`, and the stop after `encryptwallet`, leave it down
  until `sudo systemctl start gridcoinresearchd@alice.service` or the next boot.
- **Back to the plain GUI:** stop the unit first; the plain GUI and the core cannot share one data directory.
- **Remove it:** `sudo gridcoin-service disable-user alice` disables the core and its helper socket and removes
  both drop-ins. A credential file stays until you remove it.

### Different account: the system service

`gridcoin-service enable` moves a user's wallet into the service account and starts the system service. Run it,
and its counterpart `disable`, **from another account's login: a root console, ssh as root, or another
administrator.** The tool refuses a run from the user's own session, whose processes it must end:

```
gridcoin-service: this command runs in alice's own login; run this from another account's login (a root console, ssh as root, or another administrator)
```

So alice logs out, and from a root console (or as another administrator):

```bash
sudo gridcoin-service enable alice
```

It checks the user, the data directories and the units before it changes anything, and then:

1. Disables alice's per-user core and helper socket and stops the system service. It refuses while alice has
   running processes; add `--terminate-sessions` to end her sessions instead (`loginctl terminate-user`).
2. Copies `~alice/.GridcoinResearch` into `/var/lib/gridcoinresearchd` as fresh files, read as alice and written
   as `gridcoin`, holding the data directory's lock throughout. Only the default data directory moves: a per-user
   unit with a `--datadir` of its own is refused, and so is a service data directory that is not empty.
3. Quarantines the configuration file, `gridcoinsettings.json`, `DB_CONFIG` and the core's dated backups of the
   first two in `/var/lib/gridcoin-service/quarantine/alice-main-<time>/` (root only) instead of carrying them
   over, and prints the keys it removed. A sanitised `gridcoinsettings.json` goes into the service's data
   directory, and a sanitised `gridcoinresearch.conf.sanitized` lies next to the quarantined configuration file
   for review. The service reads `/etc/gridcoin/gridcoin.conf`: copy what you need (`addnode=`, staking settings,
   `boincdatadir=`) into it from the sanitised file.
4. Moves the original data directory to `~alice/.GridcoinResearch.in-service-<time>`, readable by root only, so no
   second core can stake it. Remove it once the service wallet is verified.
5. Adds alice to `gridcoin-gui`, and enables and starts `gridcoinresearchd.service`.

Then alice logs in again (a new group membership takes effect at the next login) and starts the GUI with
**Connect to the system service** in the desktop menu's actions, which runs:

```bash
gridcoinresearch -multiprocess -ipcdir=/run/gridcoinresearchd -ipcserviceuser=gridcoin
```

The GUI connects only to a core running as `gridcoin`, in an IPC directory owned by `gridcoin` or root that nobody
else can write.

In this mode:

- **Backup Wallet** asks for the passphrase (unless the wallet is unlocked fully) and streams the backup to a
  file you choose. The configuration file belongs to the administrator and is not backed up; the settings file is
  backed up instead.
- **Open debug log** and **Open config** show where the file is, for example
  `/var/lib/gridcoinresearchd/debug.log`, and how an administrator reads it (`sudo less <path>`).
- **The service restarts the core whenever it stops** (`Restart=always`), so a stop from the GUI brings it back
  after 10 seconds. Stop it with `sudo systemctl stop gridcoinresearchd.service`, and keep it off with
  `sudo systemctl disable --now gridcoinresearchd.service`.
- **Root uses the command-line client as the service account.** Root is not an IPC peer, and the RPC listener
  belongs to `gridcoin`:

  ```bash
  sudo runuser -u gridcoin -- gridcoinresearchd -conf=/etc/gridcoin/gridcoin.conf -datadir=/var/lib/gridcoinresearchd getwalletinfo
  ```

  [`multiprocess.md` → *Administering the system service*](multiprocess.md#administering-the-system-service)
  shows the alternative, `rpcserviceuser=gridcoin` in the configuration file.
- **The configuration file is root's.** `/etc/gridcoin/gridcoin.conf` is `root:gridcoin`, mode 0640, in
  `/etc/gridcoin` (`root:gridcoin`, 0710). The package creates it with generated RPC credentials; the core reads
  it and never writes it.
- **The sandbox hides `/home`, `/root`, `/run/user` and `/tmp` from the core**, and the core can write only below
  `/var/lib/gridcoinresearchd`. Put `boincdatadir=` somewhere the `gridcoin` account can read, and `backupdir=`
  (if you set it) below the data directory; the core warns at start when the backup directory is not writable.
- **Do not start the plain GUI as alice** while her wallet is in the service. It would make a new wallet in
  `~/.GridcoinResearch`, and `disable` then refuses until that is moved aside.

To move the wallet back, again from another account's login:

```bash
sudo gridcoin-service disable alice
```

It copies the wallet back into `~alice/.GridcoinResearch` (which must not exist, or hold only testnet's data
directory and the configuration file), restores alice's own configuration file from the stash, empties the
service's data directory as `gridcoin`, removes alice from `gridcoin-gui`, and disables the system service and its
helper socket. It takes `--terminate-sessions` too, and it moves out only the wallet that `enable` moved in, for
the user it moved it for. It ends by printing the `enable-user` command for a per-user core.

A host with no wallet to move can start the service on a new wallet (`sudo systemctl enable --now
gridcoinresearchd.service`), encrypt it (`sudo runuser -u gridcoin -- gridcoinresearchd
-conf=/etc/gridcoin/gridcoin.conf -datadir=/var/lib/gridcoinresearchd -stdinwalletpassphrase encryptwallet`), and
admit desktop users with `sudo gpasswd -a alice gridcoin-gui`; `disable` does not apply to such a wallet.

### Autounlock setup

Autounlock needs systemd 251 or later. On a build without the autounlock units (Ubuntu 22.04) the tool refuses
with `this build has no autounlock units (they need systemd 251 or later), so autounlock cannot be enabled`.

**1. Encrypt the passphrase into the credential file of the unit.** For alice's per-user core:

```bash
sudo -v
sudo mkdir -p -m 0700 /etc/credstore.encrypted
cred=/etc/credstore.encrypted/gridcoinresearch-alice.wallet-passphrase
systemd-ask-password 'Wallet passphrase:' \
    | sudo systemd-creds encrypt --name=wallet-passphrase --tpm2-pcrs="" - "$cred"
sudo chmod 0600 "$cred"
```

For the system service, use `cred=/etc/credstore.encrypted/gridcoinresearch.wallet-passphrase`.

- **`sudo -v` first,** so that sudo asks for your password before the pipeline starts, and its prompt and the
  passphrase prompt do not compete for the terminal.
- **`systemd-ask-password` prompts for the passphrase,** so it is never part of a command. One trailing line break
  is ignored. Never put the passphrase in the command itself (`printf '%s' 'MY-PASSPHRASE' | …`): the shell
  history keeps it, and sudo logs every command it runs (`log_allowed`, on by default). On a host where sudo also
  logs input (`log_input`), it records what the command reads, the passphrase included; there, run the same
  pipeline without `sudo` from a root shell (a root console, or after `su -`).
- **Keep `--name=wallet-passphrase`.** The units load the credential under that name, and systemd refuses an
  encrypted credential whose embedded name differs.
- **Keep `--tpm2-pcrs=""`.** On a host with a TPM, `systemd-creds` would otherwise also seal the credential to
  PCR 7, which measures the Secure Boot keys and dbx. A BIOS *restore factory keys*, a dbx update (fwupd ships
  those routinely), a firmware update or a Secure Boot toggle then changes PCR 7, the TPM refuses to unseal the
  credential for good, and the helper fails with `status=243/CREDENTIALS`: the wallet stays locked from the next
  restart on. Without a PCR policy the credential stays bound to this machine's TPM and host key and survives
  ordinary firmware maintenance. What is given up is the TPM's refusal after a Secure Boot change. To keep it,
  drop `--tpm2-pcrs=""` and re-encrypt the passphrase after every BIOS key, dbx or firmware change. A credential
  already sealed to PCR 7 that no longer unseals cannot be recovered: run the pipeline again.
- **On a host without a TPM**, the host key decrypts the credential on its own; see
  [*Whoever holds the machine's disk*](#whoever-holds-the-machines-disk).
- **The file stays root's, mode 0600.** Never give it to another account: systemd decrypts it for the helper.

**2. Turn autounlock on:**

```bash
sudo gridcoin-service autounlock enable alice        # the system service: --system
```

It refuses when the credential file is missing (`no credential at <path>; create it first`), and warns when the
file is not root's with mode 0600. It writes the drop-in `60-gridcoin-autounlock.conf` for the core, which then
starts with `-autounlocksocket=/run/gridcoin-autounlock/alice.sock`, enables `gridcoin-autounlock@alice.socket`,
and restarts a running core without waiting, so that it unlocks.

How it works: at each start, once the wallet is loaded, the core connects to that socket. systemd starts the
helper, `gridcoin-autounlock@alice.service`, under the system account `gridcoin-autounlock` (testnet
`gridcointestnet-autounlock`), which no desktop process uses, with the credential decrypted for that account
alone. The helper serves the passphrase only to the main process of `gridcoinresearchd@alice.service` as
systemd records it (a child of the core, such as a notify command, is refused), and exits. The core unlocks for
staking only, never fully, and wipes its copy. If the helper does not serve it, the core tries five times over
about a minute, then logs why and stays locked. The core never sends anything on that socket.

### Changing the passphrase, turning autounlock off, and a suspected compromise

**Changing the passphrase:**

1. Change it with the command-line client, which reads both passphrases from standard input (the old one, then
   the new one; at a terminal it prompts, and asks for the new one twice):

   ```bash
   gridcoinresearchd -stdinwalletpassphrase walletpassphrasechange
   ```

   For the system service, run it as `sudo runuser -u gridcoin -- gridcoinresearchd
   -conf=/etc/gridcoin/gridcoin.conf -datadir=/var/lib/gridcoinresearchd -stdinwalletpassphrase
   walletpassphrasechange`. A wallet unlocked for staking stays unlocked.
2. Encrypt the new passphrase into the same credential file, with the pipeline of the setup.
3. Restart the core: `sudo systemctl restart gridcoinresearchd@alice.service` (or `gridcoinresearchd.service`).
4. Check that `unlock_scope` is `staking_only` again (see [*Verifying it works*](#verifying-it-works)).

A restart between steps 1 and 2 leaves the wallet locked, and the core logs `autounlock: the passphrase did not
unlock the wallet; it stays locked`.

**Turning autounlock off:**

```bash
sudo gridcoin-service autounlock disable alice       # or --system
sudo rm /etc/credstore.encrypted/gridcoinresearch-alice.wallet-passphrase
```

`disable` turns the helper socket off, removes the drop-in, and restarts a running core, which comes back locked.
It prints `The credential file remains until you remove it: <path>`, and the file is yours to remove. Removing it
does not remove copies of it in file system snapshots or backups. If such a copy, together with the host key, may
be out of your hands, change the wallet passphrase as well.

**On a suspected compromise** of the wallet, or of a machine that held it or a backup of it, changing the
passphrase is not enough: the keys stay the same, and every earlier copy of `wallet.dat`, backups included, still
opens with the old passphrase. Move the coins to a new wallet instead:

1. Stop the core and move its `wallet.dat` aside (keep it for now). Start the core: it creates a new wallet.
   Encrypt it with a new passphrase (`gridcoinresearchd -stdinwalletpassphrase encryptwallet`; the core stops
   afterwards, so start it again), take an address from it (`getnewaddress`), and back it up.
2. Stop the core, put the old `wallet.dat` back, start it, and send the whole balance to that address.
3. Stop the core, put the new `wallet.dat` back, and start it. Encrypt the new passphrase into the credential
   file, as above.
4. Delete the old wallet, every backup of it, and the automatic backups of the new wallet from before its
   encryption (`encryptwallet` lists them), in the order of
   [*Purging pre-encryption backups*](#purging-pre-encryption-backups).

### Multiple wallets, and mainnet and testnet side by side

- **One core, one wallet.** On one host, each network runs either its system service or per-user cores, never
  both: the units conflict.
- **At most one per-user core of each network on a host.** systemd cannot keep two instances of the template
  apart, and two cores of one network collide on the P2P and RPC ports unless each user's configuration file sets
  its own `port=` and `rpcport=`.
- **Mainnet and testnet run side by side.** Their units, accounts, groups, sockets and ports differ, so
  `gridcoinresearchd@alice.service` and `gridcointestnetd@alice.service` (or the two system services) run together.
  Each network's tool manages its own units, and the trust note under [*Choosing a mode*](#choosing-a-mode)
  applies.
- **Both networks in one data directory.** Testnet's data directory is `~/.GridcoinResearch/testnet`, so a
  mainnet `enable` leaves `testnet/` and the configuration file (which testnet reads too) in place, and a testnet
  `enable` moves only `testnet/`. Each refuses while the other network's per-user core of that user runs:

  ```
  gridcoin-service: gridcointestnetd@alice.service is running and shares alice's datadir and conf; stop it first with: systemctl stop gridcointestnetd@alice.service
  ```

### Upgrading

#### Debian and Ubuntu packages

On an upgrade from a release with the earlier units (a forking `gridcoinresearchd.service`, and autounlock by
`gridcoinresearchd-autounlock.service` or `gridcoinresearchd-autounlock@<user>.service`):

- Before the new files are unpacked, the package records which of the old units were enabled, running or
  overridden, and turns the old autounlock units off. Running cores keep running.
- After unpacking, it creates the service account, the attach group and the helper's account
  (`systemd-sysusers`) and `/etc/gridcoin` (`systemd-tmpfiles`). It creates `/etc/gridcoin/gridcoin.conf` (`root:gridcoin`, 0640, with generated RPC
  credentials) when there is none, or makes an existing one owned by root readable by the group `gridcoin` and
  nobody else. Then it runs `gridcoin-service switch-over --state=/var/lib/gridcoin-service/switchover-main.state`.
- The **switch-over** moves each recorded service to the new units:
  - **Per-user cores:** every drop-in of `gridcoinresearchd@<user>.service` that the tool did not write is renamed
    to `<name>.disabled-by-gridcoin-switchover`, after the `-datadir` of its command line is read; the tool's own
    drop-in carries that data directory. An old command line with any other option the new one would drop is
    refused, and so are a copy of the unit in `/etc/systemd/system` and a drop-in for every instance
    (`gridcoinresearchd@.service.d`) that sets `ExecStart=`, `Type=`, `PIDFile=`, `NotifyAccess=` or `User=`.
    Autounlock is turned on again with the credential file where it is. The core is re-enabled if it was enabled,
    and restarted without waiting if it was running. A per-user core is not switched while the system service is
    enabled or running.
  - **The system service:** a drop-in in `/etc/systemd/system/gridcoinresearchd.service.d` that sets
    `ExecStart=`, `Type=`, `PIDFile=`, `NotifyAccess=`, `User=` or `Group=`, and a copy of the unit at
    `/etc/systemd/system/gridcoinresearchd.service`, are renamed to `<name>.disabled-by-gridcoin-switchover`. A
    `-conf` or `-datadir` they set is carried into the tool's drop-in, `gridcoinresearchd.service.d/50-gridcoin-service.conf`,
    so the service never starts on a new data directory. A mask is left as it is.
  - **Refused, and left in place:** a command line in such an override or copy **without `-datadir`**, since which
    data directory the node used cannot be told from outside it (add `-datadir=<the directory this node actually
    uses>` to its `ExecStart=`, then rerun the switch-over); a command line with any other option the new unit
    would drop, or with no `-conf` when its data directory has no `gridcoinresearch.conf`; on testnet, one without
    `-testnet`; a file to be renamed that also sets `BindPaths=`, `BindReadOnlyPaths=`, `RootDirectory=`,
    `RootImage=`, `TemporaryFileSystem=`, `MountImages=`, `ExtensionDirectories=` or `RequiresMountsFor=` (move
    those to a drop-in of their own); and a carried configuration file that anyone but root and `gridcoin` can
    write. The message names the file and what to change. Until then a copy of the unit keeps running on its old
    command line, and drop-ins alone keep the service from starting if they make the daemon fork, or run it on
    its old command line otherwise.
  - **The system service's autounlock:** its credential, `/etc/gridcoin/wallet-passphrase.cred`, stays where it
    is, owned by root with mode 0600, and the drop-in
    `/etc/systemd/system/gridcoin-autounlock.service.d/50-existing-credential.conf` points the helper at it. It is
    not decrypted, re-encrypted or moved.
- Then the package restarts the system service and its helper if they run. Later upgrades also restart running
  per-user cores, without waiting.
- When the switch-over does not finish, the upgrade says why and keeps what is left. Fix the cause and run
  `sudo gridcoin-service switch-over --state=/var/lib/gridcoin-service/switchover-main.state`. Until then a core
  it left is not restarted, and once stopped it may not start again.
- Afterwards `systemctl enable gridcoinresearchd@<user>` is not enough for a new per-user core; use
  `gridcoin-service enable-user <user>`.
- `NEWS.Debian` says all this for the package: `apt-listchanges` shows it, or read
  `/usr/share/doc/gridcoinresearchd/NEWS.Debian.gz`.

The testnet package does the same with `gridcointestnet-service` and `switchover-test.state`. Its system service
runs as `gridcointestnet`, and systemd gives `/var/lib/gridcointestnetd` to that account at the service's first
start. If `/etc/gridcoin-testnet/gridcoin.conf` is owned by `gridcoin`, give it to `root:gridcointestnet`, mode
0640; the package does that only for one owned by root.

#### Ubuntu 22.04

Its systemd (249) is too old for autounlock, so its builds have no autounlock units. The upgrade turns the old
autounlock units off, and the core starts with the wallet locked. Unlock it for staking after each start, from
the GUI or with `gridcoinresearchd -stdinwalletpassphrase walletpassphrase 99999999 true`.

#### Built from source (`cmake --install`)

`cmake --install` puts the units, the `sysusers.d` and `tmpfiles.d` entries, the helper and `gridcoin-service`
under the install prefix (`/usr/local` by default), and nothing runs them. After each install, as root:

```bash
prefix=/usr/local
sudo systemd-sysusers "$prefix/lib/sysusers.d/gridcoinresearchd.conf"
sudo systemd-tmpfiles --create "$prefix/lib/tmpfiles.d/gridcoinresearchd.conf" "$prefix/lib/tmpfiles.d/gridcoin-service.conf"
sudo systemctl daemon-reload
sudo gridcoin-service switch-over --scan
```

`--scan` finds what the package's records would have listed (old units enabled through `/etc/systemd/system`,
running per-user cores, the system service's overrides) and switches it over as above, restarting what it
switched. It prints `Nothing to switch over.` when there is nothing left to do, and keeps what fails for a rerun
with `--state`. A running system service it had nothing to switch for runs the new daemon from its next start:
`sudo systemctl try-restart gridcoinresearchd.service`.

For the system service, `/etc/gridcoin/gridcoin.conf` must exist, owned by root, readable by the group `gridcoin`
and nobody else, with RPC credentials. To create one without the password ever appearing on a screen or a command
line (`set -C` refuses to overwrite an existing file):

```bash
sudo sh -c 'set -C; umask 027; { echo rpcuser=gridcoinrpc; echo "rpcpassword=$(od -A n -t x1 -N 32 /dev/urandom | tr -d " \n")"; } > /etc/gridcoin/gridcoin.conf && chgrp gridcoin /etc/gridcoin/gridcoin.conf'
```

An existing one is made so with `sudo chown root:gridcoin /etc/gridcoin/gridcoin.conf && sudo chmod 0640
/etc/gridcoin/gridcoin.conf`.

An earlier `cmake --install` also left `$prefix/lib/systemd/system/gridcoinresearchd-autounlock.service` and
`$prefix/share/gridcoin/gridcoin_autounlock.py`. Remove them once the switch-over has run, then `sudo systemctl
daemon-reload`.

#### Other packages

The AUR package and theMarix's RPM packages are built by their maintainers. One that ships these units may or may
not run the steps above from its own scripts; after upgrading it, run `sudo gridcoin-service switch-over --scan`,
which prints `Nothing to switch over.` when there is nothing left.

### Verifying it works

- **The wallet is unlocked for staking only.** As alice:

  ```bash
  gridcoinresearchd getwalletinfo
  ```

  shows `"unlock_scope": "staking_only"`. For the system service, as root:

  ```bash
  sudo runuser -u gridcoin -- gridcoinresearchd -conf=/etc/gridcoin/gridcoin.conf -datadir=/var/lib/gridcoinresearchd getwalletinfo
  ```

  `locked` means autounlock did not happen (see below); `full` means someone unlocked the wallet fully since. An
  unencrypted wallet always reads `full`.
- **The helper served the core.** `sudo journalctl -u gridcoin-autounlock@alice.service` (the system service:
  `gridcoin-autounlock.service`) shows a line like
  `gridcoin-autounlock-helper: served gridcoinresearchd@alice.service main pid 4242`.
- **The core unlocked.** Its `debug.log` has `autounlock: wallet unlocked for staking`.

### Troubleshooting

- **`run gridcoin-service enable-user alice first`, exit status 78.** The per-user unit was enabled without the
  tool. Run `sudo gridcoin-service enable-user alice`.
- **`enable-user` refuses with `gridcoinresearchd.service is enabled and conflicts with the per-user unit`.**
  One network runs either the system service or per-user cores. Disable the system service first, or use it.
- **`enable` or `disable` refuses with `this command runs in alice's own login`, or `alice has running
  processes`.** Run it from another account's login, after alice has logged out, or add `--terminate-sessions`.
- **The GUI cannot connect to the system service.** A new member of `gridcoin-gui` must log in again (`id` lists
  the groups of the session). The core's log, `/var/lib/gridcoinresearchd/debug.log`, names a refused connection
  with `IPC: refusing peer uid …`. Check that the service runs: `systemctl status gridcoinresearchd.service`.
- **The wallet stays locked after a start.** The core's `debug.log` says why:
  - `autounlock: attempt 1 of 5 failed: connect failed: …`: the helper socket is not there, or not open to the
    core's account. Run
    `sudo gridcoin-service autounlock enable alice` again, and check `systemctl status gridcoin-autounlock@alice.socket`.
  - `the helper refused (see journalctl -u <helper unit>)`: read the helper's journal, below.
  - `autounlock: the passphrase did not unlock the wallet; it stays locked`: the credential holds another
    passphrase. Encrypt the current one into it, and restart the core.
  - `autounlock: giving up after 5 attempts; the wallet stays locked`: the attempts above all failed.
- **The helper's exit status** (`journalctl -u gridcoin-autounlock@alice.service`). systemd may print a name of its
  own beside the number, such as `3/NOTIMPLEMENTED`; for the helper only the number counts.

  | Status | Journal line | Meaning |
  |---|---|---|
  | 3 | `refused peer pid … uid …: <reason>` | What connected was not the main process of the core's unit, or ran as another account. A core started by hand is refused; so is any other program. |
  | 4 | `no connection within 5 s` | Started, but nothing was waiting on the socket. Harmless. |
  | 5 | `credential unusable: missing`, `empty`, `too long` or `not valid UTF-8` | Encrypt the passphrase again. |
  | 6 | `systemd query failed for <unit>: …` | The helper could not ask systemd for the core's main process, so it read no credential. Usually D-Bus (below). |
  | 64 | `usage: …` | The helper unit's command line was changed. |
  | 70 | `internal error: <type>` | A bug; please report it. |
  | 243 (`CREDENTIALS`) | from systemd, before the helper runs | systemd could not decrypt the credential: a missing file, one encrypted without `--name=wallet-passphrase`, on another machine, or sealed to a PCR that has changed (see `--tpm2-pcrs=""` above). Encrypt it again. |

- **D-Bus is missing.** The helper unit requires `dbus.socket`, since it asks systemd over D-Bus which process is
  the core's main process. On a minimal host or container without D-Bus the helper cannot start, and the wallet
  stays locked. Install the distribution's D-Bus package (`dbus`, or `dbus-broker`).
- **Autounlock does not unlock at boot inside a systemd container.** The helper logs `credential unusable: missing`
  at boot, while a later restart of the core works. A container started with a private root mount (podman's
  default) never marks `/` shared at boot, as systemd does only outside a container, so a unit started at boot does
  not see the credentials PID 1 mounts for it. Make `/` shared at boot, as a host's systemd does: for podman, a unit
  running `mount --make-rshared /` before `sysinit.target`, or the container manager's own option.
- **systemd warns that it cannot apply IP firewalling** for the helper. Without BPF support in the kernel,
  `IPAddressDeny=` is not applied. The helper has no network either way: `PrivateNetwork=yes` gives it none.
- **The switch-over did not finish.** The upgrade, or `--scan`, printed why and the command to rerun once the cause
  is fixed: `sudo gridcoin-service switch-over --state=/var/lib/gridcoin-service/switchover-main.state`.

### What `-printtoconsole` puts in the journal

The units start the core without `-printtoconsole`. It logs to `debug.log` in its data directory, which only its
own account (and root) can read. The journal gets systemd's own lines about the unit (start, stop, exit status)
and the messages the core writes to standard error: the errors and warnings it would show in a dialog, such as
one that stops its start.

With `printtoconsole=1` in the configuration file (or `-printtoconsole` on the command line), every line of
`debug.log` goes to the journal as well: peers and their addresses, blocks, staking and wallet events, warnings
and errors. The journal usually has more readers than the data directory (root, members of `systemd-journal`,
and on many distributions members of `adm` or `wheel`), may be kept longer, and may be forwarded to syslog or
another machine. For the system service, that shows the core's log to readers who cannot open its data directory.
The core does not write the RPC password or a wallet passphrase to its log, with one exception: never combine
`-printtoconsole` with `-ipclogtrace`, a diagnostic option that writes every IPC request, passphrases included,
in the clear.

---

## Purging pre-encryption backups

Encrypting a wallet does not encrypt the copies made before. Every backup taken before encryption, the automatic
ones the core writes to its backup directory (`walletbackups` in the data directory, or the directory set by
`-backupdir`) included, still holds the unencrypted keys: anyone who has one can spend the coins without the
passphrase.

`encryptwallet` (and the GUI's closing message after encryption) lists the automatic backups from before the
encryption by name, such as `wallet.dat-2026-11-05T12-00-00Z`. Nothing deletes them for you: a backup may be the
last way back to the coins. Delete them yourself, in this order:

1. **Check that the new passphrase works.** Start the core again (it stops after `encryptwallet`) and unlock the
   wallet with the new passphrase, for example for 60 seconds, staking only:
   `gridcoinresearchd -stdinwalletpassphrase walletpassphrase 60 true`.
2. **Make a fresh backup of the encrypted wallet** (**Backup Wallet** in the GUI, or `backupwallet`), and keep it
   somewhere safe.
3. **Delete the listed files, and every backup made by hand before encryption**: copies on other disks, USB
   sticks, other machines and cloud storage.

Look further where these apply:

- A wallet moved into the system service with `gridcoin-service enable` was copied with its backup directory, and
  the original stays in the stash `~<user>/.GridcoinResearch.in-service-<time>` until you remove it. Remove the
  stash once the service wallet is verified. The service's own backups are under
  `/var/lib/gridcoinresearchd/walletbackups`; root deletes them there.
- A deleted file can survive in file system snapshots, in backups of the whole disk, and in the free space of the
  medium. Treat those copies the same way.

---

## Windows

### Choosing a mode (Windows)

| Mode | Core runs as | IPC socket and cookie | Who can attach a GUI | Autounlock |
|---|---|---|---|---|
| Plain GUI (one process) | you | none | none | not offered |
| **Same account**: the core task `\Gridcoin\Core-Start` (an administrator's own wallet) | you | your data directory, owner-only | you | yes |
| Same account: `gridcoinresearchd -multiprocess` started by hand | you | your data directory, owner-only | you | only with `-autounlockblob` |
| **Service mode**: the Windows service `gridcoinresearchd` (any wallet holder; an administrator sets it up) | `NT SERVICE\gridcoinresearchd` | `%ProgramData%\Gridcoin\ipc\mainnet` | members of `Gridcoin GUI Users` | yes |

What each mode concedes is the list under [*What autounlock promises*](#what-autounlock-promises): the same-account
mode concedes the list for a core that runs as you, and the service mode the list for the attach group
(**attach group = node operator plus wallet viewer**). The plain GUI is unchanged.

- **Same account is for convenience: staking without the GUI open, and autounlock. It adds no isolation from your
  own session; for higher security, use the service mode.**
- **On Windows, any program running as you can obtain your wallet passphrase whenever same-account autounlock is set up; for protection against that, use the service mode.**
  The stored passphrase is encrypted with the machine's DPAPI key, which every account on the machine can use;
  only the file's access list keeps it to your account.
- **The task mode is for an administrator's own wallet.** Only an administrator can register a task that starts at
  boot, and the task is registered for the account that runs `Install-GridcoinCoreTask.ps1`, with the S4U logon:
  Windows stores no password for it. A standard user's wallet uses the service mode, which an administrator sets
  up.
- **The limits of an S4U task.** It has no network credentials and no access to your own DPAPI keys. A data
  directory or backup directory on a network share is out of its reach, and so is an AppData folder redirected to a
  network share: use a local data directory (`-DataDir`) or the service mode. Some accounts cannot get an S4U logon
  (some Microsoft Entra-only accounts); for them, use the service mode. A domain account's S4U logon needs a domain
  controller, so Core-Start waits a minute after boot and retries; a domain-joined machine that may start away from its
  domain is better served by the service mode.
- **Started elevated, the core and the GUI give up the administrator rights first.** They start themselves again
  without them, so what they create belongs to you and not to Administrators; `debug.log` then has
  `Started without administrator rights: the process that started Gridcoin had them.`
- **Keep UAC on.** `Start-GridcoinGui.ps1` refuses to run elevated, and with UAC off every console an administrator
  opens is elevated.
- **A file the core rewrites keeps the permissions it had.** That covers the settings file, the configuration file,
  `peers.dat`, `banlist.dat`, the log after a rotation and `wallet.dat` after encryption. The core warns at start
  where they are wider than it would make them, and never changes them itself. A file it creates new is owner-only
  (your account and SYSTEM). The IPC cookie and the autounlock file are the exceptions: each is written afresh with
  the narrowest permissions that work. The installer reports such files on an upgrade (below).
- **Mainnet and testnet in one account share trust.** A testnet core running as the account that holds a mainnet
  wallet can read and change that wallet's files. Run testnet as the testnet service if you would not trust a test
  build with your mainnet wallet.
- **The core's memory.** A program running as you can read the memory of a core running as you, the plain GUI
  wallet's included. The service's core runs as its own account, whose memory a desktop account cannot read unless
  it is an administrator.

The rest of this section uses the mainnet names, a user called `alice`, and the default install directory
`C:\Program Files\GridcoinResearch`. On testnet the names are:

| | Mainnet | Testnet |
|---|---|---|
| Service, and its account | `gridcoinresearchd`, `NT SERVICE\gridcoinresearchd` | `gridcointestnetd`, `NT SERVICE\gridcointestnetd` |
| Attach group | `Gridcoin GUI Users` | `Gridcoin Testnet GUI Users` |
| Service configuration file | `%ProgramData%\Gridcoin\conf\mainnet.conf` | `%ProgramData%\Gridcoin\conf\testnet.conf` |
| Service data directory | `%ProgramData%\Gridcoin\mainnet` | `%ProgramData%\Gridcoin\testnet` (its `testnet\`) |
| Service IPC directory | `%ProgramData%\Gridcoin\ipc\mainnet` | `%ProgramData%\Gridcoin\ipc\testnet` |
| `gridcoin-service.exe` option | none | `--network=test` |
| Same-account data directory | `%APPDATA%\GridcoinResearch` | `%APPDATA%\GridcoinResearch\testnet` |
| Autounlock file | `autounlock\passphrase.dpapi` in the data directory | the same, in `testnet\` |

**Before the first sync, exclude the block data and the block index from Defender** (elevated; for the service,
use `%ProgramData%\Gridcoin\mainnet` in place of `$env:APPDATA\GridcoinResearch`):

```powershell
Add-MpPreference -ExclusionPath "$env:APPDATA\GridcoinResearch\blk*.dat"
Add-MpPreference -ExclusionPath "$env:APPDATA\GridcoinResearch\txleveldb"
```

The first cuts an initial sync to about a third of its time, and the second takes about 26 seconds off every start
of a large wallet; the wildcard is needed because block files roll over at about 2 GB. The measurements are in
[`../contrib/windows/README.md`](../contrib/windows/README.md#windows-defender-exclusion-do-this-before-syncing).

### Same account: the core task

Run this as `alice`, an administrator, in an elevated PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
cd "C:\Program Files\GridcoinResearch\windows"
.\Install-GridcoinCoreTask.ps1
Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Core-Start'
```

Set the policy in the session like this and call the scripts directly; `powershell.exe -ExecutionPolicy Bypass
.\script -Arg "C:\Program Files\..."` re-parses the command line and splits the spaced path. Do not pass `-DataDir`
for the default data directory: the core creates that itself, owner-only, on its first start, whereas a named
data directory must exist already.

`Install-GridcoinCoreTask.ps1` registers **Core-Start** (at boot, through a launcher that runs
`gridcoinresearchd.exe -multiprocess -autounlockblob` and retries once after two minutes) and **Core-Stop** (on
demand), and adds an inbound firewall rule for the daemon. It refuses:

- in a console that is not elevated, with the instructions for a standard-user wallet;
- while the Windows service of the network is enabled;
- when the launcher, the core or any directory above them can be changed by an account other than an
  administrator, SYSTEM or you, or when the data directory or its configuration file belongs to another account.

Then, from a normal (not elevated) console, attach the GUI:

```powershell
cd "C:\Program Files\GridcoinResearch\windows"
.\Start-GridcoinGui.ps1                  # -CreateShortcut adds a Start menu entry
```

- **Stop the core by starting Core-Stop:** `Start-ScheduledTask -TaskPath '\Gridcoin\' -TaskName 'Core-Stop'`. It
  runs `gridcoinresearchd.exe stop`, which flushes the databases. Ending the Core-Start task instead is a hard kill
  with no flush.
- **A clean flush when Windows shuts down** needs a Group Policy shutdown script, because Windows does not wait for
  a task: `.\Set-GridcoinShutdownFlush.ps1` (elevated) sets one up, and `-Remove` takes it away. See
  [`../contrib/windows/README.md`](../contrib/windows/README.md#clean-database-flush-on-os-shutdown).
- **Remove the tasks:** `.\Uninstall-GridcoinCoreTask.ps1` (elevated); `-All` also removes the shutdown script and
  the firewall rule. It never deletes a wallet file.
- **One pair of tasks per installation.** The pair shares the installation's launcher and the shutdown script, so
  `Install-GridcoinCoreTask.ps1` refuses a second pair in another `-TaskFolder`. Run a second wallet, such as
  testnet's, in the service mode.

**Coming from the earlier task scripts.** `Install-GridcoinCoreTask.ps1`, and the installer on an upgrade, replace
the earlier tasks with the S4U ones, end the earlier resident autounlock helper,
and convert its `passphrase.cred` into the new autounlock file. The conversion decrypts the old file with your own
DPAPI key, so it works only in a desktop session of yours; pass `-NoAutounlockMigration` to skip it and set
autounlock up again in the wallet instead. The installer keeps what was turned off: a disabled Core-Start, or one
whose boot trigger is off, is registered again the same way, and an Autounlock task that was turned off is not
converted. Where it does not convert, it deletes the old `passphrase.cred`, and `UPGRADE-NOTICE.txt` says so, or
names the file to delete when it could not. Another account's earlier tasks are removed; the notice tells an
administrator's account to run `Install-GridcoinCoreTask.ps1` signed in as itself, and a standard account to ask an
administrator for the service mode (`gridcoin-service.exe install`, then `enable --user=<account>`).

### Service mode: the Windows service

An administrator sets the service up, in an elevated console:

```powershell
cd "C:\Program Files\GridcoinResearch\daemon"
.\gridcoin-service.exe install
.\gridcoin-service.exe enable --user=alice
.\gridcoin-service.exe status
```

- **`install`** creates the service, disabled; the attach groups; the directories under `%ProgramData%\Gridcoin`,
  each with its stated access list; the service's configuration file, with generated RPC credentials, when there
  is none; and the inbound firewall rule for the daemon. The service reads its configuration file and cannot
  change it.
- **`enable --user=alice`** moves alice's wallet into the service and starts it. alice's cores must be stopped: it
  refuses while a `gridcoinresearch.exe` or `gridcoinresearchd.exe` of hers runs, or while her data directory is
  locked. It disables her Core-Start task of the same network if she has one, copies her data directory (`--from=` for another one),
  reads every copied file back, and then removes her copy. Her configuration file and `DB_CONFIG` are not carried
  over (the service reads only its own configuration file); `enable` names each file it set aside, and each setting
  it dropped from `gridcoinsettings.json` because the core takes it only from its configuration file. It adds alice
  to the attach group and prints, first:

  ```
  Enabled. alice must sign out and sign in again before a GUI can attach (group membership is read at sign-in).
  ```

  Autounlock does not move with the wallet: set it up again (below).
- **alice attaches the GUI** after signing in again, from a normal console: `.\Start-GridcoinGui.ps1 -Service` in
  the `windows\` folder (`-Network test` for testnet, `-CreateShortcut` for a Start menu entry).
- **What the GUI does differently.** The configuration file belongs to the service and holds its RPC password, so
  backing it up from the GUI is refused, and *Open debug log* and *Open config file* show the file's path instead
  of opening it.
- **Stopping.** `Stop-Service gridcoinresearchd`, elevated, stops the service. A stop from the GUI or over RPC is
  followed by a restart: the service's recovery actions start it again.
- **`disable`** stops the service, sets it to Disabled, and moves the wallet back where it came from (`--to=` for
  another place), owner-only for alice; she leaves the attach group, and a Core-Start task that `enable` disabled
  is enabled again. It ends with `Disabled. The wallet is back in <directory>; <account> can start the plain GUI.`,
  and how to get unattended start: `Install-GridcoinCoreTask.ps1` for an administrator's own wallet, the service mode
  for a standard account's.
- **`uninstall`** removes the service, its firewall rule and the empty directories. It refuses while the service is
  enabled or its data directory holds a wallet.
- **The stored passphrase** is encrypted for the service account. Code that runs as the service can decrypt it, as
  the core must; a desktop account cannot.

### Autounlock setup (Windows)

Autounlock is set up in the wallet: **Settings → Autounlock for staking...** in the GUI, or `setautounlock`.
The core checks the passphrase, encrypts it into the autounlock file, and reads that file at each start when it was
started with `-autounlockblob`, as Core-Start and the service start it. From the command line, as alice in a
normal console:

```powershell
& "C:\Program Files\GridcoinResearch\daemon\gridcoinresearchd.exe" -stdinwalletpassphrase setautounlock
```

For the service, alice uses the GUI, or an administrator runs, in an elevated console:

```powershell
& "C:\Program Files\GridcoinResearch\daemon\gridcoinresearchd.exe" -conf="$env:ProgramData\Gridcoin\conf\mainnet.conf" -datadir="$env:ProgramData\Gridcoin\mainnet" -stdinwalletpassphrase setautounlock
```

### Changing the passphrase, turning autounlock off, and a suspected compromise (Windows)

**Changing the passphrase:**

1. Change it in the GUI (**Settings → Change passphrase**) or with
   `gridcoinresearchd.exe -stdinwalletpassphrase walletpassphrasechange` (the old one, then the new one).
2. The core that makes the change re-encrypts the stored passphrase at once. If it cannot, it removes the file and
   logs why, and the next start leaves the wallet locked: set autounlock up again.
3. Check after the next restart that `unlock_scope` is `staking_only` (see
   [*Verifying it works (Windows)*](#verifying-it-works-windows)).

**Turning autounlock off:** **Settings → Autounlock for staking...** offers to turn it off, or run
`gridcoinresearchd.exe -stdinwalletpassphrase clearautounlock`. Either deletes the stored passphrase and locks the
wallet. The deletion is an ordinary one: the encrypted bytes can survive in free space, backups and shadow copies.

**On a suspected compromise** of the wallet, or of a machine that held it or a backup of it, changing the
passphrase is not enough: the keys stay the same, and every earlier copy of `wallet.dat`, backups included, still
opens with the old passphrase. Move the coins to a new wallet:

1. Stop the core and move its `wallet.dat` aside (keep it for now). Start the core: it creates a new wallet.
   Encrypt it with a new passphrase (`gridcoinresearchd.exe -stdinwalletpassphrase encryptwallet`; the core stops
   afterwards, so start it again), take an address from it (`getnewaddress`), and back it up.
2. Stop the core, put the old `wallet.dat` back, start it, and send the whole balance to that address.
3. Stop the core, put the new `wallet.dat` back, start it, and set autounlock up again.
4. Delete the old wallet, every backup of it (the `walletbackups\` folder in the data directory, and copies on other
   disks, machines and cloud storage), and the automatic backups of the new wallet from before its encryption, in
   the order of [*Purging pre-encryption backups*](#purging-pre-encryption-backups).

**A stray `.part` file.** A GUI that is closed by force while it receives a `dumpwallet` export can leave
`<destination>.<random>.part` beside the destination. Only your account can read it, but it holds the private keys
in plain text: delete it.

### Multiple wallets, and mainnet and testnet side by side (Windows)

- **One core, one wallet.** For each network, a wallet runs either in the service or in a same-account core, never
  both: `enable` disables the Core-Start task it replaces, and `Install-GridcoinCoreTask.ps1` refuses while the
  service is enabled.
- **Mainnet and testnet run side by side.** Their services, accounts, groups, directories and ports differ, so the
  two services, or one service and the same-account core, run together. One installation registers one pair of
  same-account tasks.
- **Both networks in one data directory.** Testnet's data directory is `%APPDATA%\GridcoinResearch\testnet`, so a
  mainnet `enable` leaves `testnet\` in place, and a testnet `enable --network=test` moves only `testnet\`.

### Upgrading (Windows)

Run the new installer as an administrator. It stops the running cores (Core-Stop for a task, `Stop-Service` for the
service), installs, replaces the earlier tasks as above, and starts the cores it stopped. It refuses to
uninstall while the service mode is on: run `gridcoin-service.exe disable` for each network first. On the finish page,
Run starts `gridcoinresearch.exe -multiprocess` (with the `-datadir` of the launcher, if it names one) when the
installing account's own Core-Start was restarted, so the GUI attaches to that core; it is not ticked when a service or
another account's core was restarted (the notes then say how to attach a GUI); and it starts the plain GUI when nothing
was restarted.

From an earlier version, upgrade with the installer: the `Update-GridcoinCore.ps1 -NewBinariesDir` of earlier
versions copies only the programs and does not replace the earlier tasks.

On an upgrade the installer also checks the files of the default data directory (and of its `testnet\`) for
permissions wider than this release sets for new files, and lists what it finds in `UPGRADE-NOTICE.txt` in the
`windows\` folder, which the finish page offers to open. It changes nothing: a file the core rewrites keeps the
permissions it has. `gridcoinresearchd.exe -reportpermissions` runs the same check at any time.

To upgrade from a console, with the installer and the `SHA256SUMS` of the release, from an elevated PowerShell in the
`windows\` folder:

```powershell
.\Update-GridcoinCore.ps1 -Installer .\gridcoin-5.5.2.1-win64-setup.exe -Sums .\SHA256SUMS
```

It verifies before it touches anything: the signer file and the script must be writable by privileged accounts only,
the installer is copied to a protected staging directory, its Authenticode signature must be valid and from the
release signer pinned in `release-signer.txt` (a chain to Microsoft's code-signing root, the pinned identity marker
and subject name, and a timestamp), the installer must be listed in `SHA256SUMS` with a matching SHA-256, and its
version must be newer than the installed one (`-AllowDowngrade` overrides). Only then does it run the staged copy of
the installer silently (the installer first waits up to 60 seconds for its program files to be free and refuses, changing nothing, if one stays open; a file it cannot replace never gets skipped), requires the installed `gridcoinresearchd.exe` to report the new version, then prints the installer's `UPGRADE-NOTICE.txt` (removed tasks, autounlock not carried over, wide permissions found) when the run wrote one. With no signer pinned it refuses.
For a download you install by hand, `.\Test-GridcoinRelease.ps1 -InstallerPath .\gridcoin-5.5.2.1-win64-setup.exe
-SumsPath .\SHA256SUMS` checks the signature and prints the SHA-256 that `SHA256SUMS` lists for the file; compare it
with `(Get-FileHash .\gridcoin-5.5.2.1-win64-setup.exe).Hash`.

### Verifying it works (Windows)

- **The wallet is unlocked for staking only.** As alice, in a normal console:

  ```powershell
  & "C:\Program Files\GridcoinResearch\daemon\gridcoinresearchd.exe" getwalletinfo
  ```

  shows `"unlock_scope": "staking_only"`. For the service, in an elevated console, add
  `-conf="$env:ProgramData\Gridcoin\conf\mainnet.conf" -datadir="$env:ProgramData\Gridcoin\mainnet"` before
  `getwalletinfo`. `locked` means autounlock did not happen; `full` means someone unlocked the wallet fully since.
  An unencrypted wallet always reads `full`.
- **The core unlocked.** Its `debug.log` has `autounlock: wallet unlocked for staking`.
- **The service is laid out as installed.** `gridcoin-service.exe status` checks every directory and file of the
  layout against its access list.

### Troubleshooting (Windows)

- **The GUI says it could not connect to the daemon.** The core is not running, was not started with
  `-multiprocess`, uses another data directory, or runs as another account. For the service: has alice signed in
  again since `enable`, and does `Start-GridcoinGui.ps1 -Service` name the right network? See
  [`multiprocess.md` → *Troubleshooting*](multiprocess.md#troubleshooting).
- **`Install-GridcoinCoreTask.ps1` refuses with "Task mode needs your own account to be an administrator...".** Run
  it from an elevated console of the wallet's account; for a standard-user wallet, ask an administrator for the
  service mode.
- **The first start after an upgrade exits.** The first multiprocess-capable start rewrites `wallet.dat` once and
  may exit. If `Get-Process gridcoinresearchd` shows nothing just after, start Core-Start once more.
- **`server=0` in the configuration file** turns the RPC listener off, and with it Core-Stop, which stops the core
  over RPC: the only way left to stop the core is a hard kill with no flush. Leave the listener on; it binds to
  loopback only unless you add `rpcallowip`.
