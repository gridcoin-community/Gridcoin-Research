# Regression test mode (`-regtest`)

Regtest is a private, instant-staking chain mode for development and automated
testing. Unlike mainnet and testnet it has no peers, no real BOINC/research
component, and a trivial difficulty target, so you can create blocks on demand
and exercise wallet, mempool, P2P, and consensus code in a deterministic
sandbox. It is the chain mode the Python functional tests run against (see
[`test/functional/README.md`](../test/functional/README.md)).

> Regtest is enabled only on builds where the regtest chain params are compiled
> in. It is for development and testing — never point a regtest node at the real
> network.

## Starting a node

```bash
gridcoinresearchd -regtest -daemon
gridcoinresearchd -regtest getblockchaininfo   # chain == "regtest"
gridcoinresearchd -regtest stop
```

The datadir is isolated under `<datadir>/regtest/`. The daemon doubles as the
CLI (`gridcoinresearchd -regtest <command>`); there is no separate `gridcoin-cli`.

## What's different from mainnet/testnet

- **Mockable chain.** `IsMockableChain()` is true, which relaxes timing for
  tests: coinbase/coinstake maturity and `nStakeMinAge` are gated to 0, and the
  scraper threads and automatic superblock attachment are disabled.
- **Proof-of-stake on demand.** There is no proof-of-work. Mint blocks with
  `generatetoaddress <nblocks> <address>`; every block is a PoS block. The
  background staker can be turned off with `-staking=0` for fully deterministic
  height control.
- **Genesis premine.** The genesis coinbase pays a deterministic premine
  (10 × 100,000 GRC) to a known key that the daemon plants into the wallet, so a
  fresh node has spendable coins immediately. `listunspent` shows the premine
  UTXOs (note: `getbalance` reports 0 for the raw premine — it is immature for
  balance accounting even though it is spendable).
- **Default ports.** P2P `32747`, RPC `35715` (mainnet uses `32749`/`15715`,
  testnet `32748`/`25715`). The functional framework allocates per-node ports
  dynamically when running multiple instances.
- **Investor-mode only (today).** Researcher rewards (beacon/CPID claims, MRC
  payouts, superblock magnitude) require a regtest beacon + RSA trust anchor
  that are not yet wired up; staked blocks are non-cruncher (investor) claims.

## Local sidestaking

```bash
gridcoinresearchd -regtest -enablesidestaking=1 -sidestake=<address>,<percent>
```

Each staked block's reward is split to the configured destination(s);
`listsidestakes` shows the active entries.

## Notes

- Block data (LevelDB block index + chainstate) is written under
  `<datadir>/regtest/`; remove it to start from a fresh genesis.
- For programmatic use, the Python functional-test framework wraps all of the
  above (node lifecycle, RPC, P2P, premine helpers) — start there rather than
  scripting the CLI by hand.
