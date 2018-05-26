#include "cpid.h"
#include "init.h"
#include "rpcclient.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#include "keystore.h"
#include "beacon.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

using namespace json_spirit;

std::string GetBurnAddress() { return fTestNet ? "mk1e432zWKH1MW57ragKywuXaWAtHy1AHZ" : "S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB";
                             }

bool CheckMessageSignature(std::string sAction,std::string messagetype, std::string sMsg, std::string sSig, std::string strMessagePublicKey)
{
    std::string strMasterPubKey = "";
    if (messagetype=="project" || messagetype=="projectmapping")
    {
        strMasterPubKey= msMasterProjectPublicKey;
    }
    else
    {
        strMasterPubKey = msMasterMessagePublicKey;
    }

    if (!strMessagePublicKey.empty()) strMasterPubKey = strMessagePublicKey;
    if (sAction=="D" && messagetype=="beacon") strMasterPubKey = msMasterProjectPublicKey;
    if (sAction=="D" && messagetype=="poll")   strMasterPubKey = msMasterProjectPublicKey;
    if (sAction=="D" && messagetype=="vote")   strMasterPubKey = msMasterProjectPublicKey;

    std::string db64 = DecodeBase64(sSig);
    CKey key;
    if (!key.SetPubKey(ParseHex(strMasterPubKey))) return false;
    std::vector<unsigned char> vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());
    std::vector<unsigned char> vchSig = std::vector<unsigned char>(db64.begin(), db64.end());
    if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig)) return false;
    return true;
}

bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature)
{
    std::string sBeaconPublicKey = GetBeaconPublicKey(sCPID, false);
    std::string sConcatMessage = sCPID + sBlockHash;
    bool bValid = CheckMessageSignature("R","cpid", sConcatMessage, sSignature, sBeaconPublicKey);
    if(!bValid)
        LogPrintf("VerifyCPIDSignature: invalid signature sSignature=%s, cached key=%s\n"
                  ,sSignature, sBeaconPublicKey);
    return bValid;
}

std::string SignMessage(std::string sMsg, std::string sPrivateKey)
{
    CKey key;
    std::vector<unsigned char> vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());
    std::vector<unsigned char> vchPrivKey = ParseHex(sPrivateKey);
    std::vector<unsigned char> vchSig;
    key.SetPrivKey(CPrivKey(vchPrivKey.begin(), vchPrivKey.end())); // if key is not correct openssl may crash
    if (!key.Sign(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
    {
        return "Unable to sign message, check private key.";
    }
    const std::string sig(vchSig.begin(), vchSig.end());
    std::string SignedMessage = EncodeBase64(sig);
    return SignedMessage;
}

std::string AddMessage(bool bAdd, std::string sType, std::string sPrimaryKey, std::string sValue,
                       std::string sMasterKey, int64_t MinimumBalance, double dFees, std::string strPublicKey)
{
    std::string sAddress = GetBurnAddress();
    CBitcoinAddress address(sAddress);
    if (!address.IsValid())       throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");
    int64_t nAmount = AmountFromValue(dFees);
    // Wallet comments
    CWalletTx wtx;
    if (pwalletMain->IsLocked())  throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    std::string sMessageType      = "<MT>" + sType  + "</MT>";  //Project or Smart Contract
    std::string sMessageKey       = "<MK>" + sPrimaryKey   + "</MK>";
    std::string sMessageValue     = "<MV>" + sValue + "</MV>";
    std::string sMessagePublicKey = "<MPK>"+ strPublicKey + "</MPK>";
    std::string sMessageAction    = bAdd ? "<MA>A</MA>" : "<MA>D</MA>"; //Add or Delete
    //Sign Message
    std::string sSig = SignMessage(sType+sPrimaryKey+sValue,sMasterKey);
    std::string sMessageSignature = "<MS>" + sSig + "</MS>";
    wtx.hashBoinc = sMessageType+sMessageKey+sMessageValue+sMessageAction+sMessagePublicKey+sMessageSignature;
    std::string strError = pwalletMain->SendMoneyToDestinationWithMinimumBalance(address.Get(), nAmount, MinimumBalance, wtx);
    if (!strError.empty())        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    return wtx.GetHash().GetHex().c_str();
}

std::string AddContract(std::string sType, std::string sName, std::string sContract)
{
    std::string sPass = (sType=="project" || sType=="projectmapping" || sType=="smart_contract") ? GetArgument("masterprojectkey", msMasterMessagePrivateKey) : msMasterMessagePrivateKey;
    std::string result = AddMessage(true,sType,sName,sContract,sPass,AmountFromValue(1),.00001,"");
    return result;
}

bool SignBlockWithCPID(const std::string& sCPID, const std::string& sBlockHash, std::string& sSignature, std::string& sError, bool bAdvertising=false)
{
    // Check if there is a beacon for this user
    // If not then return false as GetStoresBeaconPrivateKey grabs from the config
    if (!HasActiveBeacon(sCPID) && !bAdvertising)
    {
        sError = "No active beacon";
        return false;
    }
    // Returns the Signature of the CPID+BlockHash message.
    std::string sPrivateKey = GetStoredBeaconPrivateKey(sCPID);
    std::string sMessage = sCPID + sBlockHash;
    sSignature = SignMessage(sMessage,sPrivateKey);
    // If we failed to sign then return false
    if (sSignature == "Unable to sign message, check private key.")
    {
        sError = sSignature;
        sSignature = "";
        return false;
    }
    return true;
}

