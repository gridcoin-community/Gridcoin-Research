// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_STAKING_SIGNER_H
#define GRIDCOIN_WALLET_STAKING_SIGNER_H

// keystore.h needs CScript and CScriptID complete and does not include them.
#include "script/script.h"
#include "keystore.h"

//! \brief Signs with a store's keys through its staking accessor.
//!
//! What a staking path hands to SignSignature in place of the store itself. The
//! store's own GetKey() refuses while it is unlocked for staking only; this
//! reaches the keys through GetKeyForStaking() instead, so it signs under
//! StakingOnly or Full, and never while the store is locked. Making one takes a
//! StakingKeyAccess, which only the staking paths can make.
//!
//! It borrows the store and must not outlive it.
class StakingSigningProvider final : public SigningProvider
{
public:
    StakingSigningProvider(const CCryptoKeyStore& store, StakingKeyAccess access);

    bool GetKey(const CKeyID& id, CKey& key) const override;
    bool GetPubKey(const CKeyID& id, CPubKey& pub) const override;
    bool GetCScript(const CScriptID& id, CScript& script) const override;

private:
    const CCryptoKeyStore& m_store;
    StakingKeyAccess m_access;
};

#endif // GRIDCOIN_WALLET_STAKING_SIGNER_H
