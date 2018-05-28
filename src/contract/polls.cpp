#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string.hpp>

#include "main.h"
#include "polls.h"
#include "contract.h"
#include "rpcclient.h"
#include "rpcserver.h"
#include "appcache.h"
#include "cpid.h"
#include "init.h" // for pwalletMain
#include "block.h"

double GetTotalBalance();
std::string TimestampToHRDate(double dtm);
double CoinToDouble(double surrogate);
double DoubleFromAmount(int64_t amount);
std::string PubKeyToAddress(const CScript& scriptPubKey);
bool GetEarliestStakeTime(std::string grcaddress, std::string cpid);
CBlockIndex* GetHistoricalMagnitude(std::string cpid);
StructCPID GetLifetimeCPID(const std::string& cpid, const std::string& sFrom);
bool WalletOutOfSync();

/*std::map<int,std::string> ShareType =
{
    std::pair <int, std::string> (1,"Magnitude"),
    std::pair <int, std::string> (2,"Balance"),
    std::pair <int, std::string> (3,"Mangitude+Balance")
    std::pair <int, std::string> (4,"CPID Count")
    std::pair <int, std::string> (5,"Participants")
};*/

std::string GetShareType(double dShareType)
{
    if (dShareType == 1) return "Magnitude";
    if (dShareType == 2) return "Balance";
    if (dShareType == 3) return "Magnitude+Balance";
    if (dShareType == 4) return "CPID Count";
    if (dShareType == 5) return "Participants";
    return "?";
}

std::pair<std::string, std::string> CreatePollContract(std::string sTitle, int days, std::string sQuestion, std::string sAnswers, int iSharetype, std::string sURL)
{
    if (pwalletMain->IsLocked())
        return std::make_pair("Error", "Please fully unlock the wallet first.");
    else if (fWalletUnlockStakingOnly)
        return std::make_pair("Error", "Wallet is unlocked for staking only, needs to be fully unlocked for voting");
    else if (WalletOutOfSync())
        return std::make_pair("Error", "Wallet is not synced, you need to be synced in order to create a poll");

    if (sTitle.empty() || sQuestion.empty() || sAnswers.empty() || sURL.empty())
    {
        return std::make_pair("Error", "Must specify a poll title, question, answers, and URL\n");
    }
    else if (days < 7)
        return std::make_pair("Error", "Minimum duration is 7 days; please specify a longer poll duration.");
    else
    {
        double nBalance = GetTotalBalance();
        if (nBalance < 100000)
            return std::make_pair("Error", "You must have a balance > 100,000 GRC to create a poll.  Please post the desired poll on https://cryptocurrencytalk.com/forum/464-gridcoin-grc/ or https://github.com/gridcoin-community/Gridcoin-Tasks/issues/48, for project delisting see https://github.com/gridcoin-community/Gridcoin-Tasks/issues/194");
        else
        {
            if (iSharetype != 1 && iSharetype != 2 && iSharetype != 3 && iSharetype != 4 && iSharetype != 5)
                return std::make_pair("Error", "You must specify a value of 1, 2, 3, 4 or 5 for the sSharetype.");
            else
            {
                std::string expiration = RoundToString(GetAdjustedTime() + (days*86400), 0);
                std::string contract = "<TITLE>" + sTitle + "</TITLE><DAYS>" + std::to_string(days) + "</DAYS><QUESTION>" + sQuestion + "</QUESTION><ANSWERS>" + sAnswers + "</ANSWERS><SHARETYPE>" + std::to_string(iSharetype) + "</SHARETYPE><URL>" + sURL + "</URL><EXPIRATION>" + expiration + "</EXPIRATION>";
                std::string result = AddContract("poll", sTitle, contract);
                return std::make_pair("Success",result);
            }
        }
    }
}

std::pair<std::string, std::string> CreateVoteContract(std::string sTitle, std::string sAnswer)
{
    if (sTitle.empty() || sAnswer.empty())
        return std::make_pair("Error", "Must specify a poll title and answers\n");
    if (pwalletMain->IsLocked())
        return std::make_pair("Error", "Please fully unlock the wallet first.");
    else if (fWalletUnlockStakingOnly)
        return std::make_pair("Error", "Wallet is unlocked for staking only, needs to be fully unlocked for voting");
    else if (WalletOutOfSync())
        return std::make_pair("Error", "Wallet is not synced, you need to be synced in order to place a vote");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    //Verify the Existence of the poll, the acceptability of the answer, and the expiration of the poll: (EXIST, EXPIRED, ACCEPTABLE)
    //If polltype == 1, use magnitude, if 2 use Balance, if 3 use hybrid:
    //6-20-2015
    double nBalance = GetTotalBalance();
    uint256 hashRand = GetRandHash();
    std::string email = GetArgument("email", "NA");
    boost::to_lower(email);
    GlobalCPUMiningCPID.email = email;
    GlobalCPUMiningCPID.lastblockhash = GlobalCPUMiningCPID.cpidhash;
    if (!PollExists(sTitle))
        return std::make_pair("Error", "Poll does not exist.");
    if (PollExpired(sTitle))
        return std::make_pair("Error", "Sorry, Poll is already expired.");
    if (!PollAcceptableAnswer(sTitle, sAnswer))
    {
        std::string acceptable_answers = PollAnswers(sTitle);
        return std::make_pair("Error", "Sorry, Answer " + sAnswer + " is not one of the acceptable answers, allowable answers are: " + acceptable_answers + ".  If you are voting multiple choice, please use a semicolon delimited vote string such as : 'dog;cat'.");
    }
    std::string sParam = SerializeBoincBlock(GlobalCPUMiningCPID, pindexBest->nVersion);
    std::string GRCAddress = DefaultWalletAddress();
    StructCPID structMag = GetInitializedStructCPID2(GlobalCPUMiningCPID.cpid, mvMagnitudes);
    double dmag = structMag.Magnitude;
    double poll_duration = PollDuration(sTitle) * 86400;

    // Prevent Double Voting
    std::string cpid1 = GlobalCPUMiningCPID.cpid;
    std::string GRCAddress1 = DefaultWalletAddress();
    GetEarliestStakeTime(GRCAddress1, cpid1);
    double cpid_age = GetAdjustedTime() - ReadCache("global", "nCPIDTime").timestamp;
    double stake_age = GetAdjustedTime() - ReadCache("global", "nGRCTime").timestamp;

    StructCPID structGRC = GetInitializedStructCPID2(GRCAddress, mvMagnitudes);
    LogPrintf("CPIDAge %f, StakeAge %f, Poll Duration %f \r\n", cpid_age, stake_age, poll_duration);
    double dShareType= RoundFromString(GetPollXMLElementByPollTitle(sTitle, "<SHARETYPE>", "</SHARETYPE>"), 0);

    // Share Type 1 == "Magnitude"
    // Share Type 2 == "Balance"
    // Share Type 3 == "Both"
    if (cpid_age < poll_duration) dmag = 0;
    if (stake_age < poll_duration) nBalance = 0;
    if ((dShareType == 1) && cpid_age < poll_duration)
        return std::make_pair("Error", "Sorry, When voting in a magnitude poll, your CPID must be older than the poll duration.");
    else if (dShareType == 2 && stake_age < poll_duration)
        return std::make_pair("Error", "Sorry, When voting in a Balance poll, your stake age must be older than the poll duration.");
    else if (dShareType == 3 && stake_age < poll_duration && cpid_age < poll_duration)
        return std::make_pair("Error", "Sorry, When voting in a Both Share Type poll, your stake age Or your CPID age must be older than the poll duration.");
    else
    {
        std::string voter = "<CPIDV2>"+GlobalCPUMiningCPID.cpidv2 + "</CPIDV2><CPID>"
                + GlobalCPUMiningCPID.cpid + "</CPID><GRCADDRESS>" + GRCAddress + "</GRCADDRESS><RND>"
                + hashRand.GetHex() + "</RND><BALANCE>" + RoundToString(nBalance,2)
                + "</BALANCE><MAGNITUDE>" + RoundToString(dmag,0) + "</MAGNITUDE>";
        // Add the provable balance and the provable magnitude - this goes into effect July 1 2017
        voter += GetProvableVotingWeightXML();
        std::string pk = sTitle + ";" + GRCAddress + ";" + GlobalCPUMiningCPID.cpid;
        std::string contract = "<TITLE>" + sTitle + "</TITLE><ANSWER>" + sAnswer + "</ANSWER>" + voter;
        std::string result = AddContract("vote",pk,contract);
        std::string narr = "Your CPID weight is " + RoundToString(dmag,0) + " and your Balance weight is " + RoundToString(nBalance,0) + ".";
        return std::make_pair("Success", narr + " " + "Your vote has been cast for topic " + sTitle + ": With an Answer of " + sAnswer + ": " + result.c_str());
    }
}

std::string GetPollContractByTitle(std::string objecttype, std::string title)
{
    for(const auto& item : ReadCacheSection(objecttype))
    {
        const std::string& contract = item.second.value;
        const std::string& PollTitle = ExtractXML(contract,"<TITLE>","</TITLE>");
        if(boost::iequals(PollTitle, title))
            return contract;
    }
    return std::string();
}

bool PollExists(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    return contract.length() > 10 ? true : false;
}

bool PollExpired(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    double expiration = RoundFromString(ExtractXML(contract,"<EXPIRATION>","</EXPIRATION>"),0);
    return (expiration < (double)GetAdjustedTime()) ? true : false;
}


bool PollCreatedAfterSecurityUpgrade(std::string pollname)
{
    // If the expiration is after July 1 2017, use the new security features.
    std::string contract = GetPollContractByTitle("poll",pollname);
    double expiration = RoundFromString(ExtractXML(contract,"<EXPIRATION>","</EXPIRATION>"),0);
    return (expiration > 1498867200) ? true : false;
}


double PollDuration(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    double days = RoundFromString(ExtractXML(contract,"<DAYS>","</DAYS>"),0);
    return days;
}

double PollCalculateShares(std::string contract, double sharetype, double MoneySupplyFactor, unsigned int VoteAnswerCount)
{
    std::string address = ExtractXML(contract,"<GRCADDRESS>","</GRCADDRESS>");
    std::string cpid = ExtractXML(contract,"<CPID>","</CPID>");
    double magnitude = ReturnVerifiedVotingMagnitude(contract,PollCreatedAfterSecurityUpgrade(contract));
    double balance = ReturnVerifiedVotingBalance(contract,PollCreatedAfterSecurityUpgrade(contract));
    if (VoteAnswerCount < 1) VoteAnswerCount=1;
    if (sharetype==1) return magnitude/VoteAnswerCount;
    if (sharetype==2) return balance/VoteAnswerCount;
    if (sharetype==3)
    {
        // https://github.com/gridcoin/Gridcoin-Research/issues/87#issuecomment-253999878
        // Researchers weight is Total Money Supply / 5.67 * Magnitude
        double UserWeightedMagnitude = (MoneySupplyFactor/5.67) * magnitude;
        return (UserWeightedMagnitude+balance) / VoteAnswerCount;
    }
    if (sharetype==4)
    {
        if (magnitude > 0) return 1;
        return 0;
    }
    if (sharetype==5)
    {
        if (address.length() > 5) return 1;
        return 0;
    }
    return 0;
}


double VotesCount(std::string pollname, std::string answer, double sharetype, double& out_participants)
{
    double total_shares = 0;
    out_participants = 0;
    double MoneySupplyFactor = GetMoneySupplyFactor();
    for(const auto& item : ReadCacheSection("vote"))
    {
        const std::string& contract = item.second.value;
        const std::string& Title = ExtractXML(contract,"<TITLE>","</TITLE>");
        const std::string& VoterAnswer = ExtractXML(contract,"<ANSWER>","</ANSWER>");
        const std::vector<std::string>& vVoterAnswers = split(VoterAnswer.c_str(),";");
        for (const std::string& voterAnswers : vVoterAnswers)
        {
            if (boost::iequals(pollname, Title) && boost::iequals(answer, voterAnswers))
            {
                double shares = PollCalculateShares(contract, sharetype, MoneySupplyFactor, vVoterAnswers.size());
                total_shares += shares;
                out_participants += 1.0 / vVoterAnswers.size();
            }
        }
    }
    return total_shares;
}



std::string GetPollXMLElementByPollTitle(std::string pollname, std::string XMLElement1, std::string XMLElement2)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    std::string sElement = ExtractXML(contract,XMLElement1,XMLElement2);
    return sElement;
}


bool PollAcceptableAnswer(std::string pollname, std::string answer)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    std::string answers = ExtractXML(contract,"<ANSWERS>","</ANSWERS>");
    std::vector<std::string> vAnswers = split(answers.c_str(),";");
    //Allow multiple choice voting:
    std::vector<std::string> vUserAnswers = split(answer.c_str(),";");
    for (unsigned int x = 0; x < vUserAnswers.size(); x++)
    {
        bool bFoundAnswer = false;
        for (unsigned int i = 0; i < vAnswers.size(); i++)
        {
            boost::to_lower(vAnswers[i]); //Contains Poll acceptable answers
            std::string sUserAnswer = vUserAnswers[x];
            boost::to_lower(sUserAnswer);
            if (sUserAnswer == vAnswers[i])
            {
                bFoundAnswer=true;
                break;
            }
        }
        if (!bFoundAnswer) return false;
    }
    return true;
}

std::string PollAnswers(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    std::string answers = ExtractXML(contract,"<ANSWERS>","</ANSWERS>");
    return answers;

}
std::string GetProvableVotingWeightXML()
{
    std::string sXML = "<PROVABLEMAGNITUDE>";
    //Retrieve the historical magnitude
    if (IsResearcher(msPrimaryCPID))
    {
        StructCPID st1 = GetLifetimeCPID(msPrimaryCPID,"ProvableMagnitude()");
        CBlockIndex* pHistorical = GetHistoricalMagnitude(msPrimaryCPID);
        if (pHistorical->nHeight > 1 && pHistorical->nMagnitude > 0)
        {
            std::string sBlockhash = pHistorical->GetBlockHash().GetHex();
            std::string sError;
            std::string sSignature;
            bool bResult = SignBlockWithCPID(msPrimaryCPID, pHistorical->GetBlockHash().GetHex(), sSignature, sError);
            // Just because below comment it'll keep in line with that
            if (!bResult)
                sSignature = sError;
            // Find the Magnitude from the last staked block, within the last 6 months, and ensure researcher has a valid current beacon (if the beacon is expired, the signature contain an error message)
            sXML += "<CPID>" + msPrimaryCPID + "</CPID><INNERMAGNITUDE>"
                    + RoundToString(pHistorical->nMagnitude,2) + "</INNERMAGNITUDE>" +
                    "<HEIGHT>" + ToString(pHistorical->nHeight)
                    + "</HEIGHT><BLOCKHASH>" + sBlockhash + "</BLOCKHASH><SIGNATURE>" + sSignature + "</SIGNATURE>";
        }
    }
    sXML += "</PROVABLEMAGNITUDE>";

    std::vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
    std::string sRow = "";
    double dTotal = 0;
    double dBloatThreshhold = 100;
    double dCurrentItemCount = 0;
    double dItemBloatThreshhold = 50;
    // Iterate unspent coins from transactions owned by me that total over 100GRC (this prevents XML bloat)
    sXML += "<PROVABLEBALANCE>";
    for (auto const& out : vecOutputs)
    {
        int64_t nValue = out.tx->vout[out.i].nValue;
        const CScript& pk = out.tx->vout[out.i].scriptPubKey;
        UniValue entry(UniValue::VOBJ);
        CTxDestination address;
        if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
        {
            if (CoinToDouble(nValue) > dBloatThreshhold)
            {
                std::string sScriptPubKey1 = HexStr(pk.begin(), pk.end());
                std::string strAddress=CBitcoinAddress(address).ToString();
                CKeyID keyID;
                const CBitcoinAddress& bcAddress = CBitcoinAddress(address);
                if (bcAddress.GetKeyID(keyID))
                {
                    bool IsCompressed;
                    CKey vchSecret;
                    if (pwalletMain->GetKey(keyID, vchSecret))
                    {
                        // Here we use the secret key to sign the coins, then we abandon the key.
                        CSecret csKey = vchSecret.GetSecret(IsCompressed);
                        CKey keyInner;
                        keyInner.SetSecret(csKey,IsCompressed);
                        std::string private_key = CBitcoinSecret(csKey,IsCompressed).ToString();
                        std::string public_key = HexStr(keyInner.GetPubKey().Raw());
                        std::vector<unsigned char> vchSig;
                        keyInner.Sign(out.tx->GetHash(), vchSig);
                        // Sign the coins we own
                        std::string sSig(vchSig.begin(), vchSig.end());
                        // Increment the total balance weight voting ability
                        dTotal += CoinToDouble(nValue);
                        sRow = "<ROW><TXID>" + out.tx->GetHash().GetHex() + "</TXID>" +
                                "<AMOUNT>" + RoundToString(CoinToDouble(nValue),2) + "</AMOUNT>" +
                                "<POS>" + RoundToString((double)out.i,0) + "</POS>" +
                                "<PUBKEY>" + public_key + "</PUBKEY>" +
                                "<SCRIPTPUBKEY>" + sScriptPubKey1  + "</SCRIPTPUBKEY>" +
                                "<SIG>" + EncodeBase64(sSig) + "</SIG>" +
                                "<MESSAGE></MESSAGE></ROW>";
                        sXML += sRow;
                        dCurrentItemCount++;
                        if (dCurrentItemCount >= dItemBloatThreshhold)
                            break;
                    }
                }
            }
        }
    }
    sXML += "<TOTALVOTEDBALANCE>" + RoundToString(dTotal,2) + "</TOTALVOTEDBALANCE>";
    sXML += "</PROVABLEBALANCE>";
    return sXML;
}


double ReturnVerifiedVotingBalance(std::string sXML, bool bCreatedAfterSecurityUpgrade)
{
    std::string sPayload = ExtractXML(sXML,"<PROVABLEBALANCE>","</PROVABLEBALANCE>");
    double dTotalVotedBalance = RoundFromString(ExtractXML(sPayload,"<TOTALVOTEDBALANCE>","</TOTALVOTEDBALANCE>"),2);
    double dLegacyBalance = RoundFromString(ExtractXML(sXML,"<BALANCE>","</BALANCE>"),0);

    if (fDebug10) LogPrintf(" \n Total Voted Balance %f, Legacy Balance %f \n",(float)dTotalVotedBalance,(float)dLegacyBalance);
    if (!bCreatedAfterSecurityUpgrade) return dLegacyBalance;

    double dCounted = 0;
    std::vector<std::string> vXML= split(sPayload.c_str(),"<ROW>");
    for (unsigned int x = 0; x < vXML.size(); x++)
    {
        // Prove the contents of the XML as a 3rd party
        CTransaction tx2;
        uint256 hashBlock = 0;
        uint256 uTXID(ExtractXML(vXML[x],"<TXID>","</TXID>"));
        std::string sAmt = ExtractXML(vXML[x],"<AMOUNT>","</AMOUNT>");
        std::string sPos = ExtractXML(vXML[x],"<POS>","</POS>");
        std::string sXmlSig = ExtractXML(vXML[x],"<SIG>","</SIG>");
        std::string sXmlMsg = ExtractXML(vXML[x],"<MESSAGE>","</MESSAGE>");
        std::string sScriptPubKeyXml = ExtractXML(vXML[x],"<SCRIPTPUBKEY>","</SCRIPTPUBKEY>");
        int32_t iPos = RoundFromString(sPos,0);
        std::string sPubKey = ExtractXML(vXML[x],"<PUBKEY>","</PUBKEY>");
        if (!sPubKey.empty() && !sAmt.empty() && !sPos.empty() && uTXID > 0)
        {
            if (GetTransaction(uTXID, tx2, hashBlock))
            {
                if (iPos >= 0 && iPos < (int32_t) tx2.vout.size())
                {
                    int64_t nValue2 = tx2.vout[iPos].nValue;
                    const CScript& pk2 = tx2.vout[iPos].scriptPubKey;
                    CTxDestination address2;
                    std::string sVotedPubKey = HexStr(pk2.begin(), pk2.end());
                    std::string sVotedGRCAddress = CBitcoinAddress(address2).ToString();
                    std::string sCoinOwnerAddress = PubKeyToAddress(pk2);
                    double dAmount = CoinToDouble(nValue2);
                    if (ExtractDestination(tx2.vout[iPos].scriptPubKey, address2))
                    {
                        if (sScriptPubKeyXml == sVotedPubKey && RoundToString(dAmount,2) == sAmt)
                        {
                            UniValue entry(UniValue::VOBJ);
                            entry.pushKV("Audited Amount",ValueFromAmount(nValue2));
                            std::string sDecXmlSig = DecodeBase64(sXmlSig);
                            CKey keyVerify;
                            if (keyVerify.SetPubKey(ParseHex(sPubKey)))
                            {
                                std::vector<unsigned char> vchMsg1 = std::vector<unsigned char>(sXmlMsg.begin(), sXmlMsg.end());
                                std::vector<unsigned char> vchSig1 = std::vector<unsigned char>(sDecXmlSig.begin(), sDecXmlSig.end());
                                bool bValid = keyVerify.Verify(uTXID,vchSig1);
                                // Unspent Balance is proven to be owned by the voters public key, count the vote
                                if(bValid) dCounted += dAmount;
                            }
                        }
                    }
                }
            }
        }
    }
    return dCounted;
}

double ReturnVerifiedVotingMagnitude(std::string sXML, bool bCreatedAfterSecurityUpgrade)
{
    double dLegacyMagnitude  = RoundFromString(ExtractXML(sXML,"<MAGNITUDE>","</MAGNITUDE>"),2);
    if (!bCreatedAfterSecurityUpgrade) return dLegacyMagnitude;

    std::string sMagXML = ExtractXML(sXML,"<PROVABLEMAGNITUDE>","</PROVABLEMAGNITUDE>");
    std::string sMagnitude = ExtractXML(sMagXML,"<INNERMAGNITUDE>","</INNERMAGNITUDE>");
    std::string sXmlSigned = ExtractXML(sMagXML,"<SIGNATURE>","</SIGNATURE>");
    std::string sXmlBlockHash = ExtractXML(sMagXML,"<BLOCKHASH>","</BLOCKHASH>");
    std::string sXmlCPID = ExtractXML(sMagXML,"<CPID>","</CPID>");
    if (!sXmlBlockHeight.empty() && !sMagnitude.empty() && !sXmlSigned.empty())
    {
        CBlockIndex* pblockindexMagnitude = mapBlockIndex[uint256(sXmlBlockHash)];
        if (pblockindexMagnitude)
        {
            bool fResult = VerifyCPIDSignature(sXmlCPID, sXmlBlockHash, sXmlSigned);
            bool fAudited = (RoundFromString(RoundToString(pblockindexMagnitude->nMagnitude,2),0)==RoundFromString(sMagnitude,0) && fResult);
            if (fAudited) return (double)pblockindexMagnitude->nMagnitude;
        }
    }
    return 0;
}

double GetMoneySupplyFactor()
{
    StructCPID structcpid = mvNetwork["NETWORK"];
    double TotalCPIDS = mvMagnitudes.size();
    double AvgMagnitude = structcpid.NetworkAvgMagnitude;
    double TotalNetworkMagnitude = TotalCPIDS*AvgMagnitude;
    if (TotalNetworkMagnitude < 100) TotalNetworkMagnitude=100;
    double MoneySupply = DoubleFromAmount(pindexBest->nMoneySupply);
    double Factor = (MoneySupply/TotalNetworkMagnitude+.01);
    return Factor;
}

UniValue getjsonpoll(bool bDetail, bool includeExpired, std::string byTitle)
{
    UniValue aPolls(UniValue::VARR);
    UniValue oPoll(UniValue::VOBJ);
    UniValue oAnswer(UniValue::VOBJ);
    UniValue aAnswer(UniValue::VARR);
    std::vector<polling::Poll> vPolls = GetPolls(bDetail, includeExpired, byTitle);
    for(const auto& iterPoll: vPolls)
    {
        oPoll.push_back(Pair("title", iterPoll.title));
        oPoll.push_back(Pair("pollnumber", iterPoll.pollnumber));
        oPoll.push_back(Pair("question", iterPoll.question));
        oPoll.push_back(Pair("expiration", iterPoll.expiration));
        oPoll.push_back(Pair("url", iterPoll.url));
        oPoll.push_back(Pair("sharetype", iterPoll.sharetype));
        if(bDetail)
        {
            for(const auto& iterAnswer: iterPoll.answers)
            {
                oAnswer.push_back(Pair("answer", iterAnswer.answer));
                oAnswer.push_back(Pair("shares", iterAnswer.shares));
                oAnswer.push_back(Pair("participants", iterAnswer.participants));
                aAnswer.push_back(oAnswer);
                oAnswer.clear();
                oAnswer.shrink_to_fit();
            }
            oPoll.push_back(Pair("bestAnswer", iterPoll.best_answer));
            oPoll.push_back(Pair("totalShares", iterPoll.total_shares));
            oPoll.push_back(Pair("totalParticipants", iterPoll.total_participants));
            oPoll.push_back(Pair("highestShare", iterPoll.highest_share));
            oPoll.push_back(Pair("answers", aAnswer));
        }
        aPolls.push_back(oPoll);
        oPoll.clear();
        oPoll.shrink_to_fit();
    }
    return aPolls;
}

std::vector<polling::Poll> GetPolls(bool bDetail, bool includeExpired, std::string byTitle)
{
    std::vector<polling::Poll> vPolls;
    std::string::size_type longestanswer = 0;
    polling::Poll poll;
    polling::Vote vote;
    double iPollNumber = 0;
    poll.pollnumber = 0;
    boost::to_lower(byTitle);
    double dShares=0;

    for( const auto& item: ReadCacheSection("poll"))
    {
        poll.title = boost::to_lower_copy(item.first);
        if (PollExpired(poll.title) && !includeExpired) continue;
        if (!byTitle.empty() && byTitle != poll.title) continue;

        const std::string& contract = item.second.value;
        poll.expiration = ExtractXML(contract,"<EXPIRATION>","</EXPIRATION>");
        poll.question = ExtractXML(contract,"<QUESTION>","</QUESTION>");
        poll.sAnswers = ExtractXML(contract,"<ANSWERS>","</ANSWERS>");
        poll.sharetype = ExtractXML(contract,"<SHARETYPE>","</SHARETYPE>");
        poll.url= ExtractXML(contract,"<URL>","</URL>");
        if((poll.title.length()>128) &&
                (poll.expiration.length()>64) &&
                (poll.question.length()>4096) &&
                (poll.sAnswers.length()>8192) &&
                (poll.sharetype.length()>64) &&
                (poll.url.length()>256)  )
            continue;

        const std::vector<std::string>& vAnswers = split(poll.sAnswers.c_str(),";");

        for (const std::string& answer : vAnswers)
            longestanswer = std::max( longestanswer, answer.length() );

        if( longestanswer>128 )
            continue;

        iPollNumber++;
        poll.pollnumber++;
        poll.total_participants = 0;
        poll.total_shares=0;
        poll.highest_share = 0;
        poll.expiration = TimestampToHRDate(RoundFromString(poll.expiration,0));
        poll.sharetype = GetShareType(RoundFromString(poll.sharetype,0));

        if (bDetail)
        {
            size_t i = 0;
            for (const std::string& answer : vAnswers)
            {
                vote.answer = answer;
                vote.shares = VotesCount(poll.title, answer, RoundFromString(poll.sharetype,0),vote.participants);
                if (vote.shares > poll.highest_share)
                {
                    poll.highest_share = vote.shares;
                    poll.best_answer = answer;
                }

                poll.total_participants += vote.participants;
                poll.total_shares += vote.shares;
            }
            if (poll.total_participants < 3) poll.best_answer = "";

            poll.answers.push_back(vote);
            vPolls.push_back(poll);
        }
    }
    return vPolls;
}

UniValue GetJSONPollsReport(bool bDetail, std::string QueryByTitle, std::string& out_export, bool IncludeExpired)
{
    //Title,ExpirationDate, Question, Answers, ShareType(1=Magnitude,2=Balance,3=Both)
    UniValue results(UniValue::VARR);
    UniValue entry(UniValue::VOBJ);
    entry.pushKV("Polls","Polls Report " + QueryByTitle);
    std::string rows;
    std::string row;
    double iPollNumber = 0;
    double total_participants = 0;
    double total_shares = 0;
    boost::to_lower(QueryByTitle);
    std::string sExport;
    std::string sExportRow;
    out_export.clear();

    for(const auto& item : ReadCacheSection("poll"))
    {
        const std::string& title = boost::to_lower_copy(item.first);
        const std::string& contract = item.second.value;
        std::string Expiration = ExtractXML(contract,"<EXPIRATION>","</EXPIRATION>");
        std::string Question = ExtractXML(contract,"<QUESTION>","</QUESTION>");
        std::string Answers = ExtractXML(contract,"<ANSWERS>","</ANSWERS>");
        std::string ShareType = ExtractXML(contract,"<SHARETYPE>","</SHARETYPE>");
        std::string sURL = ExtractXML(contract,"<URL>","</URL>");
        if (!PollExpired(title) || IncludeExpired)
        {
            if (QueryByTitle.empty() || QueryByTitle == title)
            {

                if( (title.length()>128) &&
                        (Expiration.length()>64) &&
                        (Question.length()>4096) &&
                        (Answers.length()>8192) &&
                        (ShareType.length()>64) &&
                        (sURL.length()>256)  )
                    continue;

                const std::vector<std::string>& vAnswers = split(Answers.c_str(),";");

                std::string::size_type longestanswer = 0;
                for (const std::string& answer : vAnswers)
                    longestanswer = std::max( longestanswer, answer.length() );

                if( longestanswer>128 )
                    continue;

                iPollNumber++;
                total_participants = 0;
                total_shares=0;
                std::string BestAnswer;
                double highest_share = 0;
                std::string ExpirationDate = TimestampToHRDate(RoundFromString(Expiration,0));
                std::string sShareType = GetShareType(RoundFromString(ShareType,0));
                std::string TitleNarr = "Poll #" + RoundToString((double)iPollNumber,0)
                        + " (" + ExpirationDate + " ) - " + sShareType;

                entry.pushKV(TitleNarr,title);
                sExportRow = "<POLL><URL>" + sURL + "</URL><TITLE>" + title + "</TITLE><EXPIRATION>" + ExpirationDate + "</EXPIRATION><SHARETYPE>" + sShareType + "</SHARETYPE><QUESTION>" + Question + "</QUESTION><ANSWERS>"+Answers+"</ANSWERS>";

                if (bDetail)
                {
                    entry.pushKV("Question",Question);
                    sExportRow += "<ARRAYANSWERS>";
                    size_t i = 0;
                    for (const std::string& answer : vAnswers)
                    {
                        double participants=0;
                        double dShares = VotesCount(title, answer, RoundFromString(ShareType,0),participants);
                        if (dShares > highest_share)
                        {
                            highest_share = dShares;
                            BestAnswer = answer;
                        }

                        entry.pushKV("#" + ToString(++i) + " [" + RoundToString(participants,3) + "]. " + answer,dShares);
                        total_participants += participants;
                        total_shares += dShares;
                        sExportRow += "<RESERVED></RESERVED><ANSWERNAME>" + answer + "</ANSWERNAME><PARTICIPANTS>" + RoundToString(participants,0) + "</PARTICIPANTS><SHARES>" + RoundToString(dShares,0) + "</SHARES>";
                    }
                    sExportRow += "</ARRAYANSWERS>";

                    //Totals:
                    entry.pushKV("Participants",total_participants);
                    entry.pushKV("Total Shares",total_shares);
                    if (total_participants < 3) BestAnswer = "";

                    entry.pushKV("Best Answer",BestAnswer);
                    sExportRow += "<TOTALPARTICIPANTS>" + RoundToString(total_participants,0)
                            + "</TOTALPARTICIPANTS><TOTALSHARES>" + RoundToString(total_shares,0)
                            + "</TOTALSHARES><BESTANSWER>" + BestAnswer + "</BESTANSWER>";

                }
                sExportRow += "</POLL>";
                sExport += sExportRow;
            }
        }
    }

    results.push_back(entry);
    out_export = sExport;
    return results;
}

UniValue GetJsonVoteDetailsReport(std::string pollname)
{
    double total_shares = 0;
    double participants = 0;
    double MoneySupplyFactor = GetMoneySupplyFactor();

    UniValue results(UniValue::VARR);
    UniValue entry(UniValue::VOBJ);
    entry.pushKV("Votes","Votes Report " + pollname);
    entry.pushKV("MoneySupplyFactor",RoundToString(MoneySupplyFactor,2));

    // Add header
    entry.pushKV("GRCAddress,CPID,Question,Answer,ShareType,URL", "Shares");

    boost::to_lower(pollname);
    for(const auto& item : ReadCacheSection("vote"))
    {
        const std::string& contract = item.second.value;
        const std::string& Title = ExtractXML(contract,"<TITLE>","</TITLE>");
        if(boost::iequals(pollname, Title))
        {
            const std::string& OriginalContract = GetPollContractByTitle("poll",Title);
            const std::string& Question = ExtractXML(OriginalContract,"<QUESTION>","</QUESTION>");
            const std::string& GRCAddress = ExtractXML(contract,"<GRCADDRESS>","</GRCADDRESS>");
            const std::string& CPID = ExtractXML(contract,"<CPID>","</CPID>");

            double dShareType = RoundFromString(GetPollXMLElementByPollTitle(Title,"<SHARETYPE>","</SHARETYPE>"),0);
            std::string sShareType= GetShareType(dShareType);
            std::string sURL = ExtractXML(contract,"<URL>","</URL>");

            std::string Balance = ExtractXML(contract,"<BALANCE>","</BALANCE>");

            const std::string& VoterAnswer = boost::to_lower_copy(ExtractXML(contract,"<ANSWER>","</ANSWER>"));
            const std::vector<std::string>& vVoterAnswers = split(VoterAnswer.c_str(),";");
            for (const auto& answer : vVoterAnswers)
            {
                double shares = PollCalculateShares(contract, dShareType, MoneySupplyFactor, vVoterAnswers.size());
                total_shares += shares;
                participants += 1.0 / vVoterAnswers.size();
                const std::string& voter = GRCAddress + "," + CPID + "," + Question + "," + answer + "," + sShareType + "," + sURL;
                entry.pushKV(voter,RoundToString(shares,0));
            }
        }
    }

    entry.pushKV("Total Participants",RoundToString(participants,2));
    results.push_back(entry);
    return results;
}
