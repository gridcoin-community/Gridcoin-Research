// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QUICK_WALLETPROMPTLATCH_H
#define BITCOIN_QT_QUICK_WALLETPROMPTLATCH_H

#include <optional>

//! The one latch the wallet's passphrase prompts share: at most one owner
//! holds it, and a prompt is opened only by the owner that holds it. GUI
//! thread only, so it takes no lock.
//!
//! One latch rather than one flag per prompt: an unlock for staking answered
//! while a wallet-operation prompt is open would leave that operation's
//! one-read mode stale, and the operation would then fully unlock, and later
//! relock, a wallet the user had just unlocked for staking. Owner::Security is
//! for the wallet's own passphrase prompts (encrypt, change the passphrase,
//! unlock for staking), which take it in a later change.
class WalletPromptLatch
{
public:
    enum class Owner { WalletOps, Security };

    //! Takes the latch for `owner` and returns true, or returns false while
    //! any owner holds it.
    bool tryAcquire(Owner owner)
    {
        if (m_holder) return false;
        m_holder = owner;
        return true;
    }

    //! Releases the latch if `owner` holds it; otherwise does nothing.
    void release(Owner owner)
    {
        if (m_holder == owner) m_holder.reset();
    }

    //! Any owner holds the latch.
    bool busy() const { return m_holder.has_value(); }

    //! `owner` holds the latch.
    bool heldBy(Owner owner) const { return m_holder == owner; }

private:
    std::optional<Owner> m_holder;
};

#endif // BITCOIN_QT_QUICK_WALLETPROMPTLATCH_H
