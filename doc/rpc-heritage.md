# RPC Heritage Ledger

Per-RPC heritage classification for backport hygiene (issue #3069; supersedes the folder-isolation goals of #1142).
Answers, per RPC: can an upstream change be taken **safely** (pure-upstream), only with **careful manual porting** (mixed), is the upstream RPC **gone** so the target is a pinned pre-removal version (removed-upstream), or is it **N/A** (pure-gridcoin)?

## Enforcement
Each RPC's bucket + fingerprint baseline is a **column on its `vRPCCommands[]` row** in `src/rpc/server.cpp`: `{ "name", &impl, cat, &help, heritage_<bucket>, "<fp>" }`. The `CRPCCommand` constructor makes every field mandatory, so a command cannot be registered without a classification (classify-at-birth). `test/lint/lint-rpc-heritage.py` (run by `lint-all.sh`) reads that column -- the same table the dispatcher reads -- and enforces:
- **presence** on every registered RPC (the row's `heritage_<bucket>` parses to a known bucket; classify-at-birth is also compiler-enforced by the required constructor field);
- **surface-fingerprint drift** on `pure-upstream`, `mixed` & `removed-upstream` -- `fp` = `sha256("ARGS:" + args[in RPCHelpMan declaration order] + "|KEYS:" + sorted(result pushKV keys, gathered by cycle-safe recursive descent through called result-builder helpers))[:12]`. A mismatch means the input/output surface changed; re-confirm the bucket and update the row + this doc. `pure-gridcoin` is not fingerprinted (`heritage_fp` empty), and any fingerprinted RPC whose output isn't literal-key-trackable -- a dynamically-keyed object, or a positional array of scalars -- uses `heritage_fp` `manual` (drift reviewed by hand).

## Tally (206)
- pure-upstream (backport-safe): **14**
- mixed (careful porting): **56**
- removed-upstream (frozen fork, deleted from current upstream): **16**
- pure-gridcoin (no current upstream analogue): **120**

## pure-upstream -- backport-safe (fingerprint-tracked)
| RPC | file | args | result-keys | fp |
|---|---|---|---|---|
| `abandontransaction` | src/wallet/rpcwallet.cpp | `(txid)` | `{}` | `8a15aece8d9f` |
| `clearbanned` | src/rpc/net.cpp | `()` | `{}` | `e169db2f48c0` |
| `getbestblockhash` | src/rpc/blockchain.cpp | `()` | `{}` | `e169db2f48c0` |
| `getblockcount` | src/rpc/blockchain.cpp | `()` | `{}` | `e169db2f48c0` |
| `getblockhash` | src/rpc/blockchain.cpp | `(index)` | `{}` | `bd157738fbdb` |
| `getconnectioncount` | src/rpc/net.cpp | `()` | `{}` | `e169db2f48c0` |
| `keypoolrefill` | src/wallet/rpcwallet.cpp | `(newsize)` | `{}` | `90973fb49630` |
| `logging` | src/rpc/misc.cpp | `(include, category, exclude, category)` | `{}` | `manual` |
| `ping` | src/rpc/net.cpp | `()` | `{}` | `e169db2f48c0` |
| `setlabel` | src/wallet/rpcwallet.cpp | `(address, label)` | `{}` | `737f242a3755` |
| `stop` | src/rpc/server.cpp | `()` | `{}` | `e169db2f48c0` |
| `verifytxoutproof` | src/rpc/rawtransaction.cpp | `(proof)` | `{}` | `manual` |
| `walletlock` | src/wallet/rpcwallet.cpp | `()` | `{}` | `e169db2f48c0` |
| `walletpassphrasechange` | src/wallet/rpcwallet.cpp | `(oldpassphrase, newpassphrase)` | `{}` | `ab947140d4c5` |

## mixed -- careful manual porting (fingerprint-tracked)
| RPC | file | args | result-keys | fp | diverges / porting note |
|---|---|---|---|---|---|
| `addnode` | src/rpc/net.cpp | `(node, command)` | `{result}` | `00f3ee2c1fd4` | Gridcoin returns {"result":"ok"} where current upstream returns null, drops upstream's v2transport arg, and uses the legacy g_connman->ConnectNode path instead of OpenNetworkConnection(...,ConnectionType::MANUAL,use_v2transport); a porter taking an upstream addnode change must re-strip v2transport/BIP324 handling and preserve the ok-object output surface. |
| `analyzepsgt` | src/rpc/psgt.cpp | `(psgt)` | `{error, estimated_final_size, fee, has_utxo, inputs, is_final, min_required_fee, missing, next, pubkeys, redeemscript, signatures}` | `96a51f778705` | Renamed port of upstream `analyzepsbt` (PSGT = Partially Signed Gridcoin Transaction). Upstream PSBT changes port with the psbt->psgt rename plus Gridcoin transaction-format adaptation. |
| `backupwallet` | src/wallet/rpcwallet.cpp | `()` | `{Backup config success, Backup settings success, Backup wallet success, Files removed, Maintain backup file retention success, Number of files removed}` | `658099c7930f` | Shares upstream's name/purpose but is substantially rewritten: Gridcoin takes no destination arg (upstream's sole required arg), auto-paths the backup and additionally copies gridcoinresearch.conf + gridcoinsettings.json with retention pruning via GRC::BackupWallet/BackupConfigFile/BackupSettingsFile/MaintainBackups, and returns a 6-key status object vs upstream's null; almost nothing in the arg or output surface maps to upstream. |
| `combinepsgt` | src/rpc/psgt.cpp | `(psgts, psgt)` | `{}` | `b8c2d95892e8` | Renamed port of upstream `combinepsbt` (PSGT = Partially Signed Gridcoin Transaction). Upstream PSBT changes port with the psbt->psgt rename plus Gridcoin transaction-format adaptation. |
| `converttopsgt` | src/rpc/psgt.cpp | `(hexstring)` | `{}` | `633cdf5f2833` | Renamed port of upstream `converttopsbt` (PSGT = Partially Signed Gridcoin Transaction). Upstream PSBT changes port with the psbt->psgt rename plus Gridcoin transaction-format adaptation. |
| `createpsgt` | src/rpc/psgt.cpp | `(inputs, input, txid, vout, outputs, address, ntime)` | `{}` | `d834d7a48b91` | Renamed port of upstream `createpsbt` (PSGT = Partially Signed Gridcoin Transaction). Upstream PSBT changes port with the psbt->psgt rename plus Gridcoin transaction-format adaptation. |
| `createrawtransaction` | src/rpc/rawtransaction.cpp | `(inputs, txid, vout, outputs, address, data)` | `{}` | `50b21e94ef71` | Gridcoin is the pre-SegWit Bitcoin shape: only inputs+outputs args (no locktime/replaceable/version that upstream's CreateTxDoc/ConstructTransaction provide), no witness handling, and it hand-builds a GRC-format CMutableTransaction (carries nTime/hashBoinc, serialized at GRC PROTOCOL_VERSION); returns a bare hex string. A porter cannot lift upstream's ConstructTransaction wholesale. |
| `decodepsgt` | src/rpc/psgt.cpp | `(psgt)` | `{addresses, asm, bip32_derivs, final_scriptSig, hex, inputs, locktime, master_fingerprint, n, non_witness_utxo_hex, outputs, partial_signatures, path, pubkey, redeem_script, reqSigs, scriptPubKey, sequence, sighash, time, tx, txid, type, unknown, value, version, vin, vout}` | `b20178d7ef4b` | Renamed port of upstream `decodepsbt` (PSGT = Partially Signed Gridcoin Transaction). Upstream PSBT changes port with the psbt->psgt rename plus Gridcoin transaction-format adaptation. |
| `decoderawtransaction` | src/rpc/rawtransaction.cpp | `(hex_string)` | `{action, address, addresses, allocation, amount, asm, authorized_operator_pubkey, block_subsidy, blockhash, blocktime, body, choices, client_version, coinbase, confirmations, contracts, cpid, description, duration_days, fee, hashboinc, hex, id, key, label, last_block_hash, locktime, magnitude, magnitude_unit, mining_id, n, name, operator_pubkey, organization, poll_txid, public_key, question, quorum_address, quorum_hash, reqSigs, research_subsidy, response_type, responses, scriptPubKey, scriptSig, sequence, signature, size, status, time, title, txid, type, url, value, version, vin, vout, weight_type}` | `ee4cdec04193` | Gridcoin drops upstream's iswitness arg and decodes via TxToJSON (not TxToUniv): the output adds Gridcoin-specific keys contracts + hashboinc and a time(nTime) field, and omits upstream's hash/vsize/weight and all witness/SegWit fields. A porter must remap the output surface, not just take TxToUniv changes. |
| `decodescript` | src/rpc/rawtransaction.cpp | `(hex_string)` | `{addresses, asm, hash, hex, htlc, p2sh, receiver_pubkey, reqSigs, sender_pubkey, timeout, type}` | `ca2d8ef9d7fb` | Gridcoin emits the legacy asm/type/reqSigs/addresses surface (via ScriptPubKeyToJSON) plus a Gridcoin-only htlc{hash,receiver_pubkey,sender_pubkey,timeout} block and an unconditional p2sh from script.GetID(); upstream has reworked decodescript to add desc (descriptor), singular address, a segwit{} wrapper and drop reqSigs/addresses, so a porter taking upstream's segwit/descriptor logic must account for Gridcoin's absent witness infrastructure and must not clobber the HTLC analysis. |
| `encryptwallet` | src/wallet/rpcwallet.cpp | `(passphrase)` | `{}` | `7b0696269744` | Body is upstream-derived but retains the legacy forced-restart behavior: Gridcoin calls StartShutdown() and returns a 'Gridcoin server stopping, restart to run with encrypted wallet' string, whereas current upstream encrypts in place (no shutdown) and returns a different message; Gridcoin also lacks the descriptor-wallet guards (WALLET_FLAG_DISABLE_PRIVATE_KEYS, IsScanningWithPassphrase, m_relock_mutex lock). A porter must preserve the shutdown semantics unless intentionally changing the lifecycle. |
| `finalizepsgt` | src/rpc/psgt.cpp | `(psgt)` | `{complete, hex}` | `bc3c28ffc9eb` | Renamed port of upstream `finalizepsbt` (PSGT = Partially Signed Gridcoin Transaction). Upstream PSBT changes port with the psbt->psgt rename plus Gridcoin transaction-format adaptation. |
| `fundrawtransaction` | src/rpc/rawtransaction.cpp | `(hexstring, options, changeAddress, changePosition, includeWatching)` | `{changepos, fee, hex}` | `5373087fac94` | Output surface (hex/fee/changepos) matches upstream, but Gridcoin explicitly rejects any tx that already has inputs (no UTXO-value lookup) and supports only three options (changeAddress, changePosition, includeWatching) versus upstream's large option set (add_inputs, include_unsafe, minconf/maxconf, change_type, lockUnspents, fee_rate/feeRate, subtractFeeFromOutputs, input_weights, max_tx_weight) plus the positional iswitness arg; GRC's CWallet::FundTransaction has a different signature, so option-set or fee-rate ports must be adapted by hand. |
| `generatetoaddress` | src/rpc/mining.cpp | `(nblocks, address)` | `{}` | `manual` | Shares upstream's (nblocks, address) args and the array-of-blockhashes output, but drops maxtries and reroutes the reward to PoS wallet keys (address is advisory under PoS, not the payout target). Output is a positional array -> fp=manual. |
| `getaddednodeinfo` | src/rpc/net.cpp | `(dns, node)` | `{addednode, address, addresses, connected}` | `3825ee523156` | Gridcoin retains the legacy required boolean 'dns' first argument that upstream removed (upstream takes only optional 'node'), and returns a JSON object (dns=false -> repeated 'addednode' keys; dns=true -> per-node objects) instead of upstream's flat array of {addednode,connected,addresses[{address,connected}]}. Result also carries a top-level 'connected' bool plus per-address 'connected' as a string ('inbound'/'outbound'/'false'). Access path uses CConnman GetAddedNodes/ForEachNode (issue #2558) rather than upstream's GetAddedNodeInfo; a porter must reconcile the dropped 'dns' arg and the object-vs-array result shape. |
| `getaddressesbylabel` | src/wallet/rpcwallet.cpp | `(label)` | `{purpose}` | `5058a857a5e1` | Account->label migration entanglement: emits `unknown` purpose for un-migrated address-book entries (see migratelabels) and drops upstream's is_change guard; a straight backport is unsafe. |
| `getbalance` | src/wallet/rpcwallet.cpp | `(account, minconf, includeWatchonly)` | `{}` | `4bbd03d840d3` | Gridcoin retains the deprecated accounts subsystem: a positional 'account' arg with AccountFromValue/GetAccountBalance/accountingDeprecationCheck and a separate '*' manual-summation path, whereas current upstream replaced arg[0] with a 'dummy' placeholder and added 'avoid_reuse'; a porter must not blindly map upstream's arg list and must preserve the accounts behavior (body returns a bare STR_AMOUNT, no pushKV keys). |
| `getblock` | src/rpc/blockchain.cpp | `(hash, txinfo)` | `{BoincHash, IsContract, IsSuperBlock, MoneySupply, action, address, addresses, all_cpid_total_credit, allocation, amount, asm, authorized_operator_pubkey, average_rac, beacons, bits, block_subsidy, blockhash, blocktime, blocktrust, body, chaintrust, choices, claim, client_version, coinbase, confirmations, contracts, cpid, description, difficulty, duration_days, entropybit, fee, fees_collected, flags, hash, hashboinc, height, hex, id, key, label, last_block_hash, locktime, m_mrc_tx_map_size, magnitude, magnitude_unit, magnitudes, merkleroot, mining_id, mint, modifier, mrc_foundation_fees, mrc_staker_fees, mrcs, n, name, nextblockhash, nonce, operator_pubkey, organization, poll_txid, previousblockhash, project, project_all_cpid_total_credits, project_greylist_status, projects, proofhash, public_key, question, quorum_address, quorum_hash, rac, reqSigs, research_subsidy, response_type, responses, scriptPubKey, scriptSig, sequence, signature, size, status, superblock, time, title, total_credit, tx, txid, type, url, value, version, vin, vout, weight_type}` | `ef9d1e297dba` | blockToJSON descends from upstream's block formatter but adds PoS keys (proofhash/entropybit/modifier/blocktrust/chaintrust/flags/signature) and research+contract keys (mint/claim/MoneySupply/superblock/BoincHash/IsSuperBlock/IsContract/mrc_foundation_fees/mrc_staker_fees), while dropping upstream's versionHex/strippedsize/weight/target/chainwork/nTx/mediantime/coinbase_tx; the arg is a bool 'txinfo' (verbosity 0/2/3 hex-and-prevout modes do not exist) vs upstream's numeric 'verbosity\|verbose' (0-3), so a porter must hand-map verbosity semantics and keep the Gridcoin output keys. |
| `getblockchaininfo` | src/rpc/blockchain.cpp | `()` | `{blocks, current, difficulty, errors, in_sync, moneysupply, target, testnet}` | `1d89fd23ddb0` | Same name and chain-info purpose as upstream but a near-total reimplementation: Gridcoin emits only blocks/in_sync/moneysupply/difficulty/testnet/errors and 'difficulty' is an object {current,target} (PoS) rather than a number, sharing essentially nothing with upstream's chain/headers/bestblockhash/bits/target/time/mediantime/verificationprogress/initialblockdownload/backgroundvalidation/chainwork/size_on_disk/pruning/signet/warnings surface; an upstream change to this RPC is not takeable without a full rewrite, so a porter must treat the two output schemas as disjoint. |
| `getdifficulty` | src/rpc/blockchain.cpp | `()` | `{current, target}` | `400047400aac` | Upstream returns a bare scalar PoW difficulty (GetDifficulty(tip)); Gridcoin returns an OBJ {current,target} from PoS helpers GRC::GetCurrentDifficulty()/GetTargetDifficulty(), so a porter must preserve the object wrapper and PoS semantics rather than collapse it to upstream's scalar. |
| `getmempoolentry` | src/rpc/mempool.cpp | `(txid)` | `{beacon_cpid, contract_types, fee, feerate, height, mrc_cpid, mrc_fee, size, time}` | `a0a4faa15dda` | Adds Gridcoin `contract_types`/`mrc_cpid`/`mrc_fee`/`beacon_cpid` fields and drops upstream's ancestor/descendant/RBF/witness surface. |
| `getmempoolinfo` | src/rpc/mempool.cpp | `()` | `{beacon_count, bytes, mandatory_sidestake_count, maxmempool, mrc_count, size, usage}` | `2eb7e5d70d20` | Adds Gridcoin `mrc_count`/`beacon_count`/`mandatory_sidestake_count` and omits upstream fee/relay/broadcast fields. |
| `getnettotals` | src/rpc/net.cpp | `()` | `{timemillis, totalbytesrecv, totalbytessent}` | `ca8fae8b33d9` | The three core keys (totalbytesrecv/totalbytessent/timemillis) match upstream, but Gridcoin omits upstream's entire uploadtarget sub-object (no max-outbound-target subsystem) and reads CNode static byte counters + GetTimeMillis() instead of connman + SystemClock, so any upstream uploadtarget change has no Gridcoin landing spot. |
| `getnetworkinfo` | src/rpc/net.cpp | `()` | `{address, connections, errors, ip, localaddresses, mininput, minor_version, paytxfee, port, protocolversion, proxy, score, timeoffset, version}` | `54255a461949` | Gridcoin retains a legacy output surface (paytxfee, mininput, proxy, ip, errors, minor_version) and lacks upstream's subversion/networks/relayfee/incrementalfee/networkactive/connections_in/connections_out/localservices keys; the two key sets barely overlap, so upstream patches cannot be applied wholesale and a porter must hand-map any change. |
| `getnewaddress` | src/wallet/rpcwallet.cpp | `(account)` | `{}` | `e503f05f1459` | GRC retains the deprecated single 'account' arg and a keypool+address-book body (GetKeyFromPool/SetAddressBookName), whereas upstream takes 'label'+'address_type' and uses GetNewDestination with output types/descriptor wallets; a porter cannot take upstream segwit/output-type changes without GRC's missing address-type and accounts plumbing. |
| `getnodeaddresses` | src/rpc/net.cpp | `(count)` | `{address, port, services, time}` | `c696e3da4da2` | GRC lacks the upstream 'network' arg and the 'network' output key, uses a different addrman path (g_connman->GetAddrMan().GetAddr() then min(count,size)) and treats count<=0 as an error while upstream treats count==0 as 'return all'; a porter must add network filtering and reconcile the count==0 semantics. |
| `getpeerinfo` | src/rpc/net.cpp | `()` | `{addr, addrlocal, banscore, bytesrecv, bytessent, conntime, id, inbound, lastrecv, lastsend, minping, nTrust, pingtime, pingwait, services, startingheight, subver, timeoffset, version}` | `4720f2ba0e5d` | GRC emits PoS-specific 'nTrust' and a retained 'banscore' (nMisbehavior) that upstream removed, and its CNodeStats omits the dozens of modern upstream fields (network, servicesnames, synced_headers/blocks, permissions, bip152_hb_*, bytes*_per_msg, connection_type, session_id, etc.); upstream output additions require corresponding CNodeStats fields that do not exist in GRC. |
| `getrawmempool` | src/rpc/mempool.cpp | `(verbose)` | `{beacon_cpid, contract_types, fee, feerate, height, mrc_cpid, mrc_fee, size, time}` | `7aaf954ee6c6` | Non-verbose path returns the txid array; the verbose path (added #3029, impl moved to rpc/mempool.cpp) renders MempoolEntryToJSON with Gridcoin contract_types/mrc_cpid/mrc_fee/beacon_cpid fields and drops upstream's ancestor/descendant/RBF/witness surface and the 'mempool_sequence' arg + machinery. A porter must not blind-take upstream verbose/sequence changes. |
| `getrawtransaction` | src/rpc/rawtransaction.cpp | `(txid, verbose)` | `{action, address, addresses, allocation, amount, asm, authorized_operator_pubkey, block_subsidy, blockhash, blocktime, body, choices, client_version, coinbase, confirmations, contracts, cpid, description, duration_days, fee, hashboinc, hex, id, key, label, last_block_hash, locktime, magnitude, magnitude_unit, mining_id, n, name, operator_pubkey, organization, poll_txid, public_key, question, quorum_address, quorum_hash, reqSigs, research_subsidy, response_type, responses, scriptPubKey, scriptSig, sequence, signature, size, status, time, title, txid, type, url, value, version, vin, vout, weight_type}` | `0da7a920386c` | GRC uses a local TxToJSON fork (not upstream TxToUniv) that emits Gridcoin contract output ('contracts','hashboinc') and a PoS tx 'time', omits segwit 'hash'/'vsize'/'weight', and the arg surface is only txid+verbose (bool-or-num) with no 'blockhash' arg and no verbosity=2 fee/prevout path; a porter must hand-merge any upstream TxToUniv field changes into TxToJSON. |
| `getreceivedbyaddress` | src/wallet/rpcwallet.cpp | `(address, minconf)` | `{}` | `abf78a6d1f07` | GRC's tally loop explicitly excludes PoS coinstake outputs (wtx.IsCoinStake()) in addition to coinbase, and lacks upstream's 'include_immature_coinbase' arg (upstream delegates to GetReceived helper); a porter must preserve the coinstake exclusion and account for the missing immature-coinbase option. |
| `gettransaction` | src/wallet/rpcwallet.cpp | `(txid, includeWatchonly)` | `{Type, account, action, address, addresses, allocation, amount, asm, authorized_operator_pubkey, block_subsidy, blockhash, blockindex, blocktime, body, category, choices, client_version, coinbase, confirmations, contracts, cpid, description, details, duration_days, fee, generated, hashboinc, hex, id, involvesWatchonly, key, label, last_block_hash, locktime, magnitude, magnitude_unit, mining_id, n, name, operator_pubkey, organization, poll_txid, public_key, question, quorum_address, quorum_hash, reqSigs, research_subsidy, response_type, responses, scriptPubKey, scriptSig, sequence, signature, size, status, time, timereceived, title, txid, type, url, value, version, vin, vout, weight_type}` | `b82e0de9cd38` | Body is upstream-shaped (amount/fee/details) but folds in the Gridcoin TxToJSON surface (hashboinc, contracts) and PoS/MRC category+type strings via ListTransactions, plus a non-wallet GetTransaction fallback branch; it retains the old includeWatchonly arg and omits upstream's verbose/decoded/hex/parent_descs/lastprocessedblock -- a porter must not blind-take upstream's amount/fee/details/verbose refactor and must preserve hashboinc/contracts and the dynamic mapValue keys (comment/to/message). |
| `gettxoutproof` | src/rpc/rawtransaction.cpp | `(txids, txid, blockhash)` | `{}` | `0a9a9c33566c` | Assumes an always-on full txindex (no UTXO-cache fallback), resolves the containing block from the FIRST txid deterministically, and adds an explicit main-chain gate; architecture-driven divergence from upstream. |
| `getwalletinfo` | src/wallet/rpcwallet.cpp | `()` | `{balance, keypoololdest, keypoolsize, masterkeyid, mining-error, newmint, stake, staking, unlocked_until, walletversion}` | `693ce305655c` | Only walletversion/keypoolsize/keypoololdest/unlocked_until/masterkeyid overlap upstream; Gridcoin emits PoS+research balances (balance/newmint/stake) and miner status (staking, mining-error) instead of upstream's descriptor-era surface (walletname/format/txcount/keypoolsize_hd_internal/private_keys_enabled/avoid_reuse/scanning/descriptors/external_signer/blank/birthtime/flags/lastprocessedblock). A porter must keep the Gridcoin-specific keys and cannot adopt the descriptor/scanning fields. |
| `help` | src/rpc/server.cpp | `(command)` | `{}` | `4ce89ec7b45f` | Core delegates to tableRPC.help like upstream, but the Gridcoin body adds custom category dispatch (wallet/staking/mining/developer/network/voting), a donation-address message in the help text, and an empty-command early-return to help_helpman().ToString(); a porter taking an upstream help change must reconcile this category branching. |
| `listaddressgroupings` | src/wallet/rpcwallet.cpp | `()` | `{}` | `manual` | Output carries the deprecated accounts subsystem (per-group 'account' field) that upstream removed -- same divergence that makes getbalance/listtransactions mixed. |
| `listbanned` | src/rpc/net.cpp | `()` | `{address, ban_created, ban_duration, ban_reason, banned_until, time_remaining}` | `2c86549f9bbd` | Emits a 'ban_reason' output key that upstream removed; an upstream backport must preserve/justify ban_reason. |
| `listlabels` | src/wallet/rpcwallet.cpp | `(purpose)` | `{}` | `manual` | Hand-rolled mapAddressBook loop with a Gridcoin-only empty-name skip (GetAccountAddress/default-key stubs) rather than upstream's pwallet->ListAddrBookLabels(); account-subsystem entanglement. |
| `listreceivedbyaddress` | src/wallet/rpcwallet.cpp | `(minconf, includeempty, includeWatchonly)` | `{account, address, amount, confirmations, involvesWatchonly, txids}` | `4b8fb0ed2cee` | Gridcoin's ListReceived retains the deprecated accounts subsystem (emits 'account' + 'involvesWatchonly' rather than upstream's 'label'), skips coinbase/coinstake outputs, and lacks upstream's newer 'address_filter' and 'include_immature_coinbase' args; a porter must remap account->label semantics and not assume the address_filter/immature-coinbase params exist. |
| `listsinceblock` | src/wallet/rpcwallet.cpp | `(blockhash, target_confirmations, include_watchonly)` | `{Type, account, address, amount, blockhash, blockindex, blocktime, category, confirmations, fee, generated, involvesWatchonly, lastblock, time, timereceived, transactions, txid, type, vout}` | `4ac802bc4b7c` | Gridcoin has no 'removed' reorg array and no include_removed/include_change/label args, finds the start block via mapBlockIndex (not chain.findCommonAncestor / fork handling), iterates mapWallet linearly, and the per-tx entries carry PoS/research fields (orphan/immature/generate categories plus 'type'/'Type' mined-type strings, 'account', 'involvesWatchonly') with no 'vout'/'abandoned'/'wtxid'/'walletconflicts'; a porter must add fork-aware ancestor logic and reconcile the divergent entry schema. |
| `listtransactions` | src/wallet/rpcwallet.cpp | `(account, count, from, includeWatchonly)` | `{Type, account, address, amount, blockhash, blockindex, blocktime, category, comment, confirmations, fee, generated, involvesWatchonly, otheraccount, time, timereceived, txid, type, vout}` | `60208a325cb1` | Gridcoin's first arg is 'account' (accounts subsystem) not upstream's 'label', and it emits AcentryToJSON 'move' entries (account/otheraccount/comment) plus PoS/research mined-type fields ('type'/'Type' = POR/POS/sidestake/MRC, orphan/immature/generate categories) instead of upstream's 'vout'/'abandoned'/'label' + TransactionDescriptionString block; a porter must keep the account/move plumbing and the Gridcoin coinstake type mapping. |
| `listunspent` | src/rpc/rawtransaction.cpp | `(minconf, maxconf, addresses, address)` | `{account, address, amount, confirmations, label, scriptPubKey, txid, vout}` | `55713032beff` | Gridcoin lacks upstream's 'include_unsafe' and 'query_options' (minimumAmount/maximumAmount/maximumCount/minimumSumAmount/include_immature_coinbase) args and emits a much thinner output (no spendable/solvable/safe/desc/parent_descs/redeemScript/witnessScript/ancestor* keys), instead adding an 'account' key gated on -enableaccounts; a porter must not assume the descriptor/solvability/safe machinery or the options object exist. |
| `sendmany` | src/wallet/rpcwallet.cpp | `(fromaccount, amounts, address, minconf, comment)` | `{}` | `c48e33af5e40` | Gridcoin retains the deprecated accounts subsystem: the first arg is a real account name driving GetAccountBalance/strFromAccount and minconf is honored, whereas upstream's first arg is an ignored 'dummy' and minconf is ignored; Gridcoin also lacks all modern args (subtractfeefrom, replaceable, conf_target, estimate_mode, fee_rate, verbose) and the verbose OBJ result form, so a porter must not map arg positions to upstream. |
| `sendrawtransaction` | src/rpc/rawtransaction.cpp | `(hex_string)` | `{}` | `52fce61c2242` | Gridcoin keeps the legacy single-arg form using AcceptToMemoryPool + RelayTransaction with a GetTransaction pre-check, while upstream (now in mempool.cpp) adds maxfeerate/maxburnamount safety args and routes through BroadcastTransaction with -privatebroadcast handling; a porter must hand-add the fee/burn guards and cannot assume the modern broadcast path. |
| `sendtoaddress` | src/wallet/rpcwallet.cpp | `(address, amount, comment, comment_to, message)` | `{}` | `ce89abdb7a10` | Gridcoin adds a Gridcoin-only 5th arg 'message' that attaches a GRC::TxMessage contract to the transaction and otherwise keeps the legacy SendMoneyToDestination path; it lacks all of upstream's subtractfeefromamount/replaceable/conf_target/estimate_mode/avoid_reuse/fee_rate/verbose args and the verbose OBJ result, so a porter must preserve the contract-message arg and cannot map positions to upstream. |
| `setban` | src/rpc/net.cpp | `(subnet, command, bantime, absolute)` | `{}` | `b5a439644c97` | Calls Gridcoin's retained 4-arg g_banman->Ban(subnet, BanReasonManuallyAdded, bantime, absolute); upstream deleted the BanReason enum (3-arg Ban) but GRC still serializes it to banlist.dat. RPC surface (subnet/ip, command, bantime, absolute; ban/unban) is upstream-faithful, so the divergence is below the fingerprint -- a backport must reconcile the Ban() signature / on-disk format. |
| `setmocktime` | src/rpc/blockchain.cpp | `(timestamp)` | `{}` | `994b2dfc6e39` | Port of upstream setmocktime; Gridcoin adapts it to its own SetMockTime()/Params().IsMockableChain() and the older single-arg RPC layer -- no NodeContext chain_clients mock-propagation loop and no upper-bound clamp -- so an upstream change must be hand-ported, not blind-taken. |
| `signmessage` | src/wallet/rpcwallet.cpp | `(address, message)` | `{}` | `6b1488c62c48` | Signs with Gridcoin's own message magic (strMessageMagic, src/main.cpp:108), not Bitcoin's -- a naive upstream backport silently breaks signature compatibility. |
| `signrawtransactionwithkey` | src/rpc/rawtransaction.cpp | `(hexstring, privkeys, privatekey, prevtxs, txid, vout, scriptPubKey, sighashtype)` | `{complete, hex}` | `6aca6f6eb6c9` | Body is the legacy pre-SegWit signrawtransaction split: prevtxs omits redeemScript/witnessScript/amount, no Taproot/DEFAULT sighash, result omits upstream's per-input `errors` array (emits only hex+complete), and it signs via Gridcoin's FetchInputs/CTxDB + CombineSignatures merge rather than upstream's FlatSigningProvider/descriptor path -- a porter must not import any segwit/witness-aware signing logic. |
| `signrawtransactionwithwallet` | src/rpc/rawtransaction.cpp | `(hexstring, prevtxs, txid, vout, scriptPubKey, sighashtype)` | `{complete, hex}` | `1d9359847326` | Shares SignRawTransactionHelper with the withkey variant: legacy non-segwit signing over Gridcoin's wallet, prevtxs lacks redeemScript/witnessScript/amount, no Taproot/DEFAULT sighash, and the result omits upstream's `errors` array (only hex+complete) -- keep segwit/descriptor signing out when porting. |
| `testmempoolaccept` | src/rpc/mempool.cpp | `(rawtxs, rawtx)` | `{allowed, base, fees, reject-reason, txid, vsize}` | `3119a888d278` | Reduced dry-run port (#3029): no maxfeerate arg, single-tx (not package) validation, omits wtxid/package-error/effective-feerate; reasons best-effort. |
| `utxoupdatepsgt` | src/rpc/psgt.cpp | `(psgt)` | `{}` | `bcc738b59c8b` | Renamed port of upstream `utxoupdatepsbt` (PSGT = Partially Signed Gridcoin Transaction). Upstream PSBT changes port with the psbt->psgt rename plus Gridcoin transaction-format adaptation. |
| `validateaddress` | src/wallet/rpcwallet.cpp | `(address)` | `{account, address, hdkeypath, hdmasterkeyid, ismine, isvalid}` | `97873d91c025` | Shares upstream's `address` arg and the isvalid/address/scriptPubKey core, but retains the pre-0.18 COMBINED wallet form (ismine, account, hdkeypath, hdmasterkeyid, pubkey, iscompressed) that upstream split into getaddressinfo, and lacks upstream's witness/descriptor fields. Do not blind-take upstream's getaddressinfo split or witness logic. |
| `verifymessage` | src/wallet/rpcwallet.cpp | `(address, signature, message)` | `{}` | `c9cdde03d99c` | Verifies against Gridcoin's message magic, not Bitcoin's; a backport must keep the GRC magic or signatures won't validate. |
| `walletcreatefundedpsgt` | src/rpc/psgt.cpp | `(inputs, input, txid, vout, outputs, address, options, changeAddress, sign)` | `{fee, psgt}` | `d2f84eca950d` | Renamed port of upstream `walletcreatefundedpsbt` (PSGT = Partially Signed Gridcoin Transaction). Upstream PSBT changes port with the psbt->psgt rename plus Gridcoin transaction-format adaptation. |
| `walletpassphrase` | src/wallet/rpcwallet.cpp | `(passphrase, timeout, stakingonly)` | `{}` | `2c3e2b290e76` | Gridcoin adds a third PoS-only `stakingonly` arg that sets the ppcoin `fWalletUnlockStakingOnly` flag and rejects re-unlock with RPC_WALLET_ALREADY_UNLOCKED, and relocks via NewThread(ThreadCleanWalletPassphrase) rather than upstream's scheduler weak-ptr callback; a porter must preserve the stakingonly arg/flag and the thread-based relock and not blindly adopt upstream's scheduler/multiwallet relock model. |
| `walletprocesspsgt` | src/rpc/psgt.cpp | `(psgt, sighashtype)` | `{complete, psgt}` | `07d707e85e80` | Renamed port of upstream `walletprocesspsbt` (PSGT = Partially Signed Gridcoin Transaction). Upstream PSBT changes port with the psbt->psgt rename plus Gridcoin transaction-format adaptation. |

## removed-upstream -- frozen Bitcoin-lineage fork, deleted from current upstream (fingerprint-tracked)
Upstream-derived RPCs that current upstream has **removed** (legacy/BDB-wallet sunset, the `CAlert` system, the legacy account family, the `signrawtransaction` split). No current backport target exists, but they are frozen forks: the fingerprint pins the surface so any change -- including a deliberate pull up to the **last valid pre-removal upstream version** -- is a controlled, re-confirmed event, not silent drift.

| RPC | file | args | result-keys | fp | diverges / porting note |
|---|---|---|---|---|---|
| `addmultisigaddress` | src/wallet/rpcwallet.cpp | `(nrequired, keys, key, account)` | `{}` | `b0bd7231da33` | removed from current upstream (legacy-wallet sunset); GRC also retains the legacy `account` arg (upstream renamed it to `label` before removal) -- behind the last valid version. |
| `addredeemscript` | src/wallet/rpcwallet.cpp | `(redeemScript, account)` | `{}` | `c9e4350b4a75` | removed from current upstream (legacy P2SH redeemscript import; superseded by importaddress/descriptors, themselves since removed). |
| `dumpprivkey` | src/wallet/rpcdump.cpp | `(gridcoinaddress, dump_hex)` | `{private_key, private_key_hex, public_key_hex}` | `96d3ee80fb58` | removed from current upstream (legacy-wallet sunset); GRC is also EXTENDED -- extra `dump_hex` arg + object return (private_key/private_key_hex/public_key_hex), where legacy upstream took (address) and returned a bare WIF string. |
| `dumpwallet` | src/wallet/rpcdump.cpp | `(filename)` | `{}` | `eaad3c94f6a6` | removed from current upstream (legacy-wallet sunset). |
| `getinfo` | src/wallet/rpcwallet.cpp | `()` | `{balance, blocks, connections, current, difficulty, errors, in_sync, ip, keypoololdest, keypoolsize, mininput, minor_version, moneysupply, newmint, paytxfee, protocolversion, proxy, stake, target, testnet, timeoffset, unlocked_until, uptime, version, walletversion}` | `1e5e3bf87281` | removed from current upstream (0.16; split into getblockchaininfo/getnetworkinfo/getwalletinfo); GRC retains the combined form with PoS/research fields (newmint/stake/in_sync/moneysupply). |
| `getunconfirmedbalance` | src/wallet/rpcwallet.cpp | `()` | `{}` | `e169db2f48c0` | removed from current upstream (superseded by getbalances). |
| `importprivkey` | src/wallet/rpcdump.cpp | `(gridcoinprivkey, label, rescan)` | `{}` | `907321bb2292` | removed from current upstream (legacy-wallet sunset); first arg renamed privkey->gridcoinprivkey (cosmetic). |
| `importwallet` | src/wallet/rpcdump.cpp | `(filename)` | `{}` | `eaad3c94f6a6` | removed from current upstream (legacy-wallet sunset). |
| `listalerts` | src/rpc/net.cpp | `()` | `{alerts, applies_to_me, cancel_upto, cancels, comment, expiration, hash, id, in_effect, maximum_version, minimum_version, priority, relay_until, reserved, status_bar, subversions, version}` | `1b838be933d7` | removed from current upstream (CAlert network-alert system retired in 0.14); GRC-side listing of retained alerts. |
| `resendtx` | src/wallet/rpcwallet.cpp | `()` | `{}` | `e169db2f48c0` | rename of upstream's resendwallettransactions, which upstream removed (rebroadcast became automatic); removed-upstream via its renamed ancestor. |
| `sendalert` | src/rpc/net.cpp | `(message, privatekey, minver, maxver, priority, id, cancelupto)` | `{nCancel, nID, nMaxVer, nMinVer, nPriority, nVersion, strStatusBar}` | `23feaca9b1f5` | removed from current upstream (CAlert network-alert system retired in 0.14); GRC retains its alert infrastructure + payload. |
| `sendalert2` | src/rpc/net.cpp | `(privatekey, id, subverlist, cancellist, expire, priority, message)` | `{Content, Success}` | `088679c27b11` | removed from current upstream (CAlert network-alert system retired in 0.14); GRC retains its alert infrastructure + payload. |
| `sethdseed` | src/wallet/rpcwallet.cpp | `(newkeypool, seed)` | `{}` | `dab6675cb642` | removed from current upstream (legacy-wallet sunset). |
| `settxfee` | src/rpc/blockchain.cpp | `(amount)` | `{}` | `468a6a9ff691` | removed from current upstream tree. |
| `signrawtransaction` | src/rpc/rawtransaction.cpp | `(hexstring, prevtxs, txid, vout, scriptPubKey, privkeys, privatekey, sighashtype)` | `{complete, hex}` | `d75693d24eec` | removed from current upstream (0.17; split into signrawtransactionwithkey/signrawtransactionwithwallet); GRC retains the combined legacy form. |
| `upgradewallet` | src/wallet/rpcwallet.cpp | `(version)` | `{}` | `2b104a7e47cc` | removed from current upstream tree. |

## pure-gridcoin -- no current upstream analogue by function (not fingerprinted)
| RPC | file |
|---|---|
| `addkey` | src/rpc/blockchain.cpp |
| `addpoll` | src/rpc/voting.cpp |
| `advertisebeacon` | src/rpc/blockchain.cpp |
| `advertisebeaconv3` | src/rpc/blockchain.cpp |
| `approvepool` | src/rpc/blockchain.cpp |
| `archivelog` | src/gridcoin/scraper/scraper.cpp |
| `askforoutstandingblocks` | src/rpc/blockchain.cpp |
| `auditsnapshotaccrual` | src/rpc/mining.cpp |
| `auditsnapshotaccruals` | src/rpc/mining.cpp |
| `authorizepool` | src/rpc/blockchain.cpp |
| `beaconaudit` | src/rpc/blockchain.cpp |
| `beaconauth` | src/rpc/blockchain.cpp |
| `beaconconvergence` | src/rpc/blockchain.cpp |
| `beaconreport` | src/rpc/blockchain.cpp |
| `beaconstatus` | src/rpc/blockchain.cpp |
| `burn` | src/wallet/rpcwallet.cpp |
| `changesettings` | src/rpc/misc.cpp |
| `checkwallet` | src/wallet/rpcwallet.cpp |
| `claimhtlc` | src/rpc/htlc.cpp |
| `consolidatemsunspent` | src/rpc/rawtransaction.cpp |
| `consolidateunspent` | src/rpc/rawtransaction.cpp |
| `convergencereport` | src/gridcoin/scraper/scraper.cpp |
| `createhtlc` | src/rpc/htlc.cpp |
| `createmrcrequest` | src/rpc/blockchain.cpp |
| `currentcontractaverage` | src/rpc/blockchain.cpp |
| `currenttime` | src/rpc/blockchain.cpp |
| `debug` | src/rpc/blockchain.cpp |
| `deletecscrapermanifest` | src/gridcoin/scraper/scraper.cpp |
| `dumpcontracts` | src/rpc/blockchain.cpp |
| `explainmagnitude` | src/rpc/blockchain.cpp |
| `exportstats1` | src/rpc/dataacq.cpp |
| `generate` | src/rpc/mining.cpp |
| `generatesuperblock` | src/rpc/mining.cpp |
| `getaccount` | src/wallet/rpcwallet.cpp |
| `getaccountaddress` | src/wallet/rpcwallet.cpp |
| `getaddressesbyaccount` | src/wallet/rpcwallet.cpp |
| `getautogreylist` | src/rpc/blockchain.cpp |
| `getbalancedetail` | src/wallet/rpcwallet.cpp |
| `getblockbymintime` | src/rpc/blockchain.cpp |
| `getblockbynumber` | src/rpc/blockchain.cpp |
| `getblocksbatch` | src/rpc/blockchain.cpp |
| `getblockstats` | src/rpc/dataacq.cpp |
| `getburnreport` | src/rpc/blockchain.cpp |
| `getcheckpoint` | src/rpc/blockchain.cpp |
| `getlaststake` | src/rpc/mining.cpp |
| `getmininginfo` | src/rpc/mining.cpp |
| `getmpart` | src/gridcoin/scraper/scraper_net.cpp |
| `getmrcinfo` | src/rpc/blockchain.cpp |
| `getnewpubkey` | src/wallet/rpcwallet.cpp |
| `getpollresults` | src/rpc/voting.cpp |
| `getpsgtpoolinfo` | src/rpc/psgt.cpp |
| `getrawprojectstatus` | src/rpc/blockchain.cpp |
| `getrawwallettransaction` | src/wallet/rpcwallet.cpp |
| `getreceivedbyaccount` | src/wallet/rpcwallet.cpp |
| `getrecentblocks` | src/rpc/dataacq.cpp |
| `getstakinginfo` | src/rpc/mining.cpp |
| `getvotingclaim` | src/rpc/voting.cpp |
| `inspectaccrualsnapshot` | src/rpc/mining.cpp |
| `inspectwalletstate` | src/wallet/rpcwallet.cpp |
| `lifetime` | src/rpc/blockchain.cpp |
| `listaccounts` | src/wallet/rpcwallet.cpp |
| `listmandatorysidestakes` | src/rpc/blockchain.cpp |
| `listmanifests` | src/gridcoin/scraper/scraper_net.cpp |
| `listpolls` | src/rpc/voting.cpp |
| `listpools` | src/rpc/blockchain.cpp |
| `listprojects` | src/rpc/blockchain.cpp |
| `listprotocolentries` | src/rpc/blockchain.cpp |
| `listpsgtpool` | src/rpc/psgt.cpp |
| `listreceivedbyaccount` | src/wallet/rpcwallet.cpp |
| `listresearcheraccounts` | src/rpc/mining.cpp |
| `listscrapers` | src/rpc/blockchain.cpp |
| `listsettings` | src/rpc/misc.cpp |
| `listsidestakes` | src/rpc/blockchain.cpp |
| `liststakes` | src/wallet/rpcwallet.cpp |
| `magnitude` | src/rpc/blockchain.cpp |
| `maintainbackups` | src/wallet/rpcwallet.cpp |
| `makekeypair` | src/wallet/rpcwallet.cpp |
| `migratelabels` | src/wallet/rpcwallet.cpp |
| `move` | src/wallet/rpcwallet.cpp |
| `network` | src/rpc/blockchain.cpp |
| `networktime` | src/rpc/blockchain.cpp |
| `parseaccrualsnapshotfile` | src/rpc/mining.cpp |
| `parselegacysb` | src/rpc/blockchain.cpp |
| `pendingbeaconreport` | src/rpc/blockchain.cpp |
| `projects` | src/rpc/blockchain.cpp |
| `rainbymagnitude` | src/rpc/blockchain.cpp |
| `readdata` | src/rpc/blockchain.cpp |
| `refundhtlc` | src/rpc/htlc.cpp |
| `registerpool` | src/rpc/blockchain.cpp |
| `removepool` | src/rpc/blockchain.cpp |
| `removepsgtfrompool` | src/rpc/psgt.cpp |
| `reorganize` | src/rpc/blockchain.cpp |
| `repairwallet` | src/wallet/rpcwallet.cpp |
| `reservebalance` | src/wallet/rpcwallet.cpp |
| `resetcpids` | src/rpc/blockchain.cpp |
| `revokebeacon` | src/rpc/blockchain.cpp |
| `savescraperfilemanifest` | src/gridcoin/scraper/scraper.cpp |
| `scanforunspent` | src/rpc/rawtransaction.cpp |
| `scraperreport` | src/gridcoin/scraper/scraper.cpp |
| `sendblock` | src/rpc/blockchain.cpp |
| `sendfrom` | src/wallet/rpcwallet.cpp |
| `sendscraperfilemanifest` | src/gridcoin/scraper/scraper.cpp |
| `setaccount` | src/wallet/rpcwallet.cpp |
| `showblock` | src/rpc/blockchain.cpp |
| `signpsgtinpool` | src/rpc/psgt.cpp |
| `stakelimit` | src/wallet/rpcwallet.cpp |
| `submitpsgt` | src/rpc/psgt.cpp |
| `superblockage` | src/rpc/blockchain.cpp |
| `superblockaverage` | src/rpc/blockchain.cpp |
| `superblocks` | src/rpc/blockchain.cpp |
| `testnewsb` | src/gridcoin/scraper/scraper.cpp |
| `testpollnotification` | src/rpc/voting.cpp |
| `validatepubkey` | src/wallet/rpcwallet.cpp |
| `versionreport` | src/rpc/blockchain.cpp |
| `vote` | src/rpc/voting.cpp |
| `votebyid` | src/rpc/voting.cpp |
| `votedetails` | src/rpc/voting.cpp |
| `walletdiagnose` | src/wallet/rpcwallet.cpp |
| `withdrawpool` | src/rpc/blockchain.cpp |
| `writedata` | src/rpc/blockchain.cpp |

## Name collisions -- pure-gridcoin, but a same-named upstream RPC exists (do NOT port)
These RPCs are genuinely Gridcoin (no upstream *analogue*), but their NAME collides with an unrelated/disjoint current upstream RPC. A backporter must NOT apply an upstream change of the same name here -- the functions are different.

| RPC | collides with unrelated upstream RPC |
|---|---|
| `generate` | upstream generate is a removed-RPC tombstone (directs callers to generatetoaddress); GRC's generate is a live PoS staking RPC -- same name, disjoint function. Do NOT port. |
| `getblockstats` | upstream getblockstats computes per-block statistics over a height/hash window; GRC's reports recent-stake stats by wallet/cpid -- same name, disjoint function. Do NOT port upstream changes here. |
| `getmininginfo` | upstream getmininginfo returns PoW mining info; GRC getmininginfo is a compatibility ALIAS of getstakinginfo (PoS staking info) -- same name, disjoint function. Do NOT port. |
