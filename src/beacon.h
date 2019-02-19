// Copyright (c) 2017-2019 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "fwd.h"
#include "util.h"
#include <string>
#include <map>

// This is modelled after AppCacheEntry/Section but named separately.
struct BeaconEntry
{
    std::string value; //!< Value of entry.
    int64_t timestamp; //!< Timestamp of entry.
};

typedef std::map<std::string, BeaconEntry> BeaconMap;

struct BeaconConsensus
{
    uint256 nBlockHash;
    BeaconMap mBeaconMap;
};

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
//!
//! Uses the appcache to find public key to cpid, and then wallet to find
//! privare key to it.
//!
//! \param cpid CPID tied to the private key.
//! \param outPrivPubKey Stored beacon private key if available, otherwise an empty string.
//! \return true on success
//!
bool GetStoredBeaconPrivateKey(const std::string& cpid, CKey& outPrivPubKey);

//!
//! \brief Get maximum beacon age.
//! \return Maximum beacon age in seconds.
//!
int64_t MaxBeaconAge();

//!
//! \brief Get beacon age advertise threshold.
//! \return The point at which a beacon should be re-advertised.
//!
int64_t BeaconAgeAdvertiseThreshold();

// Lets move more of the beacon functions out of rpcblockchain.cpp and main.cpp
// Lets also use header space where applicable - iFoggz

void GetBeaconElements(const std::string& sBeacon, std::string& out_cpid, std::string& out_address, std::string& out_publickey);
std::string GetBeaconPublicKey(const std::string& cpid, bool bAdvertisingBeacon);
int64_t BeaconTimeStamp(const std::string& cpid, bool bZeroOutAfterPOR);
bool HasActiveBeacon(const std::string& cpid);

bool VerifyBeaconContractTx(const CTransaction& tx);

//!
//! \brief Import beacon from configuration to the wallet.
//! 
//! Attempts to import beacon keys stored in the configuration file
//! into the wallet keystore.
//! 
//! \param cpid CPID to import the keys for.
//! \param wallet Wallet to import to.
//! 
//! \return \c true on successful import
//! \return \c true when the key was already in wallet
//! \return \c false on failure.
//!
bool ImportBeaconKeysFromConfig(const std::string& cpid, CWallet* wallet);

//!
//! \brief Get beacon list with consenus.
//!
//! Assembles a list of only active beacons with a consensus lookback from
//! 6 months ago the current tip minus ~1 hour.
//!
//! \return A list of active beacons.
//!
BeaconConsensus GetConsensusBeaconList();
