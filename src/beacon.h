// Copyright (c) 2017 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

//!
//! \brief Generate beacon key pair.
//! 
//! Checks the current beacon keys for their validity and generate
//! new ones if the old ones are invalid. If the old pair is valid
//! they are returned.
//! 
//! \param cpid CPID to generate keys for.
//! \param sOutPubKey Public key destination.
//! \param sOutPrivKey Private key destination.
//! \return \c 1 if keys were reused or \c 2 new keys were generated.
//!
int GenerateBeaconKeys(const std::string &cpid, std::string &sOutPubKey, std::string &sOutPrivKey);

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
