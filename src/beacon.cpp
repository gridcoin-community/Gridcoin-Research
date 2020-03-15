#include "beacon.h"
#include "block.h"
#include "util.h"
#include "uint256.h"
#include "key.h"
#include "main.h"
#include "init.h"
#include "appcache.h"
#include "contract/contract.h"
#include "key.h"
#include "neuralnet/researcher.h"
#include "neuralnet/tally.h"

std::string RetrieveBeaconValueWithMaxAge(const std::string& cpid, int64_t iMaxSeconds);
std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);

bool GenerateBeaconKeys(const std::string &cpid, CKey &outPrivPubKey)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(pwalletMain->cs_wallet);
    // Try to reuse 5-month-old beacon
    const std::string sBeaconPublicKey = GetBeaconPublicKey(cpid,false);

    if(!sBeaconPublicKey.empty())
    {
        CPubKey oldKey(ParseHex(sBeaconPublicKey));
        if(oldKey.IsValid())
        {
            if (pwalletMain->GetKey(oldKey.GetID(), outPrivPubKey))
            {
                assert(outPrivPubKey.IsValid());//GetKey gives valid key or false
                assert(outPrivPubKey.GetPubKey()==oldKey);//GetKey should return the key we ask for
                LogPrintf("GenerateBeaconKeys: Reusing >5 <6 m old Key");
                return true;
            }
        }
    }

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey, false))
        return error("GenerateBeaconKeys: Failed to get Key from Wallet");

    CKeyID keyID = newKey.GetID();

    // Assign a description for this address
    std::string strLabel= "DPoR Beacon CPID "+cpid+" "+ToString(nBestHeight);
    pwalletMain->SetAddressBookName(keyID, strLabel);

    // Get the private part of the key from wallet
    if (!pwalletMain->GetKey(keyID, outPrivPubKey))
        return error("GenerateBeaconKeys: Failed to get Private Key from Wallet");

    return true;
}

bool GetStoredBeaconPrivateKey(const std::string& cpid, CKey& outPrivPubKey)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(pwalletMain->cs_wallet);
    CPubKey oldKey(ParseHex(GetBeaconPublicKey(cpid, false)));
    if(oldKey.IsValid())
    {
        if (pwalletMain->GetKey(oldKey.GetID(), outPrivPubKey))
        {
            return outPrivPubKey.IsValid();
        }
    }
    return false;
}

int64_t MaxBeaconAge()
{
    // 6 months in seconds.
    return 3600 * 24 * 30 * 6;
}

int64_t BeaconAgeAdvertiseThreshold()
{
    // 5 months in seconds.
    return 3600 * 24 * 30 * 5;
}

void GetBeaconElements(const std::string& sBeacon, std::string& out_cpid, std::string& out_address, std::string& out_publickey)
{
    if (sBeacon.empty()) return;
    std::string sContract = DecodeBase64(sBeacon);
    std::vector<std::string> vContract = split(sContract.c_str(),";");
    if (vContract.size() < 4) return;
    out_cpid = vContract[0];
    out_address = vContract[2];
    out_publickey = vContract[3];
}

std::string GetBeaconPublicKey(const std::string& cpid, bool bAdvertisingBeacon)
{
    // 3-26-2017 - Ensure beacon public key is within 6 months of network age
    // (If advertising, let it be returned as missing after 5 months, to
    // ensure the public key is renewed seamlessly).
    int64_t iMaxSeconds = bAdvertisingBeacon ? BeaconAgeAdvertiseThreshold() : MaxBeaconAge();
    std::string sBeacon = RetrieveBeaconValueWithMaxAge(cpid, iMaxSeconds);
    if (sBeacon.empty())
        return "";

    // Beacon data structure: CPID,hashRand,Address,beacon public key: base64 encoded
    std::string sContract = DecodeBase64(sBeacon);
    std::vector<std::string> vContract = split(sContract.c_str(),";");
    if (vContract.size() < 4) return "";
    std::string sBeaconPublicKey = vContract[3];
    return sBeaconPublicKey;
}

std::set<std::string> GetAlternativeBeaconKeys(const std::string& cpid)
{
    int64_t iMaxSeconds = 60 * 24 * 30 * 6 * 60;
    std::set<std::string> result;

    for(const auto& item : ReadCacheSection(Section::BEACONALT))
    {
        const std::string& key = item.first;
        const std::string& value = item.second.value;
        if(!std::equal(cpid.begin(), cpid.end(), key.begin()))
            continue;

        const int64_t iAge = pindexBest != NULL
            ? pindexBest->nTime - item.second.timestamp
            : 0;
        if (iAge > iMaxSeconds)
            continue;

        result.emplace(value);
    }
    return result;
}

int64_t BeaconTimeStamp(const std::string& cpid)
{
    if (!IsResearcher(cpid)) {
        return 0;
    }

    AssertLockHeld(cs_main);

    const AppCacheEntry& entry = ReadCache(Section::BEACON, cpid);

    if (fDebug10) {
        LogPrintf("Beacon %s, Locktime %" PRId64, entry.value, entry.timestamp);
    }

    return entry.timestamp;
}

bool HasActiveBeacon(const std::string& cpid)
{
    return GetBeaconPublicKey(cpid, false).empty() == false;
}

std::string RetrieveBeaconValueWithMaxAge(const std::string& cpid, int64_t iMaxSeconds)
{
    AssertLockHeld(cs_main);
    const AppCacheEntry& entry = ReadCache(Section::BEACON, cpid);

    // Compare the age of the beacon to the age of the current block. If we have
    // no current block we assume that the beacon is valid.
    int64_t iAge = pindexBest != NULL
                                 ? pindexBest->nTime - entry.timestamp
                                 : 0;

    return (iAge > iMaxSeconds)
            ? ""
            : entry.value;
}

bool VerifyBeaconContractTx(const CTransaction& tx)
{
    AssertLockHeld(cs_main);
    // Check if tx contains beacon advertisement and evaluate for certain conditions
    std::string chkMessageType = ExtractXML(tx.hashBoinc, "<MT>", "</MT>");
    std::string chkMessageAction = ExtractXML(tx.hashBoinc, "<MA>", "</MA>");

    if (chkMessageType != "beacon")
        return true; // Not beacon contract

    if (chkMessageAction != "A")
        return true; // Not an add contract for beacon

    std::string chkMessageContract = ExtractXML(tx.hashBoinc, "<MV>", "</MV>");
    std::string chkMessageContractCPID = ExtractXML(tx.hashBoinc, "<MK>", "</MK>");
    // Here we GetBeaconElements for the contract in the tx
    std::string tx_out_cpid;
    std::string tx_out_address;
    std::string tx_out_publickey;

    GetBeaconElements(chkMessageContract, tx_out_cpid, tx_out_address, tx_out_publickey);

    if (tx_out_cpid.empty() || tx_out_address.empty() || tx_out_publickey.empty() || chkMessageContractCPID.empty())
        return false; // Incomplete contract

    const AppCacheEntry& beaconEntry = ReadCache(Section::BEACON, chkMessageContractCPID);
    if (beaconEntry.value.empty())
    {
        if (fDebug10)
            LogPrintf("VBCTX : No Previous beacon found for CPID %s", chkMessageContractCPID);

        return true; // No previous beacon in cache
    }

    int64_t chkiAge = pindexBest != NULL
                                    ? tx.nLockTime - beaconEntry.timestamp
                                    : 0;
    int64_t chkSecondsBase = 60 * 24 * 30 * 60;

    // Conditions
    // Condition a) if beacon is younger then 5 months deny tx
    if (chkiAge <= chkSecondsBase * 5 && chkiAge >= 1)
    {
        if (fDebug10)
            LogPrintf("VBCTX : Beacon age violation. Beacon Age %" PRId64 " < Required Age %" PRId64, chkiAge, (chkSecondsBase * 5));

        return false;
    }

    // Condition b) if beacon is younger then 6 months but older then 5 months verify using the same keypair; if not deny tx
    if (chkiAge >= chkSecondsBase * 5 && chkiAge <= chkSecondsBase * 6)
    {
        std::string chk_out_cpid;
        std::string chk_out_address;
        std::string chk_out_publickey;

        // Here we GetBeaconElements for the contract in the current beacon in chain
        GetBeaconElements(beaconEntry.value, chk_out_cpid, chk_out_address, chk_out_publickey);

        if (tx_out_publickey != chk_out_publickey)
        {
            if (fDebug10)
                LogPrintf("VBCTX : Beacon tx publickey != publickey in chain. %s != %s", tx_out_publickey, chk_out_publickey);

            return false;
        }
    }

    // Passed checks
    return true;
}

bool ImportBeaconKeysFromConfig(const std::string& cpid, CWallet* wallet)
{
    if(cpid.empty())
        return error("Empty CPID");

    std::string strSecret = GetArgument("privatekey" + cpid + (fTestNet ? "testnet" : ""), "");
    if(strSecret.empty())
        return false;

    auto vecsecret = ParseHex(strSecret);

    CKey key;
    if(!key.SetPrivKey(CPrivKey(vecsecret.begin(),vecsecret.end())))
        return error("ImportBeaconKeysFromConfig: Invalid private key");
    CKeyID vchAddress = key.GetPubKey().GetID();

    LOCK(wallet->cs_wallet);

    // Don't throw error in case a key is already there
    if (!wallet->HaveKey(vchAddress))
    {
        if (wallet->IsLocked())
            return error("ImportBeaconKeysFromConfig: Wallet locked!");

        wallet->MarkDirty();

        wallet->mapKeyMetadata[vchAddress].nCreateTime = 0;

        if (!wallet->AddKey(key))
            return error("ImportBeaconKeysFromConfig: failed to add key to wallet");

        wallet->SetAddressBookName(vchAddress, "DPoR Beacon CPID " + cpid + " imported");
    }
    return true;
}

BeaconConsensus GetConsensusBeaconList()
{
    //BeaconMap mBeaconMap;
    BeaconConsensus Consensus;

    BlockFinder MaxConsensusLadder;

    // Use 4 times the BLOCK_GRANULARITY which moves the consensus block every hour.
    // TODO: Make the mod a function of SCRAPER_CMANIFEST_RETENTION_TIME in scraper.h.
    CBlockIndex* pMaxConsensusLadder = MaxConsensusLadder.FindByHeight((pindexBest->nHeight - CONSENSUS_LOOKBACK)
                                                                        - (pindexBest->nHeight - CONSENSUS_LOOKBACK) % (BLOCK_GRANULARITY * 4));

    Consensus.nBlockHash = pMaxConsensusLadder->GetBlockHash();

    const int64_t maxTime = pMaxConsensusLadder->nTime;
    const int64_t minTime = maxTime - MaxBeaconAge();

    for(const auto& item : ReadCacheSection(Section::BEACON))
    {
        const std::string& key = item.first;
        BeaconEntry beaconentry;

        beaconentry.timestamp = item.second.timestamp;
        beaconentry.value = item.second.value;

        // Compare age restrictions if specified.
        if((minTime && beaconentry.timestamp <= minTime) ||
           (maxTime && beaconentry.timestamp >= maxTime))
            continue;

        // Skip invalid beacons.
        if (Contains("INVESTOR", beaconentry.value))
            continue;

        Consensus.mBeaconMap[key] = beaconentry;
    }

    return Consensus;
}

// -------------------------- Both In and Out
BeaconStatus GetBeaconStatus(std::string& sCPID)
{
    BeaconStatus beacon_status;

    LOCK(cs_main);

    if (sCPID.empty())
    {
        sCPID = NN::GetPrimaryCpid();
    }

    beacon_status.sPubKey = GetBeaconPublicKey(sCPID, false);
    beacon_status.iBeaconTimestamp = BeaconTimeStamp(sCPID);
    beacon_status.timestamp = TimestampToHRDate(beacon_status.iBeaconTimestamp);
    beacon_status.hasBeacon = HasActiveBeacon(sCPID);
    beacon_status.dPriorSBMagnitude = NN::Tally::GetMagnitude(NN::MiningId::Parse(sCPID)).Floating();
    beacon_status.is_mine = (sCPID == NN::GetPrimaryCpid());

    return beacon_status;
}
