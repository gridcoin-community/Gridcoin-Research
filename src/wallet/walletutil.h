// Copyright (c) 2017-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLETUTIL_H
#define BITCOIN_WALLET_WALLETUTIL_H

namespace wallet {
/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getwalletinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_HD = 5040101, // Hierarchical key derivation after BIP32 (HD Wallet)

    FEATURE_LATEST = FEATURE_HD
};

bool IsFeatureSupported(int wallet_version, int feature_version);
WalletFeature GetClosestWalletFeature(int version);

} // namespace wallet

#endif // BITCOIN_WALLET_WALLETUTIL_H
