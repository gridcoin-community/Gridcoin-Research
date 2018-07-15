#pragma once

#include <univalue.h>

#include <string>
#include <utility> //std::pair

namespace polling {

struct Vote {
    std::string answer;
    double shares;
    double participants;
};

struct Poll {
    std::string title;
    std::string question;
    std::string url;
    std::string expiration;
    std::string sharetype;
    std::string type;
    std::vector<Vote> answers;
    std::string sAnswers;
    double highest_share;
    double total_shares;
    double total_participants;
    std::string best_answer;
    int pollnumber;
};
};

std::pair<std::string, std::string> CreatePollContract(std::string sTitle, int days, std::string sQuestion, std::string sAnswers, int sSharetype, std::string sURL);

std::pair<std::string, std::string> CreateVoteContract(std::string sTitle, std::string sAnswer);

std::string GetPollContractByTitle(std::string objecttype, std::string title);

bool PollExists(std::string pollname);

bool PollExpired(std::string pollname);

bool PollCreatedAfterSecurityUpgrade(std::string pollname);

double PollDuration(std::string pollname);

double PollCalculateShares(std::string contract, double sharetype, double MoneySupplyFactor, unsigned int VoteAnswerCount);

double VotesCount(std::string pollname, std::string answer, double sharetype, double& out_participants);

std::string GetPollXMLElementByPollTitle(std::string pollname, std::string XMLElement1, std::string XMLElement2);

bool PollAcceptableAnswer(std::string pollname, std::string answer);

std::string PollAnswers(std::string pollname);

std::string GetProvableVotingWeightXML();

double ReturnVerifiedVotingBalance(std::string sXML, bool bCreatedAfterSecurityUpgrade);

double ReturnVerifiedVotingMagnitude(std::string sXML, bool bCreatedAfterSecurityUpgrade);

double GetMoneySupplyFactor();

UniValue getjsonpoll(bool bDetail, bool includeExpired, std::string byTitle);

std::vector<polling::Poll> GetPolls(bool bDetail, bool includeExpired, std::string byTitle);

UniValue GetJSONPollsReport(bool bDetail, std::string QueryByTitle, std::string& out_export, bool IncludeExpired);

UniValue GetJsonVoteDetailsReport(std::string pollname);

