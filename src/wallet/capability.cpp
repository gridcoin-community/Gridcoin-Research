// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/capability.h"

#include "keystore.h"
#include "wallet/wallet.h"

#include <algorithm>

std::chrono::seconds CapabilityTtl(CapabilityKind kind)
{
    switch (kind) {
    case CapabilityKind::Send:
        return std::chrono::seconds(300);
    case CapabilityKind::SignMessage:
        return std::chrono::seconds(120);
    case CapabilityKind::SignPSGT:
        return std::chrono::seconds(300);
    case CapabilityKind::Contract:
        return std::chrono::seconds(300);
    case CapabilityKind::Backup:
        return std::chrono::seconds(300);
    case CapabilityKind::FileTransfer:
        return std::chrono::seconds(3600);
    case CapabilityKind::Settings:
        return std::chrono::seconds(120);
    case CapabilityKind::AutoUnlockSetup:
        return std::chrono::seconds(120);
    case CapabilityKind::RpcRequest:
        return std::chrono::seconds(300);
    }

    // Only a value outside the enumeration reaches here, as a cast from a wire
    // byte could make. It gets no lifetime, so it can authorise nothing.
    return std::chrono::seconds(0);
}

bool IsKnownCapabilityKind(CapabilityKind kind)
{
    switch (kind) {
    case CapabilityKind::Send:
    case CapabilityKind::SignMessage:
    case CapabilityKind::SignPSGT:
    case CapabilityKind::Contract:
    case CapabilityKind::Backup:
    case CapabilityKind::FileTransfer:
    case CapabilityKind::Settings:
    case CapabilityKind::AutoUnlockSetup:
    case CapabilityKind::RpcRequest:
        return true;
    }

    return false;
}

std::chrono::milliseconds FailedPassphraseDelay(unsigned int consecutive_failures)
{
    const unsigned int shift = std::min(consecutive_failures, 7U);
    const std::chrono::milliseconds delay(int64_t{250} << shift);

    return std::min(delay, std::chrono::milliseconds(30000));
}

CapabilityScope::CapabilityScope(CWallet& wallet, std::optional<CapabilityId> id, CapabilityKind kind)
{
    // The capability is looked up under the wallet's capability mutex, which
    // is released before the grant is made. The grant touches only this
    // thread's state.
    if (id && wallet.IsCapabilityLive(*id, kind)) {
        m_grant.reset(new ElevationGrant(wallet));
    }
}

CapabilityScope::~CapabilityScope() = default;
