Please be consistent with the existing coding style.

Block style:

bool Function(char* psz, int n)
{
    // Comment summarising what this section of code does
    for (int i = 0; i < n; i++)
    {
        // When something fails, return early
        if (!Something())
            return false;
        ...
    }

    // Success return is usually at the end
    return true;
}

- ANSI/Allman block style
- 4 space indenting, no tabs
- No extra spaces inside parenthesis; please don't do ( this )
- No space after function names, one space after if, for and while

Variable names follow Bitcoin Core standards:
- **Local variables**: lower_snake_case (e.g., `block_height`, `transaction_fee`)
- **Member variables**: m_ prefix with lower_snake_case (e.g., `m_block_index`, `m_wallet_balance`)
- **Global constants**: UPPER_SNAKE_CASE (e.g., `MAX_BLOCK_SIZE`)

Legacy code may use Hungarian notation (type prefixes like nSomeVariable), but this is being phased out. When editing legacy code:
- Judgment is allowed for consistency within that module
- Qt GUI code may use camelCase (localVariable style) to match Qt conventions
- New code should use the modern lower_snake_case standard

-------------------------
Locking/mutex usage notes

The code is multi-threaded, and uses mutexes and locking macros to protect data structures.

**Primary Locking Macros**:
- `LOCK(mutex)`: Acquires a single mutex lock (RAII-style, releases on scope exit)
- `LOCK2(mutex1, mutex2)`: Acquires two mutexes in order to prevent deadlocks

**Common Mutexes**:
- `cs_main`: Blockchain state (most critical, acquire first in lock order)
- `cs_wallet`: Wallet operations
- `pwalletMain->cs_wallet`: Wallet instance lock

**Lock Order**: Generally acquire `cs_main` before `cs_wallet` to prevent deadlocks. The `-DENABLE_DEBUG_LOCKORDER=ON` CMake option detects lock order violations.

**Legacy**: `CRITICAL_BLOCK`/`TRY_CRITICAL_BLOCK` are deprecated and being replaced with modern `LOCK` macros.

Deadlocks due to inconsistent lock ordering (thread 1 locks cs_main
and then cs_wallet, while thread 2 locks them in the opposite order:
result, deadlock as each waits for the other to release its lock) are
a problem. Configure with -DENABLE_DEBUG_LOCKORDER=ON to get lock order
inconsistencies reported in the debug.log file.

Re-architecting the core code so there are better-defined interfaces
between the various components is a goal, with any necessary locking
done by the components (e.g. see the self-contained CKeyStore class
and its cs_KeyStore lock for example).

-------
Threads

ThreadAppInit2: Initializes Gridcoin.                       (grc-appinit2)

    ThreadFlushWalletDB: Close the wallet.dat file if it hasn't been used (grc-wallet)
                         in 500ms.

    StartNode          : Starts other network threads.                    (grc-start)

        ThreadGetMyExternalIP     : Determines outside-the-firewall IP address,            (grc-ext-ip)
                                    sends addr message to connected peers when
                                    it determines it.

        ThreadDNSAddressSeed      : Loads addresses of peers from the DNS.                 (grc-dnsseed)

        ThreadMapPort             : Universal plug-and-play startup/shutdown.              (grc-UPnP)

        ThreadSocketHandler       : Sends/Receives data from peers on port 8333.           (grc-net)

        ThreadOpenAddedConnections: Opens network connections to added nodes.              (grc-opencon)

        ThreadOpenConnections     : Initiates new connections to peers.                    (grc-opencon)

        ThreadMessageHandler      : Higher-level message handling (sending and receiving). (grc-msghand)

        ThreadDumpAddress         : Saves peers to peers.dat                               (grc-adrdump)

        ThreadStakeMiner          : Generates Gridcoins.                                   (grc-stake-miner)

        ThreadScraper             : Pulls statistics from project servers.
                                    Mutually exclusive with ScraperSubscriber

        ScraperSubscriber         : Generates superblocks.
                                    Mutually exclusive with ThreadScraper

    ThreadRPCServer    : Remote procedure call handler, listens on port 8332
                          for connections and services them.

        ThreadTopUpKeyPool          : Replenishes the keystore's keypool.     (grc-key-top)

        ThreadCleanWalletPassphrase : Re-locks an encrypted wallet after user (grc-lock-wa)
                                      has unlocked it for a period of time.

    CScheduler         : Schedules tasks.

ipcThread     : Scans to check if a URI (gridcoin:) is used. (grc-gui-ipc)

Shutdown      : Does an orderly shutdown of everything.     (grc-shutoff)

----------------
Snapshot Threads

SnapshotDownloadThread: Downloads the snapshot from gridcoin.us. (grc-snapshotdl)

SnapshotExtractThread : Extracts the downloaded snapshot.        (grc-snapshotex)
