// Copyright (c) 2017-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <wallet/walletutil.h>

#include <array>

namespace wallet {
bool IsFeatureSupported(int wallet_version, int feature_version)
{
    return wallet_version >= feature_version;
}

WalletFeature GetClosestWalletFeature(int version)
{
    static constexpr std::array wallet_features{FEATURE_LATEST, FEATURE_HD, FEATURE_COMPRPUBKEY, FEATURE_WALLETCRYPT, FEATURE_BASE};
    for (const WalletFeature& wf : wallet_features) {
        if (version >= wf) return wf;
    }
    return static_cast<WalletFeature>(0);
}
} // namespace wallet
