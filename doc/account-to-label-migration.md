# Migrating from accounts to labels

Starting with the release that ships the wallet label/account split, the legacy
**"accounts" subsystem is deprecated** and will be removed in a future release
(tracking: issue #3086). This guide explains what changed and how to move your
scripts, tools, and habits over to **labels**.

## TL;DR

- Address **labels** replace the address-naming half of accounts. Use `setlabel`,
  `getaddressesbylabel`, and `listlabels`.
- The account **balance ledger** (`move`, `sendfrom`, `getbalance "<account>"`) has
  **no label equivalent** — labels are address tags, not balance buckets. See
  [The account balance ledger is going away](#the-account-balance-ledger-is-going-away).
- Run **`migratelabels`** once to tidy existing address-book entries.
- Two temporary opt-in flags keep the deprecated RPCs working during the transition:
  `-enableaccounts=1` and (for the accounting RPCs) `-staking=0`.

## What changed under the hood

Previously a single string served *both* as an address's label *and* as its "account"
grouping key. That string is now split into a first-class record with a **`name`**
(the label) and a **`purpose`** (`receive`/`send`/`unknown`), so a label survives
independently of the account system being retired. This is wallet-local and does not
affect consensus, balances, or keys. Old wallets load unchanged (their labels appear
with `purpose` defaulting to `unknown`), and the change is downgrade-safe.

## RPC migration reference

| Deprecated account RPC | Behavior now | Replacement |
| --- | --- | --- |
| `setaccount <addr> <name>` | works, logs a one-time deprecation warning | `setlabel <addr> <label>` |
| `getaccount <addr>` | works, logs a warning | `validateaddress <addr>` — the address's label is returned in the `account` field |
| `getaddressesbyaccount <name>` | works, logs a warning | `getaddressesbylabel <label>` |
| `getaccountaddress <name>` | **throws** unless `-enableaccounts=1` | `getnewaddress` + `setlabel` |
| `listaccounts` | **throws** unless `-enableaccounts=1` **and** `-staking=0` | `listlabels` |
| `move`, `sendfrom`, `getbalance "<account>"` | **throws** unless `-enableaccounts=1` **and** `-staking=0` | no direct equivalent — see below |

New label RPCs:

- **`setlabel "<address>" "<label>"`** — set the label for an address. The address's
  purpose is derived from ownership: `receive` for wallet-owned addresses, `send` for
  external ones.
- **`getaddressesbylabel "<label>"`** — list the addresses carrying a label (with each
  address's purpose).
- **`listlabels ( "purpose" )`** — list all labels, optionally filtered to
  `receive` or `send`.

### Examples

```
# Old:
setaccount SXXXXXXXXXXXXXXXXXXXXXXXXXXXX "cold storage"
getaddressesbyaccount "cold storage"
listaccounts

# New:
setlabel SXXXXXXXXXXXXXXXXXXXXXXXXXXXX "cold storage"
getaddressesbylabel "cold storage"
listlabels
```

There is no longer an implicit "one receiving address per account." Where you used
`getaccountaddress "<name>"`, create the address explicitly and label it:

```
# New: create the address, then label it
getnewaddress                        # -> returns <address>
setlabel "<address>" "cold storage"
```

(Examples show bare RPC method names, as typed in the debug console or passed to
`gridcoinresearch`/`gridcoinresearchd`.)

## The account balance ledger is going away

This is the substantive change, not the RPC renames. Accounts also acted as a
**virtual, in-wallet balance ledger**: you could `move` funds between accounts,
`sendfrom` a named account, and query `getbalance "<account>"`. **Labels do not do
this** — a label is only a tag attached to an address; it does not track a per-label
balance, and money is never "in" a label.

If you relied on accounts to segregate balances, use one of:

- **Separate wallets** — the cleanest one-balance-per-purpose model.
- **Coin control** — track UTXOs directly with `listunspent`, pin them with
  `lockunspent`, and build spends with `createrawtransaction` / `fundrawtransaction`.
- **External accounting** — track deposits/attribution in your own system keyed by
  address (labels remain a fine human-readable tag for that).

Whole-wallet balance queries are unaffected: `getbalance` with no account argument
continues to report the wallet total.

## The two temporary opt-in flags

During the deprecation window you can keep the throwing RPCs working. (The warn-only
RPCs — `setaccount`, `getaccount`, `getaddressesbyaccount` — need no flag; they still
work and only log a deprecation warning.)

- **`-enableaccounts=1`** — required by every deprecated RPC that throws: on its own it
  re-enables `getaccountaddress`, and it is also the first requirement for the
  accounting RPCs (`move`, `sendfrom`, `listaccounts`, `getbalance "<account>"`).
- **`-staking=0`** — *additionally* required by those accounting RPCs, because they
  touch the legacy double-entry balance ledger. This is the pre-existing accounting
  guard, unchanged by this release. The reason is that **staking mutates the wallet's
  coins outside the account bookkeeping**: a coinstake consumes UTXOs (possibly ones
  attributed to an account) and credits the stake return plus research reward with no
  account attribution, so per-account balances drift from reality — the "negative or
  odd balances" the accounting API warns about. Disabling staking removes that source
  of corruption while the deprecated ledger is in use.

So the accounting RPCs need **both** `-enableaccounts=1` **and** `-staking=0`;
`getaccountaddress` needs only `-enableaccounts=1`. Both flags are **temporary**. Treat any run that needs them as a to-migrate item — the
accounts subsystem may be removed entirely in a future release.

## One-time cleanup: `migratelabels`

Address-book entries that predate the split load with `purpose` = `unknown`. Run:

```
gridcoinresearchd migratelabels
```

once to backfill each entry's purpose (`receive` for owned addresses, `send`
otherwise). It reports how many entries were updated and is **idempotent** — running
it again updates nothing. It only fills in missing purposes; existing labels and
already-set purposes are left untouched.

## See also

- Accounts retirement tracking: issue #3086.
