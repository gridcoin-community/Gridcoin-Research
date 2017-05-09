#include "beacon.h"
#include "main.h"
#include "uint256.h"
#include "key.h"

extern std::string SignBlockWithCPID(std::string sCPID, std::string sBlockHash);
extern bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature);

namespace
{
    std::string GetNetSuffix()
    {
        return fTestNet ? "testnet" : "";
    }
}

int GenerateBeaconKeys(const std::string &cpid, std::string &sOutPubKey, std::string &sOutPrivKey)
{
    // First Check the Index - if it already exists, use it
    sOutPrivKey = GetArgument("PrivateKey" + cpid + GetNetSuffix(), "");
    sOutPubKey  = GetArgument("PublicKey" + cpid + GetNetSuffix(), "");
    
    // If current keypair is not empty, but is invalid, allow the new keys to be stored, otherwise return 1: (10-25-2016)
    if (!sOutPrivKey.empty() && !sOutPubKey.empty())
    {
        uint256 hashBlock = GetRandHash();
        std::string sSignature = SignBlockWithCPID(cpid,hashBlock.GetHex());
        bool fResult = VerifyCPIDSignature(cpid, hashBlock.GetHex(), sSignature);
        if (fResult)
        {
            printf("\r\nGenerateNewKeyPair::Current keypair is valid.\r\n");
            return 1;
        }
    }
    
    // Generate the Keypair
    CKey key;
    key.MakeNewKey(false);
    CPrivKey vchPrivKey = key.GetPrivKey();
    sOutPrivKey = HexStr<CPrivKey::iterator>(vchPrivKey.begin(), vchPrivKey.end());
    sOutPubKey = HexStr(key.GetPubKey().Raw());
    
    return 2;
}

void StoreBeaconKeys(
        const std::string &cpid,
        const std::string &pubKey,
        const std::string &privKey)
{
    WriteKey("PublicKey" + cpid + GetNetSuffix(), pubKey);
    WriteKey("PrivateKey" + cpid + GetNetSuffix(), privKey);
}

std::string GetStoredBeaconPrivateKey(const std::string& cpid)
{
    return GetArgument("privatekey" + cpid + GetNetSuffix(), "");
}

std::string GetStoredBeaconPublicKey(const std::string& cpid)
{
    return GetArgument("publickey" + cpid + GetNetSuffix(), "");
}
