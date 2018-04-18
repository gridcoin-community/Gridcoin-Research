#include "appcache.h"
#include "main.h"
#include "bitcoinrpc.h"
#include "contract/contract.h"
#include "contract/polls.h"

#include <boost/filesystem.hpp>
#include <iostream>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <algorithm>

using namespace json_spirit;
double GetTotalBalance();
bool GetEarliestStakeTime(std::string grcaddress, std::string cpid);

Value addpoll(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 6)
        throw std::runtime_error(
                "addpoll <title> <days> <question> <answer1;answer2...> <sharetype> <url>\n"
                "\n"
                "<title> -----> The title for poll with no spaces. Use _ in between words\n"
                "<days> ------> The number of days the poll will run\n"
                "<question> --> The question with no spaces. Use _ in between words\n"
                "<answers> ---> The answers available for voter to choose from. Use - in between words and ; to seperate answers\n"
                "<sharetype> -> The share type of the poll; 1 = Magnitude 2 = Balance 3 = Magnitude + Balance 4 = CPID count 5 = Participant count\n"
                "<url> -------> The corresponding url for the poll\n"
                "\n"
                "Add a poll to the network; Requires 100K GRC balance\n");

    Object res;
    std::string sTitle = params[0].get_str();
    int iPollDays = params[1].get_int();
    std::string sQuestion = params[2].get_str();
    std::string sAnswers = params[3].get_str();
    int iShareType = params[4].get_int();
    std::string sURL = params[5].get_str();

	std::pair<std::string,std::string> ResultString = CreatePollContract(sTitle, iPollDays, sQuestion, sAnswers, iShareType, sURL);
    res.push_back(Pair(std::get<0>(ResultString),std::get<1>(ResultString)));
    return res;
}

Value listallpolls(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "listallpolls\n"
                "\n"
                "Lists all polls\n");

    LOCK(cs_main);

    std::string out1;
    Array res = GetJSONPollsReport(false, "", out1, true);

    return res;
}

Value listallpolldetails(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "listallpolldetails\n"
                "\n"
                "Lists all polls with details\n");

    LOCK(cs_main);

    std::string out1;
    Array res = GetJSONPollsReport(true, "", out1, true);

    return res;
}

Value listpolldetails(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "listpolldetails\n"
                "\n"
                "Lists poll details\n");

    LOCK(cs_main);

    std::string out1;
    Array res = GetJSONPollsReport(true, "", out1, false);

    return res;
}

Value listpollresults(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2 || params.size() < 1)
        throw std::runtime_error(
                "listpollresults <pollname> [bool:showexpired]\n"
                "\n"
                "<pollname> ----> name of the poll\n"
                "[showexpired] -> Optional; Default false\n"
                "\n"
                "Displays results for specified poll\n");

    LOCK(cs_main);

    Array res;
    bool bIncExpired = false;

    if (params.size() == 2)
        bIncExpired = params[1].get_bool();

    std::string Title1 = params[0].get_str();

    if (!PollExists(Title1))
    {
        Object result;

        result.push_back(Pair("Error", "Poll does not exist.  Please listpolls."));
        res.push_back(result);
    }
    else
    {
        std::string Title = params[0].get_str();
        std::string out1 = "";
        Array myPolls = GetJSONPollsReport(true, Title, out1, bIncExpired);
        res.push_back(myPolls);
    }

    return res;
}

Value listpolls(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "listpolls\n"
                "\n"
                "Lists polls\n");

    LOCK(cs_main);

    std::string out1;
    Array res = GetJSONPollsReport(false, "", out1, false);

    return res;
}

Value vote(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw std::runtime_error(
                "vote <title> <answer1;answer2...>\n"
                "\n"
                "<title -> Title of poll being voted on\n"
                "<answers> -> Answers chosen for specified poll seperated by ;\n"
                "\n"
                "Vote on a specific poll with specified answers\n");

    Object res;

    std::string sTitle = params[0].get_str();
    std::string sAnswer = params[1].get_str();

    std::pair<std::string,std::string> ResultString = CreateVoteContract(sTitle, sAnswer);
    res.push_back(Pair(std::get<0>(ResultString),std::get<1>(ResultString)));
    return res;


    return res;
}

Value votedetails(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
                "votedetails <pollname>]n"
                "\n"
                "<pollname> Specified poll name\n"
                "\n"
                "Displays vote details of a specified poll\n");

    Array res;

    std::string Title = params[0].get_str();

    if (!PollExists(Title))
    {
        Object results;

        results.push_back(Pair("Error", "Poll does not exist.  Please listpolls."));
        res.push_back(results);
    }

    else
    {
        LOCK(cs_main);

        Array myVotes = GetJsonVoteDetailsReport(Title);

        res.push_back(myVotes);
    }

    return res;
}



