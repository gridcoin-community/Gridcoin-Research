#include "beacon.h"
#include "contract/cpid.h"
#include "contract/message.h"
#include "key.h"

bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature)
{
    std::string sBeaconPublicKey = GetBeaconPublicKey(sCPID, false);
    std::string sConcatMessage = sCPID + sBlockHash;
    bool bValid = CheckMessageSignature("R","cpid", sConcatMessage, sSignature, sBeaconPublicKey);
    if(!bValid)
        LogPrintf("VerifyCPIDSignature: invalid signature sSignature=%s, cached key=%s"
                  ,sSignature, sBeaconPublicKey);
    return bValid;
}

bool SignBlockWithCPID(
    const std::string& sCPID,
    const std::string& sBlockHash,
    std::string& sSignature,
    std::string& sError,
    bool bAdvertising)
{
    // Check if there is a beacon for this user
    // If not then return false as GetStoresBeaconPrivateKey grabs from the config
    if (!HasActiveBeacon(sCPID) && !bAdvertising)
    {
        sError = "No active beacon";
        return false;
    }

    // Returns the Signature of the CPID+BlockHash message.
    CKey keyBeacon;

    if (!GetStoredBeaconPrivateKey(sCPID, keyBeacon))
    {
        sError = "No beacon key";
        return false;
    }

    std::string sMessage = sCPID + sBlockHash;
    sSignature = SignMessage(sMessage,keyBeacon);

    // If we failed to sign then return false
    if (sSignature == "Unable to sign message, check private key.")
    {
        sError = sSignature;
        sSignature = "";
        return false;
    }

    return true;
}
