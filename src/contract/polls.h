#pragma once

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <string>
#include <utility> //std::pair

using namespace json_spirit;

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

std::string GetShareType(double dShareType);

double ReturnVerifiedVotingBalance(std::string sXML, bool bCreatedAfterSecurityUpgrade);

double ReturnVerifiedVotingMagnitude(std::string sXML, bool bCreatedAfterSecurityUpgrade);

double GetMoneySupplyFactor();

Array GetJSONPollsReport(bool bDetail, std::string QueryByTitle, std::string& out_export, bool IncludeExpired);

Array GetJsonVoteDetailsReport(std::string pollname);

