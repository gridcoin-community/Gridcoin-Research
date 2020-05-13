#include "appcache.h"
#include "main.h"
#include "rpcserver.h"
#include "rpcclient.h"
#include "contract/polls.h"

#include <utility>
#include <string>

UniValue addpoll(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 6)
        throw std::runtime_error(
                "addpoll <title> <days> <question> <answer1;answer2...> <sharetype> <url>\n"
                "\n"
                "<title> -----> The title for poll with no spaces. Use _ in between words\n"
                "<days> ------> The number of days the poll will run\n"
                "<question> --> The question with no spaces. Use _ in between words\n"
                "<answers> ---> The answers available for voter to choose from. Use - in between words and ; to separate answers\n"
                "<sharetype> -> The share type of the poll; 1 = Magnitude 2 = Balance 3 = Magnitude + Balance 4 = CPID count 5 = Participant count\n"
                "<url> -------> The corresponding url for the poll\n"
                "\n"
                "Add a poll to the network; Requires 100K GRC balance\n");

    UniValue res(UniValue::VOBJ);
    std::string sTitle = params[0].get_str();
    int iPollDays = params[1].get_int();
    std::string sQuestion = params[2].get_str();
    std::string sAnswers = params[3].get_str();
    int iShareType = params[4].get_int();
    std::string sURL = params[5].get_str();

    std::pair<std::string,std::string> ResultString = CreatePollContract(sTitle, iPollDays, sQuestion, sAnswers, iShareType, sURL);
    res.pushKV(std::get<0>(ResultString),std::get<1>(ResultString));
    return res;
}

UniValue listallpolls(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "listallpolls\n"
                "\n"
                "Lists all polls\n");

    LOCK(cs_main);
    std::string out1;
    UniValue res = getjsonpoll(false, true, "");
    return res;
}

UniValue listallpolldetails(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "listallpolldetails\n"
                "\n"
                "Lists all polls with details\n");

    LOCK(cs_main);
    std::string out1;
    UniValue res = getjsonpoll(true, true, "");
    return res;
}

UniValue listpolldetails(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "listpolldetails\n"
                "\n"
                "Lists poll details\n");

    LOCK(cs_main);
    std::string out1;
    UniValue res = getjsonpoll(true, false, "");
    return res;
}

UniValue listpollresults(const UniValue& params, bool fHelp)
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
    UniValue res(UniValue::VARR);
    bool bIncExpired = false;

    if (params.size() == 2)
        bIncExpired = params[1].get_bool();

    std::string Title1 = params[0].get_str();

    if (!PollExists(Title1))
    {
        UniValue result(UniValue::VOBJ);

        result.pushKV("Error", "Poll does not exist.  Please listpolls.");
        res.push_back(result);
    }
    else
    {
        std::string Title = params[0].get_str();
        std::string out1 = "";
        UniValue myPolls = getjsonpoll(true, bIncExpired, Title);
        res.push_back(myPolls);
    }
    return res;
}

UniValue listpolls(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
                "listpolls\n"
                "\n"
                "Lists polls\n");

    LOCK(cs_main);
    std::string out1;
    UniValue res = getjsonpoll(false, false, "");
    return res;
}

UniValue vote(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw std::runtime_error(
                "vote <title> <answer1;answer2...>\n"
                "\n"
                "<title -> Title of poll being voted on\n"
                "<answers> -> Answers chosen for specified poll separated by ;\n"
                "\n"
                "Vote on a specific poll with specified answers\n");

    UniValue res(UniValue::VOBJ);

    std::string sTitle = params[0].get_str();
    std::string sAnswer = params[1].get_str();

    std::pair<std::string,std::string> ResultString = CreateVoteContract(sTitle, sAnswer);
    res.pushKV(std::get<0>(ResultString),std::get<1>(ResultString));
    return res;
}

UniValue votedetails(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
                "votedetails <pollname>\n"
                "\n"
                "<pollname> Specified poll name\n"
                "\n"
                "Displays vote details of a specified poll\n");

    UniValue res(UniValue::VARR);
    std::string Title = params[0].get_str();

    if (!PollExists(Title))
    {
        UniValue results(UniValue::VOBJ);
        results.pushKV("Error", "Poll does not exist.  Please listpolls.");
        res.push_back(results);
    }
    else
    {
        LOCK(cs_main);
        UniValue myVotes = GetJsonVoteDetailsReport(Title);
        res.push_back(myVotes);
    }
    return res;
}



