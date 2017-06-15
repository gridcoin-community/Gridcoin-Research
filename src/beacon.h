// Copyright (c) 2017 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

//!
//! \brief Generate beacon key pair.
//!
//! \param cpid CPID to generate keys for.
//! \param sOutPubKey Public key destination.
//! \param sOutPrivKey Private key destination.
//!
void GenerateBeaconKeys(const std::string &cpid, std::string &sOutPubKey, std::string &sOutPrivKey);

//!
//! \brief Store beacon keys in permanent storage.
//! \param cpid CPID tied to the keys.
//! \param pubKey Beacon public key.
//! \param privKey Beacon private key.
//!
void StoreBeaconKeys(
        const std::string &cpid,
        const std::string &pubKey,
        const std::string &privKey);

//!
//! \brief Get beacon private key from permanent storage.
//! \param cpid CPID tied to the private key.
//! \return Stored beacon private key if available, otherwise an empty string.
//!
std::string GetStoredBeaconPrivateKey(const std::string& cpid);

//!
//! \brief Get beacon public key from permanent storage.
//! \param cpid CPID tied to the public key.
//! \return Stored beacon public key if available, otherwise an empty string.
//!
std::string GetStoredBeaconPublicKey(const std::string& cpid);
