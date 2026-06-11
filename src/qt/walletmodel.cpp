#include "walletmodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactionrecord.h"
#include "transactiontablemodel.h"

#include "node/ui_interface.h"
#include "wallet/wallet.h"
#include "wallet/coincontrol.h"
#include "main.h"
#include <key_io.h>
#include "util.h"
#include "gridcoin/tx_message.h"

#include <QSet>
#include <QTimer>

void qtInsertConfirm(double dAmt, std::string sFrom, std::string sTo, std::string txid);

WalletModel::WalletModel(CWallet* wallet, OptionsModel* optionsModel, QObject* parent)
         : QObject(parent)
         , wallet(wallet)
         , optionsModel(optionsModel)
         , addressTableModel(nullptr)
         , transactionTableModel(nullptr)
         , cachedBalance(0)
         , cachedStake(0)
         , cachedUnconfirmedBalance(0)
         , cachedImmatureBalance(0)
         , cachedNumTransactions(0)
         , cachedEncryptionStatus(Unencrypted)
         , cachedNumBlocks(0)
         , m_txStore(wallet, m_event_queue)
{
    addressTableModel = new AddressTableModel(wallet, this);
    // TransactionTableModel's ctor performs the initial load via
    // m_txStore.reloadAndSnapshot(); m_txStore is already constructed (init
    // list) and no producer can run yet — subscribeToCoreSignals() is called
    // below, after the model exists.
    transactionTableModel = new TransactionTableModel(wallet, this);

    // Drain the producer→GUI event queue at a steady cadence. 500ms is
    // imperceptible for transaction-list updates while still giving the
    // queue room to absorb bursts (e.g. a reorg flood) without per-event
    // round-trips to the Qt event loop. This single timer also drives the
    // balance / row-confirmation refresh that used to be done by a
    // separate 4-second pollBalanceChanged timer; refresh now fires off
    // ChainTipChanged events pushed by the producer-side subscriber to
    // uiInterface.NotifyBlocksChanged.
    eventDrainTimer = new QTimer(this);
    connect(eventDrainTimer, &QTimer::timeout, this, &WalletModel::drainEventQueue);
    eventDrainTimer->start(MODEL_EVENT_DRAIN_INTERVAL);

    // Launch the store-worker now — before producers can fire (subscribe is
    // below) — so it is ready to drain the intake queue off the core locks
    // (PR2.5). The initial reloadAndSnapshot in the TransactionTableModel ctor
    // above ran with no worker yet; that is fine, it skips the worker barrier
    // when the worker has not started.
    m_txStore.start();

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}

qint64 WalletModel::getBalance() const
{
    return wallet->GetBalance();
}

qint64 WalletModel::getUnconfirmedBalance() const
{
    return wallet->GetUnconfirmedBalance();
}

qint64 WalletModel::getStake() const
{
    return wallet->GetStake();
}

qint64 WalletModel::getImmatureBalance() const
{
    return wallet->GetImmatureBalance();
}

int WalletModel::getNumTransactions() const
{
    int numTransactions = 0;
    {
        LOCK(wallet->cs_wallet);
        numTransactions = wallet->mapWallet.size();
    }
    return numTransactions;
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedEncryptionStatus != newEncryptionStatus)
        emit encryptionStatusChanged(newEncryptionStatus);
}

void WalletModel::checkBalanceChanged()
{
    // The Get*Balance() calls iterate the wallet's full mapWallet and become
    // INCREDIBLY expensive on large wallets. Two layers of protection:
    //
    //  1. TRY_LOCK on cs_main + cs_wallet: bow out cleanly if the core is
    //     holding them (e.g. during a wallet rescan). This is the same
    //     guard pollBalanceChanged used to apply.
    //
    //  2. A MODEL_UPDATE_DELAY (4s) stale-time gate: even when the locks
    //     are available, only actually recompute at most once per gate
    //     interval. Bursts of rapid-fire wallet events during a resync,
    //     rescan, or large consolidation collapse into a single recompute.
    //
    // The last call in a burst that fails the stale-time test isn't lost:
    // the next ChainTipChanged event (or the next drain pass with events
    // in it) re-runs this function, which by then will pass the gate.
    TRY_LOCK(cs_main, lockMain);
    if (!lockMain) {
        return;
    }
    TRY_LOCK(wallet->cs_wallet, lockWallet);
    if (!lockWallet) {
        return;
    }

    int64_t current_time = GetAdjustedTime();

    if (current_time - last_balance_update_time > MODEL_UPDATE_DELAY / 1000)
    {
        // Stamp the gate as soon as we commit to a recompute — NOT only when
        // a change is detected. The gate exists to rate-limit the expensive
        // Get*Balance() scans themselves; if the timestamp only advanced on a
        // detected change, a long-stable balance would leave the gate
        // permanently open and every drain tick (which can fire back-to-back
        // when drainEventQueue re-arms to clear a backlog) would run a fresh
        // full-wallet scan.
        last_balance_update_time = current_time;

        qint64 newBalance = getBalance();
        qint64 newStake = getStake();
        qint64 newUnconfirmedBalance = getUnconfirmedBalance();
        qint64 newImmatureBalance = getImmatureBalance();

        if (cachedBalance != newBalance
                || cachedStake != newStake
                || cachedUnconfirmedBalance != newUnconfirmedBalance
                || cachedImmatureBalance != newImmatureBalance)
        {
            cachedBalance = newBalance;
            cachedStake = newStake;
            cachedUnconfirmedBalance = newUnconfirmedBalance;
            cachedImmatureBalance = newImmatureBalance;

            emit balanceChanged(newBalance, newStake, newUnconfirmedBalance, newImmatureBalance);
        }
    }
}

void WalletModel::drainEventQueue()
{
    // Bound the per-tick batch so a large backlog (reorg flood, IBD catch-up)
    // cannot freeze the Qt main thread in a single apply pass. If the queue
    // still has events after this batch, re-arm immediately (see below)
    // instead of waiting MODEL_EVENT_DRAIN_INTERVAL for the periodic tick.
    auto events = m_event_queue.drain(MODEL_EVENT_DRAIN_MAX_BATCH);
    if (events.empty()) {
        return;
    }

    LogPrint(BCLog::LogFlags::VERBOSE,
             "WalletModel::drainEventQueue: applying %u events (front seqno=%llu, back seqno=%llu)",
             static_cast<unsigned int>(events.size()),
             static_cast<unsigned long long>(events.front().seqno),
             static_cast<unsigned long long>(events.back().seqno));

    // Detect a chain-tip advance in the batch so the consumer-side
    // post-processing (per-row confirmation refresh, balance recompute) runs
    // only when a block actually moved — not on every wallet-tx burst within
    // a single block.
    bool chain_tip_advanced = false;
    for (const auto& ev : events) {
        if (std::holds_alternative<GRC::ChainTipChangedPayload>(ev.payload)) {
            chain_tip_advanced = true;
            break;
        }
    }

    if (transactionTableModel) {
        transactionTableModel->applyEventBatch(events);

        // Note this is subtly different than the below. If a resync is being
        // done on a wallet that already has transactions, the
        // numTransactionsChanged will not be emitted after the wallet is
        // loaded because the size() does not change. See the comments in the
        // header file.
        emit transactionUpdated();

        // Equivalent of the work pollBalanceChanged used to do when it
        // observed nBestHeight != cachedNumBlocks: refresh per-row
        // confirmation status. Driven by the event payload now.
        if (chain_tip_advanced) {
            transactionTableModel->updateConfirmations();
        }
    }

    // Fan the same batch out to the per-view windowed consumers (OverviewTxModel),
    // which filter to their own viewId. The queue is drained exactly once, here.
    emit walletEventsDrained(events);

    // Balance and number of transactions might have changed.
    checkBalanceChanged();

    int newNumTransactions = getNumTransactions();
    if (cachedNumTransactions != newNumTransactions) {
        cachedNumTransactions = newNumTransactions;

        emit numTransactionsChanged(newNumTransactions);
    }

    // If this drain hit the per-tick batch cap there is still a backlog.
    // Re-arm immediately (0ms) rather than waiting for the next periodic
    // tick: this returns control to the Qt event loop — keeping the GUI
    // responsive between batches — but resumes draining straight away so a
    // burst clears in a few event-loop turns instead of one per 500ms.
    if (events.size() >= static_cast<std::size_t>(MODEL_EVENT_DRAIN_MAX_BATCH)) {
        QTimer::singleShot(0, this, &WalletModel::drainEventQueue);
    }
}

void WalletModel::updateAddressBook(const QString &address, const QString &label, bool isMine, int status)
{
    if(addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, status);
}

bool WalletModel::validateAddress(const QString &address)
{
    CTxDestination addressParsed = DecodeDestination(address.toStdString());
    return IsValidDestination(addressParsed);
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(const QList<SendCoinsRecipient> &recipients, const CCoinControl *coinControl)
{
    qint64 total = 0;
    QSet<QString> setAddress;
    QString hex;

    if(recipients.empty())
    {
        return OK;
    }

    // Pre-check input data for validity
    for (const SendCoinsRecipient& rcp : recipients) {
        if(!validateAddress(rcp.address))
        {
            return InvalidAddress;
        }
        setAddress.insert(rcp.address);

        if(rcp.amount <= 0)
        {
            return InvalidAmount;
        }
        total += rcp.amount;
    }

    if(recipients.size() > setAddress.size())
    {
        return DuplicateAddress;
    }

    int64_t nBalance = 0;
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, true, coinControl,false);

    for (auto const& out : vCoins)
        nBalance += out.tx->vout[out.i].nValue;

    bool fAnySubtractFeeFromAmount = false;
    for (const SendCoinsRecipient& rcp : recipients)
    {
        if (rcp.fSubtractFeeFromAmount)
        {
            fAnySubtractFeeFromAmount = true;
            break;
        }
    }

    if(total > nBalance)
    {
        return AmountExceedsBalance;
    }

    if(!fAnySubtractFeeFromAmount && (total + nTransactionFee) > nBalance)
    {
        return SendCoinsReturn(AmountWithFeeExceedsBalance, nTransactionFee);
    }

    CWalletTx wtx;

    if (fAnySubtractFeeFromAmount)
    {
        wtx.mapValue["subtractFeeFromAmount"] = "1";
    }

    if (!recipients[0].Message.isEmpty())
    {
        CMutableTransaction mtx;
        mtx.vContracts.emplace_back(GRC::MakeContract<GRC::TxMessage>(
            GRC::ContractAction::ADD,
            recipients[0].Message.toStdString()));
        static_cast<CTransaction&>(wtx) = CTransaction(std::move(mtx));
    }

    {
        LOCK2(cs_main, wallet->cs_wallet);

        // Sendmany
        std::vector<std::pair<CScript, int64_t> > vecSend;
        for (const SendCoinsRecipient& rcp : recipients) {
            CScript scriptPubKey;
            scriptPubKey.SetDestination(DecodeDestination(rcp.address.toStdString()));
            vecSend.push_back(std::make_pair(scriptPubKey, rcp.amount));
        }

        CReserveKey keyChange(wallet);
        int64_t nFeeRequired = 0;
        bool fCreated = wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, coinControl);

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
            for (const SendCoinsRecipient& rcp : recipients)
            {
                if (rcp.fSubtractFeeFromAmount) ++nSubtractRecipients;
            }

            // Rebuild vecSend with the given fee distributed across the
            // subtract-fee recipients. Returns false if any opted-in
            // recipient's amount would drop to zero or negative; the
            // caller should respond with FeeExceedsSubtractedAmount.
            auto BuildSubtractedVecSend = [&](int64_t nFee) -> bool {
                vecSend.clear();
                int64_t nFeeRemainder = nFee % nSubtractRecipients;
                bool fFirst = true;
                for (const SendCoinsRecipient& rcp : recipients)
                {
                    CScript scriptPubKey;
                    scriptPubKey.SetDestination(DecodeDestination(rcp.address.toStdString()));
                    int64_t nAmount = rcp.amount;

                    if (rcp.fSubtractFeeFromAmount)
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
                    return SendCoinsReturn(FeeExceedsSubtractedAmount, nFeeRequired);

                int64_t nFeePrev = nFeeRequired;
                fCreated = wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, coinControl);

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
                if (coinControl) pinControl = *coinControl;
                for (const COutPoint& op : vinsAtMaxFee)
                    pinControl.Select(op);

                int64_t nRescueFee = nMaxFeeSeen;
                bool fRescueConverged = false;
                for (int nRescueAttempt = 0; nRescueAttempt < 5; ++nRescueAttempt)
                {
                    if (!BuildSubtractedVecSend(nRescueFee))
                        return SendCoinsReturn(FeeExceedsSubtractedAmount, nRescueFee);

                    int64_t nRescuePrev = nRescueFee;
                    nFeeRequired = 0;
                    fCreated = wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, &pinControl);
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

        if(!fCreated)
        {
            if(!fAnySubtractFeeFromAmount && (total + nFeeRequired) > nBalance)
            {
                return SendCoinsReturn(AmountWithFeeExceedsBalance, nFeeRequired);
            }
            return TransactionCreationFailed;
        }

        if(!uiInterface.ThreadSafeAskFee(nFeeRequired, tr("Sending...").toStdString()))
        {
            return Aborted;
        }
        if(!wallet->CommitTransaction(wtx, keyChange))
        {
            return TransactionCommitFailed;
        }
        hex = QString::fromStdString(wtx.GetHash().GetHex());
    }

    // Add addresses / update labels that we've sent to the address book
    for (const SendCoinsRecipient& rcp : recipients) {
        std::string strAddress = rcp.address.toStdString();
        CTxDestination dest = DecodeDestination(strAddress);
        std::string strLabel = rcp.label.toStdString();
        {
            LOCK(wallet->cs_wallet);

            std::map<CTxDestination, std::string>::iterator mi = wallet->mapAddressBook.find(dest);

            // Check if we have a new address or an updated label
            if (mi == wallet->mapAddressBook.end() || mi->second != strLabel)
            {
                wallet->SetAddressBookName(dest, strLabel);
            }
        }
    }

    return SendCoinsReturn(OK, 0, hex);
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if(!wallet->IsCrypted())
    {
        return Unencrypted;
    }
    else if(wallet->IsLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(const SecureString& passphrase)
{
        return wallet->EncryptWallet(passphrase);
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase)
{
    if(locked)
    {
        // Lock
        return wallet->Lock();
    }
    else
    {
        // Unlock
        return wallet->Unlock(passPhrase);
    }
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
    bool retval;
    {
        LOCK(wallet->cs_wallet);
        wallet->Lock(); // Make sure wallet is locked before attempting pass change
        retval = wallet->ChangeWalletPassphrase(oldPass, newPass);
    }
    return retval;
}

// Handlers for core signals
static void NotifyKeyStoreStatusChanged(WalletModel *walletmodel, CCryptoKeyStore *wallet)
{
    LogPrintf("NotifyKeyStoreStatusChanged");
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}

static void NotifyAddressBookChanged(WalletModel *walletmodel, CWallet *wallet, const CTxDestination &address, const std::string &label, bool isMine, ChangeType status)
{
    LogPrintf("NotifyAddressBookChanged %s %s isMine=%i status=%i", EncodeDestination(address), label, isMine, status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(EncodeDestination(address))),
                              Q_ARG(QString, QString::fromStdString(label)),
                              Q_ARG(bool, isMine),
                              Q_ARG(int, status));
}

static void NotifyTransactionChanged(WalletModel *walletmodel, CWallet *wallet, const uint256 &hash, ChangeType status)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, wallet->cs_wallet)
{
    LogPrint(BCLog::LogFlags::VERBOSE, "NotifyTransactionChanged %s status=%i", hash.GetHex(), status);

    // Producer-side push into the WalletEventQueue. CT_NEW and CT_UPDATED are
    // handled identically: the producer looks up the wtx, applies the
    // wtx-level visibility checks (orphan coinstake/coinbase / legacy
    // OP_RETURN — datetime filter is consumer-side), and pushes either a
    // TxAdded with decomposed records or a TxRemoved with just the hash.
    // The consumer's binary-search-by-hash insert path de-dupes a TxAdded
    // when the tx is already in cachedWallet, and the remove path no-ops
    // when the tx isn't.
    //
    // The unified CT_NEW/CT_UPDATED handling matters because the wallet
    // fires CT_UPDATED (not CT_NEW) when a tx already in mapWallet is
    // re-validated against a fresh chain — e.g. during an IBD that follows
    // a chainstate wipe but retains wallet.dat. If we only acted on CT_NEW,
    // the GUI's cachedWallet would never see those txs become visible
    // again. It also covers the steady-state case where a previously
    // filtered-out tx (e.g. an orphan coinstake) becomes valid: CT_UPDATED
    // fires, the producer's showTransaction now returns true, and the
    // consumer inserts the row. The reverse direction (tx falls out of
    // visibility) is covered by pushing TxRemoved when showTransaction
    // returns false.
    //
    // Lock state at this point:
    //   The CT_NEW / CT_UPDATED / CT_UPDATING branch needs BOTH cs_main and
    //   cs_wallet held:
    //     - cs_wallet — to look up mapWallet[hash] and run
    //       decomposeTransaction (which recursively calls IsMine()).
    //     - cs_main   — TransactionRecord::showTransaction() calls
    //       CWalletTx::IsInMainChain(), which is EXCLUSIVE_LOCKS_REQUIRED
    //       (cs_main). (The thread-safety analyzer does not flag this
    //       cross-TU because GetDepthInMainChain's annotation lives on the
    //       definition in main.cpp, not the header declaration — so the
    //       requirement is verified here by hand, not by the compiler.)
    //
    //   All five CT_NEW / CT_UPDATED callsites hold both locks, verified by
    //   audit:
    //     wallet.cpp:572  AddToWallet            — EXCLUSIVE_LOCKS_REQUIRED(cs_main); LOCK(cs_wallet)
    //     wallet.cpp:2400 CommitTransaction      — LOCK2(cs_main, cs_wallet)
    //     wallet.cpp:458  WalletUpdateSpent      — caller AddToWallet / AddToWalletIfInvolvingMe, both EXCLUSIVE_LOCKS_REQUIRED(cs_main); LOCK(cs_wallet)
    //     wallet.cpp:475  WalletUpdateSpent      — same
    //     wallet.cpp:3057 UpdatedTransaction     — reached via validation.cpp (cs_main required on entry); LOCK(cs_wallet)
    //   This function is annotated EXCLUSIVE_LOCKS_REQUIRED(cs_main,
    //   wallet->cs_wallet) so the Clang thread-safety analyzer accepts the
    //   AssertLockHeld() calls and the mapWallet reads in the CT_NEW /
    //   CT_UPDATED branch. The AssertLockHeld() calls additionally enforce
    //   the requirement at runtime in DEBUG_LOCKORDER builds.
    //
    //   CT_DELETED callsites (main.cpp:1290, wallet.cpp:1349) DO NOT hold
    //   either lock — the tx has already been erased. The TxRemoved payload
    //   carries only the hash, so no wallet lookup or showTransaction call
    //   is needed; that branch touches nothing the annotation guards. The
    //   EXCLUSIVE_LOCKS_REQUIRED annotation therefore over-claims for the
    //   CT_DELETED path — harmless, because the handler is invoked only
    //   through boost::signals2, which the analyzer cannot trace, so no
    //   caller is ever checked against the annotation; its sole effect is
    //   to satisfy the analyzer inside the CT_NEW / CT_UPDATED branch.
    switch (status) {
    case CT_NEW:
    case CT_UPDATED:
    case CT_UPDATING: {
        AssertLockHeld(cs_main);
        AssertLockHeld(wallet->cs_wallet);
        auto it = wallet->mapWallet.find(hash);
        if (it == wallet->mapWallet.end()) {
            // Tx isn't in mapWallet — only happens if the notification
            // raced with an erasure. Push TxRemoved to keep the consumer
            // in sync.
            LogPrint(BCLog::LogFlags::VERBOSE,
                     "NotifyTransactionChanged: %s status=%d but tx not in mapWallet "
                     "— removing from store",
                     hash.GetHex(), status);
            walletmodel->getTxStore().enqueueRemove(hash);
            break;
        }
        const CWalletTx& wtx = it->second;

        bool visible = TransactionRecord::showTransaction(wtx, false, 0);

        // showTransaction() hides a generated (coinstake/coinbase) tx whose
        // block is not yet in the main chain. This handler, however, runs
        // synchronously inside block connection: the wallet is notified of a
        // block's transactions before SetBestChain advances pindexBest (see
        // main.cpp), so the block being connected — and its own coinstake —
        // transiently read as orphan. That is a false negative: the block is
        // a split-second from becoming the tip, and without this guard its
        // coinstake gets a TxRemoved and never enters the GUI model.
        //
        // Detect exactly that window: a block sitting directly on the current
        // tip but not yet the tip itself is the one being connected right
        // now. A genuine orphan has pprev != pindexBest and stays hidden, so
        // -showorphans semantics are unchanged. For a current-era coinstake
        // the orphan check is showTransaction()'s only false path, so this
        // override cannot un-hide a tx filtered for any other reason.
        if (!visible && (wtx.IsCoinStake() || wtx.IsCoinBase())) {
            auto bi = mapBlockIndex.find(wtx.hashBlock);
            if (bi != mapBlockIndex.end() && bi->second != nullptr
                    && !bi->second->IsInMainChain()
                    && bi->second->pprev == pindexBest) {
                LogPrint(BCLog::LogFlags::VERBOSE,
                         "NotifyTransactionChanged: %s is in the block being "
                         "connected — keeping visible despite transient orphan state",
                         hash.GetHex());
                visible = true;
            }
        }

        if (visible) {
            // Decompose under the locks already held, compute per-row status
            // producer-side (updateStatus requires cs_main, held here), then
            // ENQUEUE to the store-worker (PR2.5). The worker applies the datetime
            // cutoff, de-dupes, computes positions, maintains the per-view cursors
            // and emits events off the core locks; the enqueue itself is O(1).
            // Status is computed here so the off-lock cursors can filter/sort by
            // it without re-touching the wallet.
            const QList<TransactionRecord> decomposed =
                TransactionRecord::decomposeTransaction(wallet, wtx);
            if (!decomposed.isEmpty()) {
                std::vector<TransactionRecord> recs(decomposed.begin(), decomposed.end());
                for (TransactionRecord& rec : recs) {
                    rec.updateStatus(wtx);
                    rec.populateDisplayLabel(*wallet);  // address-book label snapshot (PR4)
                }
                // CT_NEW is a fresh insert; CT_UPDATED / CT_UPDATING is an upsert
                // of an existing tx (e.g. a confirmation) — the store updates it in
                // place and repositions it in any status-sorted cursor.
                if (status == CT_NEW) {
                    walletmodel->getTxStore().enqueueInsert(std::move(recs));
                } else {
                    walletmodel->getTxStore().enqueueUpsert(std::move(recs));
                }
            }
        } else {
            // Tx is genuinely filtered out (a real orphan coinstake, or a
            // legacy non-IsFromMe OP_RETURN). Ensure the store removes the rows
            // if they were previously visible — a no-op if absent.
            walletmodel->getTxStore().enqueueRemove(hash);
        }
        break;
    }
    case CT_DELETED:
        walletmodel->getTxStore().enqueueRemove(hash);
        break;
    }
}

static void NotifyBlocksChangedForWallet(WalletModel *walletmodel,
                                         bool /*syncing*/,
                                         int height,
                                         int64_t best_time,
                                         uint32_t /*target_bits*/)
{
    // Fired from main.cpp::SetBestChain (under cs_main) after every chain
    // tip advance — connect, disconnect, or reorg. Pushes a lightweight
    // marker into the event queue. The Qt-side drain handler reacts by
    // refreshing per-row confirmation status and re-running the existing
    // (rate-limited) balance recompute path. This replaces the 4-second
    // pollBalanceChanged poll that used to compare nBestHeight to a cached
    // copy on a timer.
    walletmodel->getEventQueue().push(GRC::ChainTipChangedPayload{height, best_time});
}

void WalletModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    wallet->NotifyStatusChanged.connect(boost::bind(&NotifyKeyStoreStatusChanged, this,
                                                    boost::placeholders::_1));
    wallet->NotifyAddressBookChanged.connect(boost::bind(NotifyAddressBookChanged, this,
                                                         boost::placeholders::_1, boost::placeholders::_2,
                                                         boost::placeholders::_3, boost::placeholders::_4,
                                                         boost::placeholders::_5));
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this,
                                                         boost::placeholders::_1, boost::placeholders::_2,
                                                         boost::placeholders::_3));
    uiInterface.NotifyBlocksChanged_connect(boost::bind(NotifyBlocksChangedForWallet, this,
                                                       boost::placeholders::_1, boost::placeholders::_2,
                                                       boost::placeholders::_3, boost::placeholders::_4));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    wallet->NotifyStatusChanged.disconnect(boost::bind(&NotifyKeyStoreStatusChanged, this,
                                                       boost::placeholders::_1));
    wallet->NotifyAddressBookChanged.disconnect(boost::bind(NotifyAddressBookChanged, this,
                                                            boost::placeholders::_1, boost::placeholders::_2,
                                                            boost::placeholders::_3, boost::placeholders::_4,
                                                            boost::placeholders::_5));
    wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this,
                                                            boost::placeholders::_1, boost::placeholders::_2,
                                                            boost::placeholders::_3));
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;

    if ((!was_locked) && fWalletUnlockStakingOnly)
    {
       setWalletLocked(true);
       was_locked = getEncryptionStatus() == Locked;

    }
    if(was_locked)
    {
        // Request UI to unlock wallet
        emit requireUnlock();
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked && !fWalletUnlockStakingOnly);
}

WalletModel::UnlockContext::UnlockContext(WalletModel *wallet, bool valid, bool relock):
        wallet(wallet),
        valid(valid),
        relock(relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

bool WalletModel::getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    return wallet->GetPubKey(address, vchPubKeyOut);
}

bool WalletModel::getKeyFromPool(CPubKey& out_public_key, const std::string& label)
{
    if (!wallet->GetKeyFromPool(out_public_key, false)) {
        return false;
    }

    if (!label.empty()) {
        wallet->SetAddressBookName(out_public_key.GetID(), label);
    }

    return true;
}

// returns a list of COutputs from COutPoints
void WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs)
{
    LOCK2(cs_main, wallet->cs_wallet);
    for (auto const& outpoint : vOutpoints)
    {
        if (!wallet->mapWallet.count(outpoint.hash)) continue;
        int nDepth = wallet->mapWallet[outpoint.hash].GetDepthInMainChain();
        if (nDepth < 0) continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, nDepth);
        vOutputs.push_back(out);
    }
}

// AvailableCoins + LockedCoins grouped by wallet address (put change in one group with wallet address)
void WalletModel::listCoins(std::map<QString, std::vector<COutput> >& mapCoins) const
{
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, true, nullptr, false);

    LOCK2(cs_main, wallet->cs_wallet); // ListLockedCoins, mapWallet

    for (auto const& out : vCoins)
    {
        COutput cout = out;

        while (wallet->IsChange(cout.tx->vout[cout.i]) && cout.tx->vin.size() > 0 && (wallet->IsMine(cout.tx->vin[0]) != ISMINE_NO))
        {
            if (!wallet->mapWallet.count(cout.tx->vin[0].prevout.hash)) break;
            cout = COutput(&wallet->mapWallet[cout.tx->vin[0].prevout.hash], cout.tx->vin[0].prevout.n, 0);
        }

        CTxDestination address;
        if(!ExtractDestination(cout.tx->vout[cout.i].scriptPubKey, address)) continue;
        mapCoins[EncodeDestination(address).c_str()].push_back(out);
    }
}

bool WalletModel::isLockedCoin(uint256 hash, unsigned int n) const
{
    return false;
}

void WalletModel::lockCoin(COutPoint& output)
{
    return;
}

void WalletModel::unlockCoin(COutPoint& output)
{
    return;
}

void WalletModel::listLockedCoins(std::vector<COutPoint>& vOutpts)
{
    return;
}
