# Windowed Transaction Table — Testing

**Status:** as-built at the PR5-C tip — branch `testnet_v13_checkpoint_removed`, commit `900889be6` (*qt: retire the lazy render-thread lock path; producer-side tx detail (windowed-model PR5-C)*). Companion to `doc/windowed-transaction-table-architecture.md`; the on-branch design of record is `doc/transaction_table_windowed_model.md`. Every claim below is anchored to a file and line you can open and read on this branch.

The stack replaced the GUI's old client-side `TransactionFilterProxy` + full `QList<TransactionRecord>` replica with a producer/consumer windowed model: an authoritative off-`cs_main` `GRC::WalletTxStore` (producer) feeds per-view Qt-free cursors (server-side filter/sort) and viewport-windowed thin consumer models over an MPSC `WalletEventQueue`. Lock order is `cs_main -> cs_wallet -> cs_store` (leaf), `cs_store` never held while taking the wallet locks (`src/qt/wallettxstore.h:91-96`).

---

## 1. Test philosophy

The stack is multithreaded GUI code, and the single hardest constraint on its test strategy is a structural fact about CI: **the Sanitizers (ASan/UBSan) job builds with `ENABLE_GUI=OFF`** (`.github/workflows/cmake_quality.yml:66`). Under `ENABLE_GUI=OFF`, `src/qt` is never added to the build (`src/CMakeLists.txt:376-378` gates `add_subdirectory(qt)` behind `ENABLE_GUI`), so `TransactionRecord`, the Qt models, and the whole event-wiring layer are never compiled, never run, and never seen by a sanitizer in that job. That GUI-OFF blind spot is exactly what let PR #2944's heap corruption escape CI. The entire test philosophy is built to shrink that blind spot.

The result is a deliberate three-tier split:

1. **Qt-free cores → GUI-OFF Boost suites.** The highest-risk arithmetic — ordering, filter/sort predicate, per-view `view_index` maintenance, and consumer-side window reconciliation — was extracted into Qt-free translation units (`src/qt/txorder.{h,cpp}`, `src/qt/txfilter.{h,cpp}`, `src/qt/cursor.{h,cpp}`, `src/qt/windowcache.h`). These compile straight into the main `test_gridcoin` Boost binary (added to the target at `src/test/CMakeLists.txt:50-69`; the binary itself is built whenever `ENABLE_TESTS=ON` regardless of GUI, the gating being at `src/CMakeLists.txt:384-385`). Consequently this logic runs in *every* configuration CI exercises, including the Clang ASan/UBSan job. The suites are fixtureless — they drive the cores with synthetic record/table/sink harnesses, never `TestChain100Setup` (verified: none of `src/test/qt_*tests.cpp` reference a chain fixture; they only inherit the binary-wide `BOOST_GLOBAL_FIXTURE(TestingSetup)` at `src/test/test_gridcoin.cpp:88`).

2. **Producer worker + MPSC queue → Qt/QTest suites with a nullptr-wallet harness.** The event queue and the double-queue store-worker are concurrency primitives that depend on Qt (`QTest` loop pumping, `QObject`) and on `TransactionRecord`, so they can only live in the GUI test binary `test_gridcoin-qt`, built when `ENABLE_GUI=ON` + `ENABLE_TESTS=ON` (`src/qt/test/CMakeLists.txt:1-7`, reached via `src/qt/CMakeLists.txt:464-465`). The worker's insert/remove/drain path is wallet-free by design, so these tests construct a `WalletTxStore` with a **null `CWallet*`** and exercise the full ordering/MPSC/shutdown behaviour without a wallet (`src/qt/test/wallettxstore_tests.cpp:59`).

3. **GUI behaviours → mesh soak + ARM real-world.** The model shells (`DetailedTxModel`, `OverviewTxModel`), the Qt event wiring, and the wallet-touching producer paths (`reloadAndSnapshot`, the `getRowDetail` `toHTML` path) cannot be unit-tested GUI-OFF. They carry a mandatory ASan/UBSan-GUI + isolated-testnet soak burden per model-touching PR, validated against pathological wallets with `-DENABLE_DEBUG_LOCKORDER`, plus a 121,979-tx 32-bit ARMv7 real-world validation.

---

## 2. GUI-off unit test inventory

All four suites are Qt-free Boost suites compiled into `test_gridcoin` and registered to ctest as `gridcoin_tests` (`src/test/CMakeLists.txt:123`).

### 2.1 `qt_txorder_tests` — the ordering core
- **File:** `src/test/qt_txorder_tests.cpp` (suite macro at `:32`); links `src/qt/txorder.cpp` (`src/test/CMakeLists.txt:59-60`).
- **Asserts:** the `(time DESC, hash ASC, idx ASC)` strict-total order that the producer relies on to position-stamp `RowsInserted`/`RowsRemoved`, plus the `lower_bound` insert slot and the same-hash contiguity that the remove path depends on. All cases use a local `TxOrderKey K()` helper (`:26`).
- **6 cases:** `txorder_time_descending_primary` (`:36`, newer time sorts first), `txorder_hash_ascending_tiebreak` (`:45`), `txorder_idx_ascending_tiebreak` (`:54`), `txorder_strict_weak_equal_is_false` (`:63`, `TxOrderLess(a,a)==false`), `txorder_lower_bound_insert_positions` (`:72`, front/between/end slots mirroring `WalletTxStore::insertTransaction`), `txorder_same_hash_records_cluster_contiguously` (`:95`, one tx's decomposed parts occupy a single `[min,max]` run).

### 2.2 `qt_txfilter_tests` — the filter/sort predicate core
- **File:** `src/test/qt_txfilter_tests.cpp` (suite at `:36`); links `src/qt/txfilter.cpp` (`src/test/CMakeLists.txt:54-55`).
- **Asserts:** the `Accepts` / `Less` / `CompareKeys` logic that was the `TransactionFilterProxy::filterAcceptsRow` predicate + the `Qt::EditRole` sort keys. All rows built from `DefaultPassingRow()` (`:23`) so each case flips one dimension.
- **12 cases:** `accepts_default_spec_passes` (`:40`); `accepts_address_substr_matches_address_or_label` (`:48`, the label arm is the PR4 producer-snapshotted label, case-insensitive, reject on neither); `accepts_inactive_gating` (`:68`, the two-gate `show_inactive` AND `show_orphans` original logic — inactive masked unless both open, active never masked); `accepts_type_mask` (`:110`, loops types 0..11, `1u<<t` accepts / `~(1u<<t)` rejects / default `ALL_TYPES` accepts); `accepts_date_range_boundaries` (`:129`, inclusive both ends); `accepts_address_or_label_substring` (`:148`, case-insensitive substring not prefix, empty matches all); `accepts_min_amount_uses_abs` (`:179`, `llabs(net_amount) >=`, boundary equal passes); `accepts_combined_predicates` (`:207`, AND of type/date/min/substr, each flip rejects); `less_numeric_columns` (`:233`, Date/Amount asc+desc, equal→false); `less_string_columns_case_insensitive` (`:261`, Status/Type/Address case-insensitive, equal→false both orders); `compare_keys_three_way_and_tie` (`:293`, 3-way sign, DESC flips sign, equal keys→0 — the tie sentinel the cursor breaks by native index, PR4-fix E); `address_sorts_label_then_address` (`:308`, two-level label-then-address with a split-literal `\x01` control byte that cannot cross the field boundary, PR4-fix G).

### 2.3 `qt_cursor_tests` — the per-view `view_index` maintenance core
- **File:** `src/test/qt_cursor_tests.cpp` (suite at `:83`); links `src/qt/cursor.cpp` (`src/test/CMakeLists.txt:63-64`).
- **Harness:** synthetic `Rec` (`:31-39`) + `Table` (`:50-72`) whose `FieldsFn`/`KeysFn` return a `const&` **recomputed on each call** from the live row — deliberately mirroring the production store's reference-returning projector cache (PR4-fix F) and being reentrant-safe within one comparison.
- **15 cases:** `rebuild_filters_inactive_and_sorts_date_desc` (`:87`); `store_insert_shifts_absidx_and_places_in_sort_order` (`:108`, Item 4: shift `+count` then slot by sort key); `store_remove_erases_by_identity_and_shifts` (`:132`); `status_update_flip_out_then_in` (`:153`, membership flip-out then flip-in re-slots); `identity_locate_with_equal_sort_keys` (`:179`, **Item 1** — locate the exact row by absolute identity, not sort-key `lower_bound`, when keys collide); `reposition_moves_down_no_off_by_one` (`:203`, **Item 2** — insert slot recomputed AFTER the erase); `served_window_eviction_on_insert_when_full` (`:226`, Insert+evict at cap=2); `served_window_promotion_on_remove_with_overflow` (`:250`, Remove+promote at cap); `off_window_update_maintained_without_emission` (`:272`, **Item 3** — off-window move maintains `view_index` but emits zero deltas; then `setLimit` reveals it correctly); `setlimit_grow_then_shrink` (`:302`); `ct_updated_first_confirmation_moves_out_of_window` (`:328`, the live OverviewPage path — a first confirmation drops a row past the served window and promotes another); `equal_keys_tiebreak_by_native_index` (`:356`, PR4-fix E — equal primary keys break by ascending absidx, identical under ASC and DESC); `interleaved_reposition_to_equal_keys` (`:385`, the relabel-one-drive-one interleave that `applyAddressBookChange`/`applyChainTipRefresh` require — guards against the recompute-all-first de-sort bug); `position_of_maps_absidx_to_accepted_row_or_npos` (`:419`, backs PR5 `rowForKey`); `position_of_multipart_same_key_distinct_rows` (`:441`, distinct absidx → distinct rows, MIN for `idx<0`).

### 2.4 `qt_windowcache_tests` — the consumer-side two-channel reconciliation core
- **File:** `src/test/qt_windowcache_tests.cpp` (suite at `:101`). `windowcache.h` is a header-only template, so only the test `.cpp` is added (`src/test/CMakeLists.txt:65-69`).
- **Harness:** synthetic `Rec{id}` (`:35`), `seq()`/`recs()` builders (`:40`, `:49`), and a `RecSink` (`:60`) that probes `cache->total()` at each `begin*` so a test can assert the mutation happens **inside** the begin/end bracket (`probe()` at `:70`); `check_slice()` (`:83`) asserts the cached slice contents and placeholders just outside it.
- **26 cases**, across two channels:
  - **seed:** `seed_sets_state_without_signals` (`:105`, `seedInitial` emits no sink signals).
  - **Structural Insert — all four position regimes:** `insert_before_window_shifts_base_only` (`:124`, also pins the pre-mutation total seen at `beginInsert`), `insert_inside_window_splices` (`:145`), `insert_at_window_top_splices_no_flash` (`:156`), `insert_at_window_end_appends` (`:169`, `pos==base+len`), `insert_after_window_changes_total_only` (`:180`), `insert_empty_or_out_of_range_is_noop` (`:193`, empty/`pos<0`/`pos>total` drop without advancing seqno).
  - **Structural Remove — every overlap regime:** `remove_before_window` (`:209`), `remove_front_overlap` (`:222`), `remove_strictly_inside` (`:233`), `remove_back_overlap` (`:244`), `remove_spans_whole_cache` (`:255`), `remove_after_window` (`:268`), `remove_out_of_range_is_noop` (`:279`).
  - **Structural Change:** `change_refreshes_in_cache_overlap` (`:294`), `change_partial_overlap_clips_to_cache` (`:311`), `change_fully_off_window_no_signal` (`:325`, seqno still advances, no `dataChanged`).
  - **Seqno/epoch gate (PR4-fix B):** `structural_seqno_gate_skips_already_reflected` (`:339`, `seqno <= high-water` skipped both at-equal and below), `reset_skips_stale_and_takes_max_baseline` (`:358`, baseline = `max(high_water, seqno)`, asserts hw 15 wins over seqno 12).
  - **Content channel (never advances seqno/count):** `content_fill_adopts_on_exact_match` (`:381`, asserts total and structuralSeqno unchanged), `content_fill_discards_on_epoch_mismatch` (`:398`), `content_fill_discards_on_seqno_mismatch_either_way` (`:410`, rejects both staler `hw=9` and ahead `hw=11`), `structural_delta_applies_after_a_content_fill` (`:422`), `content_fill_rejects_slice_past_table_end` (`:443`, PR5-A review — `[96,106)` and `first==total` rejected, `[95,100)` accepted), `change_short_fresh_refreshes_what_it_has_and_advances_seqno` (`:462`, PR5-A review — contract-violation safety, no OOB read, seqno still advances).
  - **End-to-end routing:** `consumer_routing_scenario` (`:480`) mirrors `DetailedTxModel::applyEventBatch`'s exact structural/content call sequence against the cache + sink — the reconcilable core of the model shell that is not itself GUI-OFF-testable.

---

## 3. The harnesses

### 3.1 Nullptr-wallet store + async worker-drain poll (`wallettxstore_tests`)
- **File:** `src/qt/test/wallettxstore_tests.cpp`; class declared in `wallettxstore_tests.h:18` (private slots `:23-33`).
- **Construction:** `WalletTxStore store(nullptr, q); store.start();` (`:59`, `:81`, `:102`, …). The worker's insert/remove path never dereferences the wallet, so a null `CWallet*` is sufficient for ordering, MPSC serialization, and shutdown. `getRowDetail`'s early-return paths (unknown hash, wrong idx) also return under `cs_store` before any wallet deref, so they too are null-wallet-safe.
- **Synthetic records:** `hashOf(n)` (`:27-30`, locale-free distinct hashes via `ArithToUint256`, `n+1` avoids the zero hash) and `makeRec(hash,time,idx)` (`:35-40`, carries only the ordering-key fields the store touches).
- **Async drain poll:** `waitForQueue(q, expected)` (`:45-50`) polls `q.size()` via `QTest::qWait(5)` up to 300× (~1.5 s ceiling) because the worker drains asynchronously off the producer thread. It is a poll, not a barrier — generous above observed latency but finite (see §8).
- **6 cases:** `workerDrainsAllInsertsInOrder` (`:54`, 50 distinct inserts → dense seqnos `[0,50)`), `workerHandlesInterleavedInsertRemove` (`:78`, Insert/Insert/Remove dispatched in enqueue order), `workerPreservesAllUnderConcurrentProducers` (`:99`, 4 producers × 100 → 400 unique dense seqnos), `dtorWithPendingIntakeIsClean` (`:138`, flood 500 then destroy → dtor sets `m_stop` + joins without hanging), `getRowDetailUnknownHashReturnsEmpty` (`:154`, PR5-C), `getRowDetailWrongIdxReturnsEmpty` (`:169`, PR5-C).

### 3.2 MPSC queue harness (`wallet_event_queue_tests`)
- **File:** `src/qt/test/wallet_event_queue_tests.cpp` (note: underscored filename; the class is `WalletEventQueueTests`, declared `wallet_event_queue_tests.h:11`).
- Pure `GRC::WalletEventQueue` construction (no wallet, no store). Multi-producer cases (`:105`) spin three `std::thread`s pushing concurrently and assert via a `std::set<uint64_t>` that every seqno appears exactly once.
- **6 cases:** `emptyDrainIsEmpty` (`:20`), `singlePushDrainsOne` (`:30`, seqno 0, `RowsRemovedPayload` variant), `seqnoMonotonicWithinSingleProducer` (`:46`, 100 pushes → dense `[0,N)`), `allPayloadVariantsRoundTrip` (`:62`, `RowsInserted`/`RowsRemoved`/`ChainTipChanged` variant round-trip incl. height/best_time), `drainPartialBatch` (`:82`, `drain(3)` then `drain()` preserves order/seqno across the boundary), `multiProducerSeqnosAreUniqueAndDense` (`:105`, 3 threads × 500 → every seqno `[0,1500)` exactly once — the resync-from-N+1 contract for the multiprocess form).

### 3.3 Qt-free synthetic harnesses (the four Boost cores)
None use a chain or wallet fixture. The cursor suite's `Table`/`Rec` projectors (`qt_cursor_tests.cpp:50-72`) and the windowcache suite's `Rec`/`RecSink`/`check_slice` (`qt_windowcache_tests.cpp:35-97`) are hand-built stand-ins. This is intentional: it is what keeps the cores GUI-OFF-compilable. **No `TestChain100Setup` is used or wanted in these suites.**

### 3.4 Registration
- Qt binary `test_gridcoin-qt`: `src/qt/test/CMakeLists.txt:1-7`, `AUTOMOC ON` (`:9-11`), linked against `gridcoinqt` + `Qt::Test` (`:13-21`), registered as ctest `gridcoin_qt_tests` (`:23`). The runner `test_main.cpp` `QTest::qExec`s in order: `BitcoinUnitsTests`, `URITests`, `WalletEventQueueTests` (test3, `:37`), `WalletTxStoreTests` (test4, `:41`).
- Boost binary `test_gridcoin`: cores + suites added unconditionally under `ENABLE_TESTS` (`src/test/CMakeLists.txt:54-69`), registered as ctest `gridcoin_tests` (`:123`).

---

## 4. CI coverage

The two test binaries are exercised far more widely than a single GUI-ON workflow. The Qt-free cores (`gridcoin_tests`) run in **every** ctest-running job — the GUI-OFF Sanitizers job *and* all GUI-ON jobs. The two concurrency Qt suites (`gridcoin_qt_tests`) run in **~14 GUI-ON jobs across three workflows** (`cmake_compatibility.yml`, `cmake_production.yml`, `cmake_distros.yml`). The one accurate "no coverage" statement is narrow and is called out explicitly below.

| Job | Workflow:lines | Config | What it builds / runs | What it catches for this stack |
|---|---|---|---|---|
| **Sanitizers (Clang)** | `cmake_quality.yml:30-89` | `ENABLE_GUI=OFF`, `ENABLE_TESTS=ON`, `USE_QT6=ON`, `Debug`, `-fsanitize=address,undefined -fno-omit-frame-pointer` (`:66-72`) | `ctest --test-dir build_asan` (`:89`) → runs `gridcoin_tests` incl. the four Qt-free suites | Memory/UB bugs in the ordering, filter, cursor `view_index` arithmetic, and windowcache reconciliation — the highest-risk logic, and **the only sanitizer-instrumented run anywhere in CI**. `UBSAN_OPTIONS=...:halt_on_error=1` (`:85`), so any UB aborts the whole run. **Does NOT build `src/qt`/`src/qt/test`**, so the MPSC queue and store-worker Qt suites are NOT exercised here. |
| **Thread Safety (Clang)** | `cmake_quality.yml:164-208` | `ENABLE_GUI=ON`, `ENABLE_TESTS=OFF`, `WERROR_THREAD_SAFETY=ON`, `Debug` (`:199-203`) | Compile-only: `--target gridcoinresearch gridcoinresearchd` (`:208`). Runs **zero** tests. | Statically enforces the `cs_main -> cs_wallet -> cs_store` lock order via the `GUARDED_BY`/`EXCLUSIVE_LOCKS_REQUIRED`/`NO_THREAD_SAFETY_ANALYSIS` annotations (`src/qt/wallettxstore.h:283-361`). **Clang-only**: under GCC these annotations parse but are silent no-ops, so the GCC jobs below do not enforce the lock order. |
| **Linux Native (System Qt6)** | `cmake_compatibility.yml:31-80` | `ENABLE_GUI=ON`, `ENABLE_TESTS=ON`, `USE_QT6=ON`, `RelWithDebInfo` (`:63-72`; `BUILD_TYPE` env `:19`), GCC | `ctest --test-dir build_native` (`:80`) → `gridcoin_tests` **and** `gridcoin_qt_tests` | A primary functional run of the MPSC queue + store-worker Qt suites (shared-link, GCC, not under sanitizers). |
| **Linux ARM64 (System Qt6)** | `cmake_compatibility.yml:85-182` | cross `aarch64-linux-gnu-g++` (`:165`), `ENABLE_GUI=ON`, `ENABLE_TESTS=ON` (`:168`,`:171`), QEMU emulator (`:172`) | `ctest` under QEMU, `QT_QPA_PLATFORM=offscreen` (`:181-182`) → both suites | Both binaries on the **64-bit cross target**; offscreen Qt under emulation. |
| **Linux ARMhf (System Qt6)** | `cmake_compatibility.yml:187-268` | cross `arm-linux-gnueabihf-g++` (`:251`, `linux/arm/v7`), `ENABLE_GUI=ON`, `ENABLE_TESTS=ON` (`:256`,`:255`), QEMU (`:258`) | `ctest` under QEMU, `QT_QPA_PLATFORM=offscreen` (`:267-268`) → both suites | The **32-bit** target in CI — `int`/`size_t`/pointer-width assumptions in the windowcache base arithmetic and queue seqnos, complementing the real ARMv7 ODROID hand-validation. |
| **Linux Static / Depends** | `cmake_production.yml:28-262` (matrix host `x86_64-pc-linux-gnu`, `STATIC_LIBS=ON` `:42`) | `ENABLE_GUI=ON` (`:126`), `ENABLE_TESTS=ON` (`:130`), `Release`, GCC; **static-Boost link mode** (`BOOST_TEST_STATIC_LINK`, `src/test/CMakeLists.txt:99-101`) | `ctest --test-dir build_linux_depends`, `QT_QPA_PLATFORM=offscreen` (`:190-192`) → both suites | The only **static-link** run of both binaries — exercises static Boost.Test registration, a distinct link mode the shared-link Native job cannot reach. |
| **Windows Cross (wine)** | `cmake_production.yml:28-262` (matrix host `x86_64-w64-mingw32`, `CMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/wine` `:53`) | `ENABLE_GUI=ON` (`:126`), `ENABLE_TESTS=ON` (`:130`), `Release`, mingw-gcc | `ctest` executed **under wine** (`:195-204`), `QT_QPA_PLATFORM=offscreen` (`:198`) → both suites | The only **Windows runtime** in CI — Windows ABI / mingw behaviour for the queue seqnos and windowcache base arithmetic, run under wine. |
| **macOS ARM64 / Intel** | `cmake_production.yml:267-450` / `:452-636` (both `macos-14`; Intel via Rosetta, `CMAKE_OSX_ARCHITECTURES=x86_64` `:541`) | `ENABLE_GUI=ON` (`:356`/`:542`), `ENABLE_TESTS=ON` (`:360`/`:546`), `Release`, **AppleClang** | `ctest --test-dir build` (`:373` / `:559`) → both suites | The only **macOS / AppleClang** runs — a second non-GCC toolchain over both binaries on two architectures. |
| **Distros ×7** | `cmake_distros.yml:18-109` (matrix: Fedora, OpenSUSE Leap 16, OpenSUSE Tumbleweed, Arch, Debian Sid, Linux Mint 22, Alpine) | `build_targets.sh TARGET=native WITH_GUI=true USE_QT6=true` (`:99-108`) → `-DENABLE_TESTS=ON` (`build_targets.sh:339`), `RelWithDebInfo`, GCC | `ctest --test-dir build` (`build_targets.sh:349`) → both suites | Seven distinct GCC/libstdc++/glibc combinations (plus **musl** on Alpine) over both binaries. |
| **Lint** | `cmake_quality.yml:96-153` | n/a | `test/lint/lint-all.sh` (`:149`) | Whitespace/include-guard/style hygiene across the new TUs. |

**Net CI consequence.** The four Qt-free cores get **ASan/UBSan** instrumentation (Sanitizers, GUI-OFF) plus functional coverage in all ~14 GUI-ON jobs — spanning **GCC** (Linux native, ARM64/ARMhf cross, Depends static, 7 distros), **AppleClang** (macOS ×2), and **mingw-gcc under wine** (Windows); shared- and static-link; glibc and musl; 64- and 32-bit. The two concurrency Qt suites (`wallet_event_queue_tests`, `wallettxstore_tests`) get that same broad functional coverage across the ~14 GUI-ON jobs — but the single accurate "no coverage" statement is narrow and important: **none of these jobs run under a sanitizer** (the only sanitizer job is GUI-OFF, so it never builds `src/qt`/`src/qt/test`). The data-race / use-after-free safety net for the concurrency primitives therefore rests on **TSan + the ASan/UBSan-GUI mesh soak**, not on any CI job.

---

## 5. GUI mesh-soak methodology

GUI-OFF CI cannot reach the model shells (`DetailedTxModel`, `OverviewTxModel`), the Qt event wiring, or the wallet-touching producer paths; and no CI job runs the GUI under a sanitizer. That irreducible risk is closed by a manual soak on an isolated 4-node testnet:

- **Topology / wallets.** A 4-node IP-namespace mesh with deliberately shaped wallets: heavy stake-accreted wallets (the worst case for ordering churn and `view_index` reposition volume) and a deliberately fragmented wallet (maximizes decomposed-part count per tx, stressing the same-hash contiguity invariant and the `(hash, idx)` identity paths).
- **Sanitizer builds.** Model-touching PRs are deployed as `RelWithDebInfo` ASan/UBSan GUI builds to instances 8/9, watching specifically for use-after-free across `std::vector` reallocation (the dangling-`TransactionRecord*` class the `(hash, idx)` identity rule prevents) and across the viewport boundary (placeholder/at() pointer lifetime). This is the **only** place the GUI runs under a sanitizer at all.
- **Lock-order enforcement.** Built with `-DENABLE_DEBUG_LOCKORDER=ON` so a runtime violation of `cs_main -> cs_wallet -> cs_store` is caught directly — the runtime complement to the compile-time Thread Safety job (which is Clang-only and so absent on the GCC-built deployed nodes).
- **Exercise pass.** A full scroll / header-click sort / filter-widget change / reorg / new-tx insert / address-book label-edit pass driven against those wallets, watching the producer worker drain, the per-tip status refresh, and the anchor-on-resort restore.

**What is soak-only** (no CI coverage at all): `WalletTxStore::reloadAndSnapshot`'s worker-quiesce rebuild barrier and `getRowDetail`'s matching-key → `TransactionDesc::toHTML` path both require a live `CWallet`/`cs_main` and are explicitly documented as soak-only (`src/qt/test/wallettxstore_tests.h:13-17`, `:27-33`). Only the null-wallet-safe early returns of `getRowDetail` are pinned in CI.

---

## 6. Real-world validation

A **121,979-transaction wallet on a 32-bit ARMv7 ODROID with 1.9 GiB RAM** was driven through the full detailed-table experience (scroll, sort, filter) with smooth interaction. This is the most-constrained real target the stack is expected to run on and proves the properties no unit suite can:

- **Random-access scroll on a six-figure history stays responsive** — the windowed consumer caches only a viewport slice, so paint cost and per-scroll fetch cost are bounded by the viewport, not by N.
- **Resort/refilter completes interactively** on 122k rows — the server-side cursor computes the new `view_index` off the render thread and the consumer reconciles a slice, rather than the old model's O(N) Qt-thread decompose + whole-model `dataChanged`.
- **32-bit pointer/index correctness** under real data — corroborating the ARMhf CI job with a genuine device and a genuine large wallet.
- **No render-path lock coupling** — after PR5-C the scroll/paint thread takes neither `cs_main` nor `cs_wallet`, which is what keeps the table fluid while the node is busy.

It does **not** demonstrate an in-process memory-footprint reduction: the full decomposed record set stays resident in the one process. The footprint win is explicitly deferred to the multiprocess split (#2937), where the store lives node-side.

---

## 7. Coverage matrix

In the "CI job" column below, **GUI-ON (×14)** is shorthand for the ~14 GUI-ON jobs that run `ctest`: `cmake_compatibility.yml` (Linux Native, ARM64, ARMhf), `cmake_production.yml` (Linux Static/Depends, Windows-under-wine, macOS ARM64, macOS Intel), and `cmake_distros.yml` (7 distros). The Qt-free cores additionally run in the **GUI-OFF Sanitizers** job (the only sanitizer-instrumented run).

| Behaviour | GUI-off unit test | CI job | Soak | Real-world |
|---|---|---|---|---|
| `(time DESC, hash ASC, idx ASC)` total order + `lower_bound` slot | `qt_txorder_tests` `:36-91` | Sanitizers + GUI-ON (×14) | — | — |
| Same-hash decomposed parts contiguous (remove-range invariant) | `qt_txorder_tests:95` | Sanitizers + GUI-ON (×14) | fragmented wallet | fragmented txs on ODROID |
| `Accepts` predicate (type/date/min/substr, inactive two-gate) | `qt_txfilter_tests:40-229` | Sanitizers + GUI-ON (×14) | filter-widget pass | — |
| Sort keys / `CompareKeys` 3-way + tie sentinel (PR4-fix E) | `qt_txfilter_tests:233-305` | Sanitizers + GUI-ON (×14) | header-click sort | sort on 122k rows |
| Address column `(label, address)` two-level sort (PR4-fix G) | `qt_txfilter_tests:308` | Sanitizers + GUI-ON (×14) | label-edit pass | — |
| Cursor identity-locate vs sort-key `lower_bound` (Item 1/2) | `qt_cursor_tests:179,203` | Sanitizers + GUI-ON (×14) | reposition under churn | — |
| Off-window maintenance, no emission (Item 3) | `qt_cursor_tests:272` | Sanitizers + GUI-ON (×14) | scroll past window | scroll on ODROID |
| Served-window evict/promote at cap | `qt_cursor_tests:226,250,302` | Sanitizers + GUI-ON (×14) | OverviewPage live | — |
| CT_UPDATED first-confirmation reposition | `qt_cursor_tests:328` | Sanitizers + GUI-ON (×14) | live tip advances | — |
| Interleaved relabel/refresh reposition (PR4-fix C) | `qt_cursor_tests:385` | Sanitizers + GUI-ON (×14) | address-book edit | — |
| `positionOf` → `rowForKey` / anchor backing (PR5) | `qt_cursor_tests:419,441` | Sanitizers + GUI-ON (×14) | anchor-on-resort | — |
| WindowCache structural Insert/Remove/Change base arithmetic | `qt_windowcache_tests:124-335` | Sanitizers + GUI-ON (×14) | scroll/insert/remove | scroll on ODROID |
| WindowCache seqno gate + Reset baseline (PR4-fix B) | `qt_windowcache_tests:339,358` | Sanitizers + GUI-ON (×14) | drain-race window | — |
| WindowCache content-channel exact-match gate | `qt_windowcache_tests:381-460` | Sanitizers + GUI-ON (×14) | fast-drag fetch race | fast drag on ODROID |
| End-to-end consumer routing (mirror of `applyEventBatch`) | `qt_windowcache_tests:480` | Sanitizers + GUI-ON (×14) | live model shell | live model on ODROID |
| MPSC queue dense/unique/monotonic seqnos | `wallet_event_queue_tests:46,105` | GUI-ON (×14); **no sanitizer** | ASan/UBSan-GUI deploy | — |
| Store-worker drain order + concurrent producers | `wallettxstore_tests:54,99` | GUI-ON (×14); **no sanitizer** | ASan/UBSan-GUI deploy | live producer load |
| Clean worker shutdown with pending intake | `wallettxstore_tests:138` | GUI-ON (×14); **no sanitizer** | node restart cycles | — |
| `getRowDetail` null-wallet-safe early returns (PR5-C) | `wallettxstore_tests:154,169` | GUI-ON (×14); **no sanitizer** | — | — |
| `cs_main->cs_wallet->cs_store` lock order (compile-time) | — | Thread Safety (Clang) | `-DENABLE_DEBUG_LOCKORDER` runtime | — |
| `reloadAndSnapshot` worker-quiesce rebuild barrier | **none** | **none** | ASan/UBSan-GUI deploy | startup on ODROID |
| `getRowDetail` matching-key → `toHTML` (live wallet) | **none** | **none** | double-click detail dialog | detail dialog on ODROID |
| Model shells (`DetailedTxModel`/`OverviewTxModel`), Qt wiring | **none** (shell); core mirrored at `qt_windowcache_tests:480` | **none** (sanitizer) | full GUI pass | full GUI on ODROID |

---

## 8. Known gaps & follow-ups

- **Sticky type-filter bug (soak-surfaced, untested).** A sticky type-filter bug surfaced during the GUI mesh soak. It is **pre-existing** (introduced in PR5-B, not by this windowed work as a whole) and tracked as a separate follow-up. **No windowed suite asserts against it**, and it must not be assumed covered by the type-mask unit case `accepts_type_mask` (`qt_txfilter_tests.cpp:110`): that case validates the bit-mask predicate in isolation, not the UI widget's persisted-state behaviour. This is the canonical example of a behaviour that lives entirely in the GUI shell, outside the GUI-OFF reach.
- **Wallet-touching producer paths have zero automated coverage.** `reloadAndSnapshot`'s rebuild barrier and `getRowDetail`'s `toHTML` path need a live `CWallet`/`cs_main` and are soak-only by explicit design (`src/qt/test/wallettxstore_tests.h:13-17,27-33`). If either regresses, only the mesh soak / ODROID will catch it.
- **Concurrency suites have no CI sanitizer coverage.** `wallet_event_queue_tests` and `wallettxstore_tests` *do* run in the ~14 GUI-ON jobs (compatibility ×3, production ×4, distros ×7) — across GCC/AppleClang/mingw, static- and shared-link, glibc/musl, 64- and 32-bit, Linux/Windows/macOS — but **never under a sanitizer**, because the only sanitizer job is GUI-OFF and so never builds `src/qt/test`. Their race/UAF safety net is TSan + the ASan/UBSan-GUI soak deploys, not CI.
- **Model shells are not GUI-OFF-testable.** `DetailedTxModel`/`OverviewTxModel` and the Qt event wiring cannot be unit-tested under `ENABLE_GUI=OFF`. The `consumer_routing_scenario` case (`qt_windowcache_tests.cpp:480`) pins the *reconcilable core* of `applyEventBatch`, but the model shell itself (placeholder rendering, viewport reporting, `ensureRowCached`, anchor capture/restore) is soak-only.
- **`waitForQueue` is a finite poll, not a barrier.** It caps at ~1.5 s (`wallettxstore_tests.cpp:45-50`); a saturated CI runner could in principle miss the deadline and fail the subsequent `QCOMPARE`. The ceiling is generous but not infinite — a flaky failure here means runner load, not a logic bug.
- **Thread-safety enforcement is Clang-only.** The lock-order annotations are no-ops under GCC, so the GCC jobs (Linux native, ARM64/ARMhf, Depends, distros) and the AppleClang macOS jobs do not enforce ordering. The compile-time guarantee comes solely from the Thread Safety (Clang) job; the runtime guarantee on deployed (GCC) nodes comes solely from `-DENABLE_DEBUG_LOCKORDER` in the soak.
- **On "PR5-D".** There is no PR5-D in the landed ladder. `canFetchMore`/`fetchMore` was **rejected** (a full `rowCount` is self-contradictory with it and breaks the scrollbar), not deferred; server-side windowed fetch (`getRows`) is already live in-process at this tip (PR5-A/B/C). What is genuinely deferred to #2937 is only the **transport swap** (Cap'n Proto schema + `interfaces::` shell) of `getRows`/`getAllRows`/`getRowDetail`/`rowForKey`; the call shape is final. Any future doc should not describe windowed fetch as a pending item.

### Provenance note (branch-verifiable mapping)
Each suite landed in the same commit as the core it exercises. The branch-verifiable fact is that these introducing commits self-label with **windowed-model step names** and reference **issue #2944**:

- `wallet_event_queue_tests.cpp` — `7842f9d01` *gui: add wallet_event_queue foundation (event types + MPSC queue)* (the #2944 event-queue foundation; predates the windowed ladder proper).
- `qt_txfilter_tests.cpp` — `1a3cdfb80` *qt: extract Qt-free transaction filter/sort core (**windowed-model PR1**, #2944)*.
- `qt_txorder_tests.cpp` — `f6588e14d` *qt: relocate cached-wallet ordering into producer-owned WalletTxStore (**windowed-model step 2**, #2944)*.
- `wallettxstore_tests.cpp` — `39d90ed74` *test: cover the **PR2.5** WalletTxStore store-worker*.
- `qt_cursor_tests.cpp` — `c92f80dea` *qt: add the Qt-free per-view cursor + view_index-maintenance suite (**windowed-model PR3-A**)*.
- `qt_windowcache_tests.cpp` — `761935dac` *qt: windowed-table read APIs + Qt-free WindowCache core (**windowed-model PR5-A**)*; the two `getRowDetail*` null-wallet cases arrived with the PR5-C tip `900889be6`.

The GitHub **PR numbers** sometimes attached to these steps (e.g. "#3003", "#3010", "#3015", …) come from the **PR tracker, not from branch git** — none of the introducing commits above carry them, so this document does not assert any PR-number-to-step mapping. (In particular, the commit that introduces the Qt-free filter core self-labels it *PR1*, which contradicts a "#3010 = filter core / #3003 = container swap" reading some summaries use; treat any such mapping as tracker metadata, not branch fact.) None of this affects the file:line test mappings in §2-§3, which are keyed to the tree at `900889be6`.
