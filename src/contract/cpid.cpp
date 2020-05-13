#include "beacon.h"
#include "contract/cpid.h"
#include "key.h"

bool VerifyCPIDSignature(
    const std::string& sCPID,
    const std::string& sBlockHash,
    const std::string& sSignature,
    const std::string& sBeaconPublicKey)
{
    CKey key;

    bool valid = key.SetPubKey(ParseHex(sBeaconPublicKey))
        && key.Verify(
            Hash(sCPID.begin(), sCPID.end(), sBlockHash.begin(), sBlockHash.end()),
            DecodeBase64(sSignature.c_str()));

    if (!valid) {
        LogPrintf(
            "VerifyCPIDSignature: invalid signature sSignature=%s, cached key=%s",
            sSignature,
            sBeaconPublicKey);
    }

    return valid;
}

bool VerifyCPIDSignature(
    const std::string& sCPID,
    const std::string& sBlockHash,
    const std::string& sSignature)
{
    return VerifyCPIDSignature(
        sCPID,
        sBlockHash,
        sSignature,
        GetBeaconPublicKey(sCPID, false));
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
    if (!HasActiveBeacon(sCPID) && !bAdvertising) {
        sError = "No active beacon";
        return false;
    }

    CKey keyBeacon;

    if (!GetStoredBeaconPrivateKey(sCPID, keyBeacon)) {
        sError = "No beacon key";
        return false;
    }

    std::vector<unsigned char> signature_bytes;

    // Returns the Signature of the CPID+BlockHash message.
    bool result = keyBeacon.Sign(
        Hash(sCPID.begin(), sCPID.end(), sBlockHash.begin(), sBlockHash.end()),
        signature_bytes);

    if (!result) {
        sError = "Unable to sign message, check private key.";
        sSignature = "";

        return false;
    }

    sSignature = EncodeBase64(signature_bytes.data(), signature_bytes.size());

    return true;
}
