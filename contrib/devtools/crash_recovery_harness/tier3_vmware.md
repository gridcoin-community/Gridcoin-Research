# Tier 3 — VMware abrupt power-off (manual procedure)

The most faithful reproduction of Scenario C from issue #2865 available
without dedicated hardware: from the host, abruptly terminate the
VMware guest process so the guest's OS page cache is dropped instantly,
matching what real power loss looks like to the wallet inside.

This is a manual procedure, not a scripted harness. The VMware
lifecycle is host-specific and not worth automating until we want CI
integration on a VMware-capable runner.

## Prerequisites

- VMware Workstation (or VMware Fusion on macOS) on the host.
- A guest VM with a build environment for the Gridcoin wallet, network
  access to a peer node (the host's isolated testnet via host-only
  networking is the cleanest setup), and a clean snapshot to revert to
  between runs.
- The host's `vmrun` CLI (ships with VMware Workstation; usually at
  `/usr/bin/vmrun` on Linux, `C:\Program Files (x86)\VMware\VMware Workstation\vmrun.exe`
  on Windows, `/Applications/VMware Fusion.app/Contents/Public/vmrun`
  on macOS Fusion).

## Procedure

For each test run:

1. **Snapshot revert.** From the host:

   ```bash
   vmrun -T ws revertToSnapshot /path/to/guest.vmx "clean-pre-test"
   vmrun -T ws start /path/to/guest.vmx
   ```

   The snapshot should be of the guest at a known-good state: wallet
   installed, peer configured, datadir empty (so the next start does
   a full IBD).

2. **Start IBD inside the guest.** SSH or console into the guest, start
   the wallet:

   ```bash
   gridcoinresearchd -testnet -daemon -addnode=<peer-host-ip>:<port>
   ```

   Watch progress with `gridcoinresearchd getblockchaininfo` from inside
   the guest until the chain height passes a configurable target -- a
   reasonable choice is ~50% of the peer's tip, so the IBD is well
   underway but not yet finished.

3. **Abruptly drop the guest's page cache.** From the host (NOT from
   inside the guest):

   ```bash
   vmrun -T ws stop /path/to/guest.vmx hard
   ```

   The `hard` modifier is the key -- it is equivalent to pulling the
   plug on the VM. The guest's OS gets no chance to flush its page
   cache. Soft stop (without `hard`) would do a graceful shutdown,
   which defeats the test.

4. **Reboot and observe.** Start the VM again:

   ```bash
   vmrun -T ws start /path/to/guest.vmx
   ```

   SSH in once it's up. Restart the wallet:

   ```bash
   gridcoinresearchd -testnet -daemon
   ```

   The Phase 2 recovery hook runs as part of startup. Check the debug.log:

   ```bash
   grep "RunStartupCoherenceRecovery" ~/.GridcoinResearch/testnet/debug.log
   ```

## What to expect

In the **common case** the abrupt stop landed cleanly enough that the
on-disk state is coherent:

```
INFO: RunStartupCoherenceRecovery: chain tip at height N is coherent. No rewind needed.
```

In the **Scenario C case** the abrupt stop interrupted between the
LevelDB index commit and the blk*.dat fsync:

```
INFO: RunStartupCoherenceRecovery: detected inconsistency past height N.
INFO: RunStartupCoherenceRecovery: last consistent block is at height M (K superblocks crossed in the abandoned range).
INFO: AbandonChainTo: abandoning chain tip <hash> @ N down to <hash> @ M.
INFO: RunStartupCoherenceRecovery: rewind crossed K superblock(s); resetting beacon registry for full replay.  (if K > 0)
INFO: RunStartupCoherenceRecovery: registry bookmarks reconciled to height M. Phase 2 recovery complete; P2P sync will re-supply blocks past the rewound tip.
```

Followed by normal `AcceptBlock` activity as P2P re-supplies the
missing blocks.

## Pass criteria

After the restart, all of the following:

1. Wallet boots without manual intervention (no `-reindex` needed).
2. `RunStartupCoherenceRecovery` INFO lines present.
3. No `LevelDB Sync failure` / `FileCommit failed` lines in the
   post-restart debug.log.
4. `getblockchaininfo` reaches the peer's tip within a reasonable
   timeout (depends on how far behind the recovered tip is).
5. If a beacon rebuild fired, `listbeacons` row count matches a
   control node's within reorg-fork tolerance.

If any of these fail, capture the full debug.log and the
`/dev/sd*` block-device state from the guest and file an issue against
#2865.

## Reproducibility tips

- **Vary the abrupt-stop timing.** Try stopping at different IBD
  heights and at different times relative to the every-5000-blocks
  fsync boundary Phase 1 enforces during IBD. The Scenario C window is
  smallest right after a boundary and largest right before the next.
- **Use a slow guest disk.** A guest with a thin-provisioned or
  rate-limited disk extends the writeback window between page-cache and
  durable storage, increasing the chance of catching Scenario C.
- **Test the multi-SB-cross beacon rebuild path** by abrupt-stopping
  during a window that spans multiple superblock heights, then
  inspecting `listbeacons` after recovery to confirm parity with a
  control node.

## Why not automate this

Three reasons:

1. **VMware lifecycle is host-specific.** Different sites have
   different `vmx` paths, snapshot names, networking topologies, and
   credentials. A generic script would either hard-code one site or
   wrap so many `${VAR:-default}` knobs it becomes harder to read than
   the manual procedure.
2. **The interesting failure modes are timing-dependent.** Catching
   Scenario C reliably requires varying the abrupt-stop point against
   the fsync cadence, which is closer to exploratory testing than to a
   CI gate.
3. **We don't currently have a VMware-capable CI runner.** When that
   changes, this document is the spec for what to automate.

The Tier 1 and Tier 2 scripts cover the automation we *can* do today on
a Linux developer host.
