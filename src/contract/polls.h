#pragma once

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_value.h"
#include "json/json_spirit_utils.h"

#include "main.h"
#include "bitcoinrpc.h"
#include "init.h" // for pwalletMain
#include "block.h"

#include <boost/filesystem.hpp>
#include <iostream>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <algorithm>

using namespace json_spirit;

struct Vote {
    // uint128_t cpid;
    std::string address;
    double shares;
};

struct Poll {
    std::string title;
    std::string url;
    // int64_t expiration;
    // PollType type;
    // std::map<std::string, std::vector> answers;
};

Value addpoll(const Array& params, bool fHelp);

Value listallpolls(const Array& params, bool fHelp);

Value listallpolldetails(const Array& params, bool fHelp);

Value listpolldetails(const Array& params, bool fHelp);

Value listpollresults(const Array& params, bool fHelp);

Value listpolls(const Array& params, bool fHelp);

Value vote(const Array& params, bool fHelp);

Value votedetails(const Array& params, bool fHelp);

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

std::string GetShareType(double dShareType);

double ReturnVerifiedVotingBalance(std::string sXML, bool bCreatedAfterSecurityUpgrade);

double ReturnVerifiedVotingMagnitude(std::string sXML, bool bCreatedAfterSecurityUpgrade);

double GetMoneySupplyFactor();

Array GetJSONPollsReport(bool bDetail, std::string QueryByTitle, std::string& out_export, bool IncludeExpired);

Array GetJsonVoteDetailsReport(std::string pollname);

