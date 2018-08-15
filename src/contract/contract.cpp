#include "cpid.h"
#include "init.h"
#include "rpcclient.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#include "keystore.h"
#include "beacon.h"
#include "appcache.h"

double GetTotalBalance();

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
    if (messagetype=="protocol")  strMasterPubKey = msMasterProjectPublicKey;

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
        LogPrintf("VerifyCPIDSignature: invalid signature sSignature=%s, cached key=%s"
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

std::string SendMessage(bool bAdd, std::string sType, std::string sPrimaryKey, std::string sValue,
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

std::string SendContract(std::string sType, std::string sName, std::string sContract)
{
    std::string sPass = (sType=="project" || sType=="projectmapping" || sType=="smart_contract") ? GetArgument("masterprojectkey", msMasterMessagePrivateKey) : msMasterMessagePrivateKey;
    std::string result = SendMessage(true,sType,sName,sContract,sPass,AmountFromValue(1),.00001,"");
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

int64_t AmountFromDouble(double dAmount)
{
    if (dAmount <= 0.0 || dAmount > MAX_MONEY)        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    int64_t nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))         throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

std::string executeRain(std::string sRecipients)
{
    CWalletTx wtx;
    wtx.mapValue["comment"] = "Rain";
    set<CBitcoinAddress> setAddress;
    vector<pair<CScript, int64_t> > vecSend;
    std::string sRainCommand = ExtractXML(sRecipients,"<RAIN>","</RAIN>");
    std::string sRainMessage = MakeSafeMessage(ExtractXML(sRecipients,"<RAINMESSAGE>","</RAINMESSAGE>"));
    std::string sRain = "<NARR>Project Rain: " + sRainMessage + "</NARR>";

    if (!sRainCommand.empty())
        sRecipients = sRainCommand;

    wtx.hashBoinc = sRain;
    int64_t totalAmount = 0;
    double dTotalToSend = 0;
    std::vector<std::string> vRecipients = split(sRecipients.c_str(),"<ROW>");
    LogPrintf("Creating Rain transaction with %" PRId64 " recipients. ", vRecipients.size());

    for (unsigned int i = 0; i < vRecipients.size(); i++)
    {
        std::string sRow = vRecipients[i];
        std::vector<std::string> vReward = split(sRow.c_str(),"<COL>");

        if (vReward.size() > 1)
        {
            std::string sAddress = vReward[0];
            std::string sAmount = vReward[1];

            if (sAddress.length() > 10 && sAmount.length() > 0)
            {
                double dAmount = RoundFromString(sAmount,4);
                if (dAmount > 0)
                {
                    CBitcoinAddress address(sAddress);
                    if (!address.IsValid())
                        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ")+sAddress);

                    if (setAddress.count(address))
                        throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+sAddress);

                    setAddress.insert(address);
                    dTotalToSend += dAmount;
                    int64_t nAmount = AmountFromDouble(dAmount);
                    CScript scriptPubKey;
                    scriptPubKey.SetDestination(address.Get());
                    totalAmount += nAmount;
                    vecSend.push_back(make_pair(scriptPubKey, nAmount));
                }
            }
        }
    }

    EnsureWalletIsUnlocked();
    // Check funds
    double dBalance = GetTotalBalance();

    if (dTotalToSend > dBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");
    // Send
    CReserveKey keyChange(pwalletMain);
    int64_t nFeeRequired = 0;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired);
    LogPrintf("Transaction Created.");

    if (!fCreated)
    {
        if (totalAmount + nFeeRequired > pwalletMain->GetBalance())
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction creation failed");
    }

    LogPrintf("Committing.");
    // Rain the recipients
    if (!pwalletMain->CommitTransaction(wtx, keyChange))
    {
        LogPrintf("Commit failed.");

        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");
    }
    std::string sNarr = "Rain successful:  Sent " + wtx.GetHash().GetHex() + ".";
    LogPrintf("Success %s",sNarr.c_str());
    return sNarr;
}

bool MemorizeMessage(const CTransaction &tx)
{
    const std::string &msg = tx.hashBoinc;
    const int64_t &nTime = tx.nTime;
    if (msg.empty()) return false;
    bool fMessageLoaded = false;

    if (!Contains(msg,"<MT>"))
        return false;

    std::string sMessageType      = ExtractXML(msg,"<MT>","</MT>");
    std::string sMessageKey       = ExtractXML(msg,"<MK>","</MK>");
    std::string sMessageValue     = ExtractXML(msg,"<MV>","</MV>");
    std::string sMessageAction    = ExtractXML(msg,"<MA>","</MA>");
    std::string sSignature        = ExtractXML(msg,"<MS>","</MS>");
    std::string sMessagePublicKey = ExtractXML(msg,"<MPK>","</MPK>");

    if (sMessageType.empty() || sMessageKey.empty() || sMessageValue.empty() || sMessageAction.empty()  || sSignature.empty())
        return false;

    if (sMessageType=="beacon" && Contains(sMessageValue,"INVESTOR"))
        return false;

    if (sMessageType=="superblock")
        return false;

    //Verify sig first
    if(!CheckMessageSignature(sMessageAction,sMessageType,sMessageType+sMessageKey+sMessageValue,
                              sSignature,sMessagePublicKey))
       return false;

    if (sMessageAction=="A")
    {
        /* With this we allow verifying blocks with stupid beacon */
        if("beacon"==sMessageType)
        {
            std::string out_cpid;
            std::string out_address;
            std::string out_publickey;
            GetBeaconElements(sMessageValue, out_cpid, out_address, out_publickey);
            WriteCache("beaconalt",sMessageKey+"."+ToString(nTime),out_publickey,nTime);
        }

        WriteCache(sMessageType,sMessageKey,sMessageValue,nTime);
        if(fDebug10 && sMessageType=="beacon" ){
            LogPrintf("BEACON add %s %s %s", sMessageKey, DecodeBase64(sMessageValue), TimestampToHRDate(nTime));
        }
        fMessageLoaded = true;
        if (sMessageType=="poll")
        {
            if (Contains(sMessageKey,"[Foundation"))
            {
                msPoll = "Foundation Poll: " + sMessageKey.substr(0,80);
            }
            else
            {
                msPoll = "Poll: " + sMessageKey.substr(0,80);
            }
        }
    }
    else if(sMessageAction=="D")
    {
        if (fDebug10) LogPrintf("Deleting key type %s Key %s Value %s", sMessageType, sMessageKey, sMessageValue);
        if(fDebug10 && sMessageType=="beacon" ){
            LogPrintf("BEACON DEL %s - %s", sMessageKey, TimestampToHRDate(nTime));
        }
        DeleteCache(sMessageType,sMessageKey);
        fMessageLoaded = true;
    }
    // If this is a boinc project, load the projects into the coin:
    if (sMessageType=="project" || sMessageType=="projectmapping")
    {
        //Reserved
        fMessageLoaded = true;
    }

    if(fDebug)
        WriteCache("TrxID;"+sMessageType,sMessageKey,tx.GetHash().GetHex(),nTime);

    return fMessageLoaded;
}
