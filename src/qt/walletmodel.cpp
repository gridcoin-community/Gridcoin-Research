#include "walletmodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactionrecord.h"
#include "transactiontablemodel.h"

#include "node/ui_interface.h"
#include "wallet/wallet.h"
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
{
    addressTableModel = new AddressTableModel(wallet, this);
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

            last_balance_update_time = current_time;

            emit balanceChanged(newBalance, newStake, newUnconfirmedBalance, newImmatureBalance);
        }
    }
}

void WalletModel::drainEventQueue()
{
    auto events = m_event_queue.drain();
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

    // Balance and number of transactions might have changed.
    checkBalanceChanged();

    int newNumTransactions = getNumTransactions();
    if (cachedNumTransactions != newNumTransactions) {
        cachedNumTransactions = newNumTransactions;

        emit numTransactionsChanged(newNumTransactions);
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

        // If any recipient has "subtract fee from amount" enabled, rebuild the
        // outputs with the fee deducted and create the transaction again.
        // This runs even if the first pass failed (e.g. sending entire balance),
        // since nFeeRequired is initialized to nTransactionFee and provides a
        // reasonable starting estimate.
        //
        // The outer loop handles fee refinement: CreateTransaction may discover
        // that the actual fee (based on transaction size) exceeds the initial
        // estimate. When that happens, it updates nFeeRequired and fails because
        // SelectCoins can't cover the higher total. We detect the fee increase,
        // rebuild outputs with the new fee, and retry. This converges quickly
        // since fees are monotonically non-decreasing and bounded by tx size.
        if (fAnySubtractFeeFromAmount)
        {
            int nSubtractRecipients = 0;
            for (const SendCoinsRecipient& rcp : recipients)
            {
                if (rcp.fSubtractFeeFromAmount) ++nSubtractRecipients;
            }

            // Retry limit prevents infinite loops in pathological cases
            for (int nAttempt = 0; nAttempt < 10; ++nAttempt)
            {
                vecSend.clear();
                int64_t nFeeRemainder = nFeeRequired % nSubtractRecipients;
                bool fFirst = true;
                for (const SendCoinsRecipient& rcp : recipients)
                {
                    CScript scriptPubKey;
                    scriptPubKey.SetDestination(DecodeDestination(rcp.address.toStdString()));
                    int64_t nAmount = rcp.amount;

                    if (rcp.fSubtractFeeFromAmount)
                    {
                        nAmount -= nFeeRequired / nSubtractRecipients;
                        // First opted-in recipient absorbs the truncation remainder
                        if (fFirst)
                        {
                            nAmount -= nFeeRemainder;
                            fFirst = false;
                        }
                        if (nAmount <= 0)
                        {
                            return SendCoinsReturn(FeeExceedsSubtractedAmount, nFeeRequired);
                        }
                    }

                    vecSend.push_back(std::make_pair(scriptPubKey, nAmount));
                }

                int64_t nFeePrev = nFeeRequired;
                fCreated = wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, coinControl);

                if (fCreated || nFeeRequired <= nFeePrev)
                    break;

                // Fee increased (tx was larger than estimated) — retry with
                // the updated fee subtracted from recipient amounts.
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
    //   CT_NEW / CT_UPDATED callsites all hold cs_wallet (wallet.cpp:458,
    //   475, 572, 2400, 3057). We can safely look up mapWallet[hash] and
    //   run decomposeTransaction (which only requires cs_wallet via the
    //   recursive IsMine() calls it makes).
    //
    //   CT_DELETED callsites (main.cpp:1290, wallet.cpp:1349) DO NOT hold
    //   cs_wallet — the tx has already been erased. The TxRemoved payload
    //   carries only the hash, so no wallet lookup is needed.
    switch (status) {
    case CT_NEW:
    case CT_UPDATED:
    case CT_UPDATING: {
        auto it = wallet->mapWallet.find(hash);
        if (it == wallet->mapWallet.end()) {
            // Tx isn't in mapWallet — only happens if the notification
            // raced with an erasure. Push TxRemoved to keep the consumer
            // in sync.
            LogPrint(BCLog::LogFlags::VERBOSE,
                     "NotifyTransactionChanged: %s status=%d but tx not in mapWallet "
                     "— pushing TxRemoved",
                     hash.GetHex(), status);
            walletmodel->getEventQueue().push(GRC::TxRemovedPayload{hash});
            break;
        }
        if (TransactionRecord::showTransaction(it->second, false, 0)) {
            GRC::TxAddedPayload payload;
            payload.records = TransactionRecord::decomposeTransaction(wallet, it->second);
            if (!payload.records.isEmpty()) {
                walletmodel->getEventQueue().push(std::move(payload));
            }
        } else {
            // Tx is now filtered out (e.g. became an orphan coinstake).
            // Ensure the consumer removes the row if it was previously
            // visible — TxRemoved is a no-op if the tx isn't in
            // cachedWallet.
            walletmodel->getEventQueue().push(GRC::TxRemovedPayload{hash});
        }
        break;
    }
    case CT_DELETED:
        walletmodel->getEventQueue().push(GRC::TxRemovedPayload{hash});
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
