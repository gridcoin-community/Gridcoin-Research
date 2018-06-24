// Copyright (c) 2017-2018 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "fwd.h"
#include <string>

//!
//! \brief Generate beacon key pair.
//!
//! Checks the current beacon keys for their validity and generate
//! new ones if the old ones are invalid. If the old pair is valid
//! they are returned.
//!
//! Generated key is stored in wallet.
//!
//! \param cpid CPID to generate keys for.
//! \param outPrivPubKey Generated or reused Private+public key
//! \return \c false on failure
//!
bool GenerateBeaconKeys(const std::string &cpid, CKey &outPrivPubKey);


//!
//! \brief Get beacon private key from permanent storage.
//! \param cpid CPID tied to the private key.
//! \param outPrivPubKey Stored beacon private key if available, otherwise an empty string.
//! \return true on success
//!
bool GetStoredBeaconPrivateKey(const std::string& cpid, CKey& outPrivPubKey);

// Push new beacon keys into memory as this process is not automatic and currently requires a restart of client to do so.
// This corrects issues where users who have deleted beacons and then advertise new ones.
// This corrects issues where users who readvertise and the existing keypair is no longer valid.
// TODO: remove this

void ActivateBeaconKeys(
        const std::string &cpid,
        const std::string &pubKey,
        const std::string &privKey);

// Lets move more of the beacon functions out of rpcblockchain.cpp and main.cpp
// Lets also use header space where applicable - iFoggz

void GetBeaconElements(const std::string& sBeacon, std::string& out_cpid, std::string& out_address, std::string& out_publickey);
std::string GetBeaconPublicKey(const std::string& cpid, bool bAdvertisingBeacon);
int64_t BeaconTimeStamp(const std::string& cpid, bool bZeroOutAfterPOR);
bool HasActiveBeacon(const std::string& cpid);

bool VerifyBeaconContractTx(const CTransaction& tx);
