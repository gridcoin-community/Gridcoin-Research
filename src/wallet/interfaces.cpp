// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/wallet.h"

#include "interfaces/handler.h"
#include "key_io.h"
#include "wallet/wallet.h"

#include <memory>
#include <utility>

namespace interfaces {
namespace {

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

    bool isCrypted() override { return m_wallet->IsCrypted(); }

    bool isLocked() override { return m_wallet->IsLocked(); }

    bool isUnlockedForStakingOnly() override
    {
        return !m_wallet->IsLocked() && fWalletUnlockStakingOnly;
    }

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

} // namespace

std::unique_ptr<Wallet> MakeWallet(CWallet* wallet)
{
    return std::make_unique<WalletImpl>(wallet);
}

} // namespace interfaces
