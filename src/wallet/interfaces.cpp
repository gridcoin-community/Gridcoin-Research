// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/wallet.h"

#include "gridcoin/backup.h"
#include "gridcoin/tx_message.h"
#include "interfaces/handler.h"
#include "key_io.h"
#include "main.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace interfaces {
namespace {

//! Build the value snapshot of one wallet output. The maturity flag needs
//! cs_main (GetBlocksToMaturity walks the chain), which every caller holds.
WalletOutput MakeWalletOutput(const CWalletTx& wtx, unsigned int n, int depth)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    WalletOutput output;
    output.outpoint = COutPoint(wtx.GetHash(), n);
    output.amount = wtx.vout[n].nValue;

    CTxDestination address;
    if (ExtractDestination(wtx.vout[n].scriptPubKey, address)) {
        output.address = EncodeDestination(address);
    }

    output.depth = depth;
    output.time = wtx.GetTxTime();
    output.immature = wtx.IsCoinStake() && wtx.GetBlocksToMaturity() > 0 && depth > 0;

    return output;
}

//! In-process Wallet implementation: thin wrappers over CWallet. Balance
//! queries lock internally (CWallet takes LOCK2(cs_main, cs_wallet) itself),
//! so callers never hold core locks. Signal bridges convert the wallet's own
//! signal payloads to value types before they cross the boundary.
class WalletImpl : public Wallet
{
public:
    explicit WalletImpl(CWallet* wallet) : m_wallet(wallet) {}

    int64_t getBalance() override { return m_wallet->GetBalance(); }

    int64_t getStake() override { return m_wallet->GetStake(); }

    int64_t getUnconfirmedBalance() override { return m_wallet->GetUnconfirmedBalance(); }

    int64_t getImmatureBalance() override { return m_wallet->GetImmatureBalance(); }

    bool tryGetBalances(WalletBalances& balances) override
    {
        // The Get*Balance() calls iterate the wallet's full mapWallet and
        // become very expensive on large wallets, so refresh-path callers
        // must be able to bow out cleanly when the core is holding the locks
        // (e.g. during a wallet rescan) instead of stalling the GUI thread.
        // The locks are held across all four scans so the snapshot is
        // coherent (the individual getters re-lock recursively).
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            return false;
        }
        TRY_LOCK(m_wallet->cs_wallet, lockWallet);
        if (!lockWallet) {
            return false;
        }

        balances.balance = m_wallet->GetBalance();
        balances.stake = m_wallet->GetStake();
        balances.unconfirmed_balance = m_wallet->GetUnconfirmedBalance();
        balances.immature_balance = m_wallet->GetImmatureBalance();

        return true;
    }

    int getNumTransactions() override
    {
        return WITH_LOCK(m_wallet->cs_wallet,
                         return static_cast<int>(m_wallet->mapWallet.size()));
    }

    bool isCrypted() override { return m_wallet->IsCrypted(); }

    bool isLocked() override { return m_wallet->IsLocked(); }

    bool isUnlockedForStakingOnly() override
    {
        return !m_wallet->IsLocked() && fWalletUnlockStakingOnly;
    }

    bool getUnlockStakingOnlyFlag() override { return fWalletUnlockStakingOnly; }

    bool encryptWallet(const SecureString& passphrase) override
    {
        return m_wallet->EncryptWallet(passphrase);
    }

    bool lockWallet() override { return m_wallet->Lock(); }

    bool unlockWallet(const SecureString& passphrase, bool staking_only) override
    {
        if (!m_wallet->Unlock(passphrase)) {
            return false;
        }

        fWalletUnlockStakingOnly = staking_only;

        return true;
    }

    bool changeWalletPassphrase(const SecureString& old_passphrase,
                                const SecureString& new_passphrase) override
    {
        LOCK(m_wallet->cs_wallet);
        m_wallet->Lock(); // Make sure wallet is locked before attempting pass change
        return m_wallet->ChangeWalletPassphrase(old_passphrase, new_passphrase);
    }

    bool backupWallet(const std::string& dest) override
    {
        return GRC::BackupWallet(*m_wallet, dest);
    }

    bool backupConfigFile(const std::string& dest) override
    {
        return GRC::BackupConfigFile(dest);
    }

    bool getPubKey(const CKeyID& address, CPubKey& pub_key_out) override
    {
        return m_wallet->GetPubKey(address, pub_key_out);
    }

    bool getKeyFromPool(CPubKey& pub_key_out, const std::string& label) override
    {
        if (!m_wallet->GetKeyFromPool(pub_key_out, false)) {
            return false;
        }

        if (!label.empty()) {
            m_wallet->SetAddressBookName(pub_key_out.GetID(), label);
        }

        return true;
    }

    std::vector<WalletOutput> getOutputs(const std::vector<COutPoint>& outpoints) override
    {
        std::vector<WalletOutput> outputs;

        LOCK2(cs_main, m_wallet->cs_wallet);

        for (const COutPoint& outpoint : outpoints) {
            auto it = m_wallet->mapWallet.find(outpoint.hash);
            if (it == m_wallet->mapWallet.end()) continue;
            if (outpoint.n >= it->second.vout.size()) continue;
            int depth = it->second.GetDepthInMainChain();
            if (depth < 0) continue;
            outputs.push_back(MakeWalletOutput(it->second, outpoint.n, depth));
        }

        return outputs;
    }

    std::map<std::string, std::vector<WalletOutput>> listCoins() override
    {
        // The locks are taken BEFORE the AvailableCoins scan (which re-locks
        // recursively) and held across the loop below: COutput carries raw
        // pointers into mapWallet, and an unlocked window between the scan
        // and the dereferences would let a concurrent erasure (e.g. the
        // mempool-conflict EraseFromWallet path) free the pointed-at
        // transactions. The old GUI-side code scanned first and locked
        // after; the migration closes that window.
        std::map<std::string, std::vector<WalletOutput>> coins;

        LOCK2(cs_main, m_wallet->cs_wallet); // mapWallet

        std::vector<COutput> vCoins;
        m_wallet->AvailableCoins(vCoins, true, nullptr, false);

        for (const COutput& out : vCoins) {
            // Group change under the address it derives from by walking back
            // through own-wallet inputs (put change in one group with the
            // wallet address).
            COutput cout = out;

            while (m_wallet->IsChange(cout.tx->vout[cout.i]) && cout.tx->vin.size() > 0
                   && m_wallet->IsMine(cout.tx->vin[0]) != ISMINE_NO)
            {
                auto it = m_wallet->mapWallet.find(cout.tx->vin[0].prevout.hash);
                if (it == m_wallet->mapWallet.end()) break;
                cout = COutput(&it->second, cout.tx->vin[0].prevout.n, 0);
            }

            WalletOutput output = MakeWalletOutput(*out.tx, out.i, out.nDepth);

            // Group key: the walked ancestor's address. When the change-walk
            // stayed on the original output (the common case), reuse the
            // snapshot's already-encoded address instead of extracting and
            // base58-encoding the same destination a second time under the
            // locks.
            std::string group_key;
            if (cout.tx == out.tx && cout.i == out.i) {
                group_key = output.address;
            } else {
                CTxDestination address;
                if (ExtractDestination(cout.tx->vout[cout.i].scriptPubKey, address)) {
                    group_key = EncodeDestination(address);
                }
            }
            if (group_key.empty()) continue;

            coins[std::move(group_key)].push_back(std::move(output));
        }

        return coins;
    }

    CoinControlSummary computeCoinControlSummary(const WalletCoinControl& selection,
                                                 const std::vector<int64_t>& recipient_amounts,
                                                 bool subtract_fee_from_amount) override
    {
        CoinControlSummary summary;

        // Recipient side: pay amount, low-output flag, and the dummy outputs
        // GetMinFee's dust check inspects. Mirrors the pre-migration
        // updateLabels() exactly (the dummy script content is irrelevant to
        // the fee; only the count and nValue matter).
        CAmount pay_amount = 0;
        CMutableTransaction tx_dummy;
        for (const int64_t amount : recipient_amounts) {
            pay_amount += amount;
            if (amount > 0) {
                if (amount < CENT) {
                    summary.low_output = true;
                }
                tx_dummy.vout.push_back(CTxOut(amount, (CScript)std::vector<unsigned char>(24, 0)));
            }
        }

        CAmount amount = 0;
        unsigned int bytes_inputs = 0;
        unsigned int quantity = 0;

        LOCK2(cs_main, m_wallet->cs_wallet);

        for (const COutPoint& outpoint : selection.selected) {
            auto it = m_wallet->mapWallet.find(outpoint.hash);
            if (it == m_wallet->mapWallet.end()) continue;
            if (outpoint.n >= it->second.vout.size()) continue;
            if (it->second.GetDepthInMainChain() < 0) continue; // skip vanished/conflicted

            quantity++;
            amount += it->second.vout[outpoint.n].nValue;

            // Byte estimate per input from pubkey compression (148 compressed
            // else 180; 148 in every error/non-pubkeyhash case), matching the
            // former GUI-side getOutputs()+getPubKey() loop.
            CTxDestination address;
            if (ExtractDestination(it->second.vout[outpoint.n].scriptPubKey, address)) {
                const CKeyID* key_id = std::get_if<CKeyID>(&address);
                CPubKey pubkey;
                if (key_id && m_wallet->GetPubKey(*key_id, pubkey)) {
                    bytes_inputs += (pubkey.IsCompressed() ? 148 : 180);
                } else {
                    bytes_inputs += 148;
                }
            } else {
                bytes_inputs += 148;
            }
        }

        summary.quantity = static_cast<int>(quantity);
        summary.amount = amount;

        if (quantity > 0) {
            // Always assume +1 output for change here.
            summary.bytes = bytes_inputs
                + ((recipient_amounts.size() > 0 ? recipient_amounts.size() + 1 : 2) * 34) + 10;

            const CAmount fee = nTransactionFee * (1 + (int64_t)summary.bytes / 1000);
            const CAmount min_fee = GetMinFee(CTransaction(tx_dummy), 1000, GMF_SEND, summary.bytes);
            summary.fee = std::max(fee, min_fee);

            if (pay_amount > 0) {
                // When subtracting fee from amount, the fee is absorbed by the
                // recipients rather than coming from the change output.
                summary.change = subtract_fee_from_amount
                    ? amount - pay_amount
                    : amount - summary.fee - pay_amount;

                // A sub-cent change output isn't worth creating: absorb it into
                // the fee. The fee INCREASES by the change removed (fee + change),
                // matching CreateTransaction's behaviour -- the pre-migration GUI
                // code set fee = change here, dropping the already-computed
                // sub-cent base fee and under-reporting the fee by that amount.
                if (summary.fee < CENT && summary.change > 0 && summary.change < CENT) {
                    summary.fee += summary.change;
                    summary.change = 0;
                }

                if (summary.change == 0) {
                    summary.bytes -= 34;
                }
            }

            summary.after_fee = amount - summary.fee;
            if (summary.after_fee < 0) {
                summary.after_fee = 0;
            }
        }

        return summary;
    }

    unsigned int getMaxConsolidationInputs() override { return GetMaxInputsForConsolidationTxn(); }

    SendCoinsResult sendCoins(const std::vector<WalletSendRecipient>& recipients,
                              const std::optional<WalletCoinControl>& coin_control,
                              int64_t accepted_fee) override;

    std::unique_ptr<Handler> handleStatusChanged(StatusChangedFn fn) override
    {
        return MakeSignalHandler(m_wallet->NotifyStatusChanged.connect(
            [fn = std::move(fn)](CCryptoKeyStore*) { fn(); }));
    }

    std::unique_ptr<Handler> handleAddressBookChanged(AddressBookChangedFn fn) override
    {
        return MakeSignalHandler(m_wallet->NotifyAddressBookChanged.connect(
            [fn = std::move(fn)](CWallet*,
                                 const CTxDestination& address,
                                 const std::string& label,
                                 bool is_mine,
                                 const std::string& purpose,
                                 ChangeType status) {
                fn(EncodeDestination(address), label, is_mine, purpose, status);
            }));
    }

    std::unique_ptr<Handler> handleTransactionChanged(TransactionChangedFn fn) override
    {
        return MakeSignalHandler(m_wallet->NotifyTransactionChanged.connect(
            [fn = std::move(fn)](CWallet*, const uint256& tx_hash, ChangeType status) {
                fn(tx_hash, status);
            }));
    }

private:
    CWallet* m_wallet;
};

SendCoinsResult WalletImpl::sendCoins(const std::vector<WalletSendRecipient>& recipients,
                                      const std::optional<WalletCoinControl>& coin_control,
                                      int64_t accepted_fee)
{
    // Reconstruct the wallet-side CCoinControl from the boundary value type
    // (the raw class does not cross the interface).
    CCoinControl ctrl;
    const CCoinControl* coin_control_ptr = nullptr;
    if (coin_control) {
        if (!coin_control->dest_change.empty()) {
            ctrl.destChange = DecodeDestination(coin_control->dest_change);
        }
        ctrl.fAllowWatchOnly = coin_control->allow_watch_only;
        for (const COutPoint& outpoint : coin_control->selected) {
            ctrl.Select(outpoint);
        }
        coin_control_ptr = &ctrl;
    }

    // Defensive guards at the trust boundary. The GUI pre-checks both
    // conditions, but the node side must not rely on the client: an empty
    // recipient list would index recipients[0] below, and an undecodable
    // address becomes CNoDestination, which CScript::SetDestination renders
    // as an EMPTY script — CreateTransaction would fund that
    // anyone-can-spend output.
    if (recipients.empty()) {
        return {SendCoinsStatus::TransactionCreationFailed};
    }

    for (const WalletSendRecipient& rcp : recipients) {
        if (!IsValidDestination(DecodeDestination(rcp.address))) {
            return {SendCoinsStatus::InvalidAddress};
        }
    }

    // Amounts likewise: a non-positive or out-of-range amount must not reach
    // the balance arithmetic below. Checking MoneyRange on the running total
    // after every add keeps the arithmetic itself defined: the total is at
    // most MAX_MONEY before an add and each addend is at most MAX_MONEY, so
    // no intermediate sum can approach INT64_MAX.
    int64_t total = 0;
    for (const WalletSendRecipient& rcp : recipients) {
        if (rcp.amount <= 0 || !MoneyRange(rcp.amount)) {
            return {SendCoinsStatus::InvalidAmount};
        }
        total += rcp.amount;
        if (!MoneyRange(total)) {
            return {SendCoinsStatus::InvalidAmount};
        }
    }

    // Locked across scan AND dereference: COutput carries raw pointers into
    // mapWallet, and the old GUI-side code's unlocked window between
    // AvailableCoins returning and this loop reading out.tx let a concurrent
    // wallet-transaction erasure free the pointed-at CWalletTx.
    int64_t nBalance = 0;
    {
        LOCK2(cs_main, m_wallet->cs_wallet);

        std::vector<COutput> vCoins;
        m_wallet->AvailableCoins(vCoins, true, coin_control_ptr, false);

        for (auto const& out : vCoins)
            nBalance += out.tx->vout[out.i].nValue;
    }

    bool fAnySubtractFeeFromAmount = false;
    for (const WalletSendRecipient& rcp : recipients)
    {
        if (rcp.subtract_fee_from_amount)
        {
            fAnySubtractFeeFromAmount = true;
            break;
        }
    }

    if (total > nBalance)
    {
        return {SendCoinsStatus::AmountExceedsBalance};
    }

    if (!fAnySubtractFeeFromAmount && (total + nTransactionFee) > nBalance)
    {
        return {SendCoinsStatus::AmountWithFeeExceedsBalance, nTransactionFee};
    }

    CWalletTx wtx;

    if (fAnySubtractFeeFromAmount)
    {
        wtx.mapValue["subtractFeeFromAmount"] = "1";
    }

    if (!recipients[0].message.empty())
    {
        CMutableTransaction mtx;
        mtx.vContracts.emplace_back(GRC::MakeContract<GRC::TxMessage>(
            GRC::ContractAction::ADD,
            recipients[0].message));
        static_cast<CTransaction&>(wtx) = CTransaction(std::move(mtx));
    }

    {
        LOCK2(cs_main, m_wallet->cs_wallet);

        // Sendmany
        std::vector<std::pair<CScript, int64_t>> vecSend;
        for (const WalletSendRecipient& rcp : recipients) {
            CScript scriptPubKey;
            scriptPubKey.SetDestination(DecodeDestination(rcp.address));
            vecSend.push_back(std::make_pair(scriptPubKey, rcp.amount));
        }

        CReserveKey keyChange(m_wallet);
        int64_t nFeeRequired = 0;
        bool fCreated = m_wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, coin_control_ptr);

        // If any recipient has "subtract fee from amount" enabled, rebuild
        // the outputs with the fee deducted and create the transaction
        // again. This runs even if the first pass failed (e.g. sending
        // entire balance), since CWallet::CreateTransaction overwrites the
        // caller's nFeeRet to nTransactionFee on entry (wallet.cpp:2964)
        // and bumps from there — so even a failed pass leaves a reasonable
        // starting estimate.
        //
        // The loop refines the subtracted fee against the fee the wallet
        // actually charges. A single retry is not enough because the
        // wallet's own internal fee-bumping — primarily byte-tier
        // crossing where nPayFee = nTransactionFee * (1 + nBytes/1000)
        // jumps when the signed tx crosses each 1 KB boundary
        // (wallet.cpp:3191) — can return an nFeeRequired that exceeds
        // what we subtracted. Committing at that point under-debits the
        // recipient and silently absorbs the surplus into the sender's
        // change (issue #2981). Sub-CENT change handling
        // (wallet.cpp:3064-3078) is a second potential fee-bumper but
        // is dormant under default fee parameters because its trigger
        // `nFeeRet < GetBaseFee` is false when nFeeRet is seeded from
        // the default nTransactionFee.
        //
        // We require strict equality (subtracted == returned) for
        // convergence. The earlier `<=` form let the inverse case
        // (returned < subtracted) commit silently, over-debiting the
        // recipient and "saving" the difference back to the sender's
        // change.
        //
        // In a uniform-UTXO wallet, the loop can enter a 2-cycle: when
        // the higher fee is subtracted, the target shrinks and the wallet
        // picks fewer inputs (size drops below 1 KB → tier-1 fee, e.g.
        // 0.001); when the lower fee is subtracted, the target grows and
        // the wallet picks more inputs (size crosses 1 KB → tier-2 fee,
        // 0.002). The two states map to each other and strict equality
        // never fires. Constructable: ten 400.000-GRC UTXOs, send 2400.001
        // with subtract-fee — the loop alternates 0.001 / 0.002
        // indefinitely.
        //
        // To force a deterministic, convergent commit in such a case,
        // track the largest fee observed during the loop and the input
        // set that produced it. If the loop exits without strict
        // convergence — whether from oscillation or from a slower
        // non-converging case hitting the 10-attempt cap — pin coin
        // selection to that input set via CCoinControl and re-create
        // the transaction with the larger fee subtracted. With inputs
        // pinned, SelectCoins returns exactly those outpoints
        // (wallet.cpp:2750-2758), the transaction size is fixed, the
        // wallet's computed fee matches our subtraction, and the commit
        // converges. The sender pays exactly the entered amount; the
        // recipient receives (entered − higher fee).
        if (fAnySubtractFeeFromAmount)
        {
            int nSubtractRecipients = 0;
            for (const WalletSendRecipient& rcp : recipients)
            {
                if (rcp.subtract_fee_from_amount) ++nSubtractRecipients;
            }

            // Rebuild vecSend with the given fee distributed across the
            // subtract-fee recipients. Returns false if any opted-in
            // recipient's amount would drop to zero or negative; the
            // caller should respond with FeeExceedsSubtractedAmount.
            auto BuildSubtractedVecSend = [&](int64_t nFee) -> bool {
                vecSend.clear();
                int64_t nFeeRemainder = nFee % nSubtractRecipients;
                bool fFirst = true;
                for (const WalletSendRecipient& rcp : recipients)
                {
                    CScript scriptPubKey;
                    scriptPubKey.SetDestination(DecodeDestination(rcp.address));
                    int64_t nAmount = rcp.amount;

                    if (rcp.subtract_fee_from_amount)
                    {
                        nAmount -= nFee / nSubtractRecipients;
                        // First opted-in recipient absorbs the truncation remainder
                        if (fFirst)
                        {
                            nAmount -= nFeeRemainder;
                            fFirst = false;
                        }
                        if (nAmount <= 0)
                            return false;
                    }

                    vecSend.push_back(std::make_pair(scriptPubKey, nAmount));
                }
                return true;
            };

            // Track the largest fee returned and the input set that
            // produced it. Seed from pass 1 only if it succeeded —
            // seeding nMaxFeeSeen from a *failed* pass-1 call would
            // leave a phantom high fee with no corresponding snapshot
            // (the wallet sets nFeeRet = nTransactionFee on entry and
            // can bump it before returning false), which would then
            // block subsequent successful iterations with lower fees
            // from populating vinsAtMaxFee via the `>` comparison.
            // Inside the loop, snapshot on the first success regardless
            // of fee value (vinsAtMaxFee.empty()) so we always have a
            // valid input set to pin if the rescue is needed.
            int64_t nMaxFeeSeen = 0;
            std::vector<COutPoint> vinsAtMaxFee;
            if (fCreated)
            {
                nMaxFeeSeen = nFeeRequired;
                for (const CTxIn& in : wtx.vin)
                    vinsAtMaxFee.push_back(in.prevout);
            }
            bool fConverged = false;

            for (int nAttempt = 0; nAttempt < 10; ++nAttempt)
            {
                if (!BuildSubtractedVecSend(nFeeRequired))
                    return {SendCoinsStatus::FeeExceedsSubtractedAmount, nFeeRequired};

                int64_t nFeePrev = nFeeRequired;
                fCreated = m_wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, coin_control_ptr);

                if (fCreated && nFeeRequired == nFeePrev)
                {
                    fConverged = true;
                    break;
                }

                if (fCreated && (vinsAtMaxFee.empty() || nFeeRequired > nMaxFeeSeen))
                {
                    nMaxFeeSeen = nFeeRequired;
                    vinsAtMaxFee.clear();
                    for (const CTxIn& in : wtx.vin)
                        vinsAtMaxFee.push_back(in.prevout);
                }
            }

            // Rescue pass for oscillation or cap-without-convergence.
            // Requires a non-empty snapshot — if every prior pass failed
            // we have no input set to pin and we fall through to the
            // TransactionCreationFailed return below.
            //
            // Pinning the inputs locks transaction size against
            // SelectCoins-driven input-count flipping. Under default
            // Gridcoin fee parameters and reasonable UTXO shapes the
            // wallet returns the same fee on the rescue call that
            // produced the snapshot — fee-tier transitions happen at
            // the 1 KB byte boundary, and the only remaining structural
            // variation (change-vout present/absent, ±34 bytes) plus
            // per-signature DER length variation (1-2 bytes per input)
            // are not enough to cross 1 KB at typical input counts:
            // 6 pinned inputs span 936-970 bytes (tier 1×), 7 pinned
            // span 1084-1118 bytes (tier 2×). So in practice the rescue
            // converges in iter 0.
            //
            // The bounded loop is defensive against pathologies I can
            // describe but cannot construct concretely: keypool
            // exhaustion mid-rescue (CreateTransaction returns false on
            // reservekey.GetReservedKey), an unusually low custom
            // -paytxfee combined with an adversarial UTXO sum that
            // re-activates the sub-CENT change handler at
            // wallet.cpp:3064-3078 (which is dormant under defaults
            // because the trigger nFeeRet < GetBaseFee is false once
            // nFeeRet equals the default nTransactionFee), or a future
            // change to wallet internals that introduces new
            // fee-determining factors. The strict-equality convergence
            // check matches the outer loop; on non-convergence we set
            // fCreated = false so the caller returns
            // TransactionCreationFailed rather than commit a tx whose
            // subtract-fee accounting doesn't match the wallet's actual
            // charge.
            if (!fConverged && !vinsAtMaxFee.empty())
            {
                // Copy any caller-supplied options (destChange,
                // fAllowWatchOnly) and add the pinned outpoints. CCoinControl
                // uses default copy semantics; setSelected, destChange, and
                // fAllowWatchOnly are all carried over.
                CCoinControl pinControl;
                if (coin_control_ptr) pinControl = *coin_control_ptr;
                for (const COutPoint& op : vinsAtMaxFee)
                    pinControl.Select(op);

                int64_t nRescueFee = nMaxFeeSeen;
                bool fRescueConverged = false;
                for (int nRescueAttempt = 0; nRescueAttempt < 5; ++nRescueAttempt)
                {
                    if (!BuildSubtractedVecSend(nRescueFee))
                        return {SendCoinsStatus::FeeExceedsSubtractedAmount, nRescueFee};

                    int64_t nRescuePrev = nRescueFee;
                    nFeeRequired = 0;
                    fCreated = m_wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, &pinControl);
                    if (!fCreated)
                        break;

                    if (nFeeRequired == nRescuePrev)
                    {
                        fRescueConverged = true;
                        break;
                    }
                    nRescueFee = nFeeRequired;
                }

                // Non-converging rescue: fall through to the
                // TransactionCreationFailed return below rather than
                // commit a tx whose subtract-fee accounting doesn't
                // match the wallet's actual charge.
                if (!fRescueConverged)
                    fCreated = false;
            }
        }

        if (!fCreated)
        {
            if (!fAnySubtractFeeFromAmount && (total + nFeeRequired) > nBalance)
            {
                return {SendCoinsStatus::AmountWithFeeExceedsBalance, nFeeRequired};
            }
            return {SendCoinsStatus::TransactionCreationFailed};
        }

        // Fee-confirmation gate, replacing the ThreadSafeAskFee modal that
        // used to block right here — inside LOCK2, across arbitrary user
        // think-time (doc/multiprocess_design.md section 4.5). Same
        // thresholds the old GUI handler applied: auto-accept a fee below
        // the base fee or within the configured transaction fee. A larger
        // fee needs the user's explicit acceptance: unless the caller
        // already accepted at least this much, return without committing —
        // this scope's locks release and keyChange's destructor returns the
        // reserved key to the pool. The caller re-invokes with the accepted
        // fee, the transaction is recreated from current wallet state, and
        // any fee that meanwhile grew past the accepted amount lands back
        // here, so no unaccepted fee is ever committed.
        // Threshold on the actual transaction rather than the dummy the old
        // GUI handler used (it had no wtx in scope; GetBaseFee is
        // version-sensitive, and today wtx carries the same default version
        // as a fresh CMutableTransaction, so this is behavior-identical).
        int64_t nMinFee = GetBaseFee(wtx, GMF_SEND);
        if (nFeeRequired >= nMinFee && nFeeRequired > nTransactionFee && nFeeRequired > accepted_fee)
        {
            return {SendCoinsStatus::FeeConfirmationRequired, nFeeRequired};
        }

        if (!m_wallet->CommitTransaction(wtx, keyChange))
        {
            return {SendCoinsStatus::TransactionCommitFailed};
        }
    }

    // Add addresses / update labels that we've sent to the address book
    for (const WalletSendRecipient& rcp : recipients) {
        CTxDestination dest = DecodeDestination(rcp.address);
        {
            LOCK(m_wallet->cs_wallet);

            auto mi = m_wallet->mapAddressBook.find(dest);

            // Check if we have a new address or an updated label
            if (mi == m_wallet->mapAddressBook.end() || mi->second.name != rcp.label)
            {
                m_wallet->SetAddressBookName(dest, rcp.label);
            }
        }
    }

    SendCoinsResult result;
    result.status = SendCoinsStatus::OK;
    result.txid_hex = wtx.GetHash().GetHex();

    return result;
}

} // namespace

std::unique_ptr<Wallet> MakeWallet(CWallet* wallet)
{
    return std::make_unique<WalletImpl>(wallet);
}

} // namespace interfaces
