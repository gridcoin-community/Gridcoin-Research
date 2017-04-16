#include "beacon.h"
#include "main.h"
#include "uint256.h"
#include "key.h"

extern std::string SignBlockWithCPID(std::string sCPID, std::string sBlockHash);
extern bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature);

int GenerateBeaconKeys(const std::string &cpid, std::string &sOutPubKey, std::string &sOutPrivKey)
{
    // First Check the Index - if it already exists, use it
    std::string sSuffix = fTestNet ? "testnet" : "";
    sOutPrivKey = GetArgument("PrivateKey" + cpid + sSuffix, "");
    sOutPubKey  = GetArgument("PublicKey" + cpid + sSuffix, "");
    
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
    
    // Store the Keypair
    WriteKey("PrivateKey" + cpid + sSuffix,sOutPrivKey);
    WriteKey("PublicKey" + cpid + sSuffix,sOutPubKey);
    return 2;
}
