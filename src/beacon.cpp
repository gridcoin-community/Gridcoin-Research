#include "beacon.h"
#include "util.h"
#include "uint256.h"
#include "key.h"
#include "main.h"

std::vector<std::string> split(std::string s, std::string delim);
extern std::string SignBlockWithCPID(std::string sCPID, std::string sBlockHash);
extern bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature);
std::string RetrieveBeaconValueWithMaxAge(const std::string& cpid, int64_t iMaxSeconds);
int64_t GetRSAWeightByCPIDWithRA(std::string cpid);

namespace
{
    std::string GetNetSuffix()
    {
        return fTestNet ? "testnet" : "";
    }
}

bool GenerateBeaconKeys(const std::string &cpid, std::string &sOutPubKey, std::string &sOutPrivKey)
{
    // First Check the Index - if it already exists, use it
    sOutPrivKey = GetArgument("privatekey" + cpid + GetNetSuffix(), "");
    sOutPubKey  = GetArgument("publickey" + cpid + GetNetSuffix(), "");

    // If current keypair is not empty, but is invalid, allow the new keys to be stored, otherwise return 1: (10-25-2016)
    if (!sOutPrivKey.empty() && !sOutPubKey.empty())
    {
        uint256 hashBlock = GetRandHash();
        std::string sSignature = SignBlockWithCPID(cpid,hashBlock.GetHex());
        bool fResult = VerifyCPIDSignature(cpid, hashBlock.GetHex(), sSignature);
        if (fResult)
        {
            printf("\r\nGenerateNewKeyPair::Current keypair is valid.\r\n");
            return false;
        }
    }

    // Generate the Keypair
    CKey key;
    key.MakeNewKey(false);
    CPrivKey vchPrivKey = key.GetPrivKey();
    sOutPrivKey = HexStr<CPrivKey::iterator>(vchPrivKey.begin(), vchPrivKey.end());
    sOutPubKey = HexStr(key.GetPubKey().Raw());

    return true;
}

void StoreBeaconKeys(
        const std::string &cpid,
        const std::string &pubKey,
        const std::string &privKey)
{
    WriteKey("publickey" + cpid + GetNetSuffix(), pubKey);
    WriteKey("privatekey" + cpid + GetNetSuffix(), privKey);
}

std::string GetStoredBeaconPrivateKey(const std::string& cpid)
{
    return GetArgument("privatekey" + cpid + GetNetSuffix(), "");
}

std::string GetStoredBeaconPublicKey(const std::string& cpid)
{
    return GetArgument("publickey" + cpid + GetNetSuffix(), "");
}

void ActivateBeaconKeys(
        const std::string &cpid,
        const std::string &pubKey,
        const std::string &privKey)
{
    SetArgument("publickey" + cpid + GetNetSuffix(), pubKey);
    SetArgument("privatekey" + cpid + GetNetSuffix(), privKey);
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
    //3-26-2017 - Ensure beacon public key is within 6 months of network age (If advertising, let it be returned as missing after 5 months, to ensure the public key is renewed seamlessly).
    int iMonths = bAdvertisingBeacon ? 5 : 6;
    int64_t iMaxSeconds = 60 * 24 * 30 * iMonths * 60;
    std::string sBeacon = RetrieveBeaconValueWithMaxAge(cpid, iMaxSeconds);
    if (sBeacon.empty()) return "";
    // Beacon data structure: CPID,hashRand,Address,beacon public key: base64 encoded
    std::string sContract = DecodeBase64(sBeacon);
    std::vector<std::string> vContract = split(sContract.c_str(),";");
    if (vContract.size() < 4) return "";
    std::string sBeaconPublicKey = vContract[3];
    return sBeaconPublicKey;
}

int64_t BeaconTimeStamp(const std::string& cpid, bool bZeroOutAfterPOR)
{
    std::string sBeacon = mvApplicationCache["beacon;" + cpid];
    int64_t iLocktime = mvApplicationCacheTimestamp["beacon;" + cpid];
    int64_t iRSAWeight = GetRSAWeightByCPIDWithRA(cpid);
    if (fDebug10)
        printf("\r\n Beacon %s, Weight %" PRId64 ", Locktime %" PRId64 "\r\n",sBeacon.c_str(), iRSAWeight, iLocktime);
    if (bZeroOutAfterPOR && iRSAWeight==0)
        iLocktime = 0;
    return iLocktime;

}

bool HasActiveBeacon(const std::string& cpid)
{
    return GetBeaconPublicKey(cpid, false).empty() == false;
}

std::string RetrieveBeaconValueWithMaxAge(const std::string& cpid, int64_t iMaxSeconds)
{
    const std::string key = "beacon;" + cpid;
    const std::string& value = mvApplicationCache[key];

    // Compare the age of the beacon to the age of the current block. If we have
    // no current block we assume that the beacon is valid.
    int64_t iAge = pindexBest != NULL
          ? pindexBest->nTime - mvApplicationCacheTimestamp[key]
          : 0;

    return (iAge > iMaxSeconds)
          ? ""
          : value;
}
