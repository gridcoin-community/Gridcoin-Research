# BlockRewardRules Design Notes

## Motivation

PR #2848 revealed a class of bug where the miner and validator independently
computed the eligible mandatory sidestake set with slightly different logic.
The staker's address matching a mandatory sidestake destination caused the
validator to miscount stake-split outputs as mandatory sidestakes. The root
cause: two separate implementations of the same decision, free to drift apart.

BlockRewardRules (#2880) addresses this by extracting a **shared spec
computation** that both miner and validator consume, making this class of
drift bug structurally impossible for mandatory sidestakes.

## Design Principles

### Shared spec, separate operations

The construct (miner) and validate paths are inherently asymmetric — they are
not two implementations of the same algorithm but two *different algorithms*
that consume the same input:

- **Construct** mutates a `CMutableTransaction`, shuffles when over the output
  limit, and appends outputs. It does not verify anything.
- **Validate** iterates existing outputs, matches them against the spec with
  double-counting prevention, and checks amounts. It does not create anything.

A "construct then compare" unification would break down because of the
non-deterministic shuffle and because voluntary sidestakes interleave with
mandatory ones in the actual coinstake. The right boundary for sharing is the
**eligible set and amount computation** (the "spec"), not the operations that
consume it.

### Validation-only logic stays validation-only

The `Check()` path (which replaced the former `ClaimValidator` class) is pure validation with no miner
counterpart. MRC output matching, beacon signature checking, reward envelope
checks, legacy fallbacks — none of these have a construction dual. Forcing them
into a unified framework would be over-abstraction for no structural benefit.

### Incremental, not big-bang

The refactoring is deliberately incremental. The consensus validation code is
battle-tested and high-consequence — a regression here means chain splits.
Each step should:

1. Extract a named, testable unit (spec computation, helper function)
2. Add unit tests *before* or *alongside* the extraction
3. Keep the existing validation semantics identical
4. Gate any actual consensus changes behind block version checks

### Comment preservation

The original `ClaimValidator` class (now removed) contained extensive inline comments explaining
*why* validation steps exist — historical context (beaconalt bug, newbie
snapshot fix), structural invariants (mrc_start_index arithmetic, dust
elimination ordering relative to shuffle), and subtle distinctions
(m_mrc_tx_map.size() vs mrc_non_zero_outputs). These comments represent
hard-won domain knowledge and must be carried forward, not condensed away.

## What does NOT fit the shared-spec pattern

- **MRC binding** (mempool selection in `CreateNewBlock`): purely a miner-side
  concern — selecting which MRC transactions to include based on priority,
  CPID uniqueness, and output limits. The validator checks the resulting
  outputs but has no role in the selection. No validator dual, no drift risk.

## Future Directions

Areas where the shared-spec pattern could be extended (each as its own
incremental step, motivated by concrete need):

- **Foundation MRC sidestake output**: the fee split output to the foundation
  address is computed independently in `CreateMRCRewards` (miner) and
  `CheckReward` (validator). Could become a shared spec entry.
- **MRC output layout**: the index arithmetic for mrc_start_index is replicated
  in both paths. A shared layout descriptor could eliminate this.
- **Voluntary sidestake construction**: lower priority since voluntary
  sidestakes are not consensus-enforced, but the dust elimination logic is
  similar enough to be a candidate.

Each extension should be motivated by a concrete bug, a version-gated consensus
change that forces the code to be touched, or a clear testability improvement —
not by architectural aesthetics alone.
