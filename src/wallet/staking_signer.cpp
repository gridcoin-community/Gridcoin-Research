// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/staking_signer.h"

StakingSigningProvider::StakingSigningProvider(const CCryptoKeyStore& store, StakingKeyAccess access)
    : m_store(store)
    , m_access(access)
{
}

bool StakingSigningProvider::GetKey(const CKeyID& id, CKey& key) const
{
    return m_store.GetKeyForStaking(id, key, m_access);
}

bool StakingSigningProvider::GetPubKey(const CKeyID& id, CPubKey& pub) const
{
    return m_store.GetPubKey(id, pub);
}

bool StakingSigningProvider::GetCScript(const CScriptID& id, CScript& script) const
{
    return m_store.GetCScript(id, script);
}
