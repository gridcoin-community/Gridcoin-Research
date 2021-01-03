// Copyright (c) 2012-2019 The Bitcoin developers
// Copyright (c) 2020 The Gridcoin developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protocol.h"
#include "util.h"

#include <univalue.h>

using namespace std;

static void EnableOrDisableLogCategories(UniValue cats, bool enable) {

    std::vector<std::string> vcats;

    // The below is to allow non-array parameters. The current front-end for Gridcoin does not
    // understand JSON parameters. This will handle that when it does, and deal with single
    // parameters too.
    if (cats.isArray())
    {
        for (unsigned int i = 0; i < cats.size(); ++i)
        {
            vcats.push_back(cats[i].get_str());
        }
    }
    else
    {
        vcats.push_back(cats.get_str());
    }

    for (const auto& cat : vcats)
    {
        bool success;
        if (enable) {
            LogPrintf("INFO: EnableOrDisableLogCategories: enabling category %s", cat);
            success = LogInstance().EnableCategory(cat);
        } else {
            LogPrintf("INFO: EnableOrDisableLogCategories: disabling category %s", cat);
            success = LogInstance().DisableCategory(cat);
        }

        if (!success) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "unknown logging category " + cat);
        }
    }
}

UniValue logging(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
    {
        throw runtime_error(
           "logging [json array category adds] [json array category removes]\n"
            "Gets and sets the logging configuration.\n"
            "When called without an argument, returns the list of categories with status that are currently being debug logged or not.\n"
            "When called with arguments, adds or removes categories from debug logging and return the lists above.\n"
            "The arguments are evaluated in order \"include\", \"exclude\".\n"
            "If an item is both included and excluded, it will thus end up being excluded.\n"
            "The valid logging categories are: " + ListLogCategories() + ".\n"
            "In addition, the following are available as category names with special meanings:\n"
            "all or 1: represent all logging categories.\n"
            "none or 0: even if other logging categories are specified, ignore all of them.\n\n"
            "Examples:\n"
            "logging all net: enables all and disables net.\n"
            "logging \"\" all: disables all.\n\n"
            "Note that unlike Bitcoin, we don't process JSON arrays correctly as arguments yet for the command line,\n"
            "so, for the rpc cli, it is limited to one enable and/or one disable category. Using CURL works with the full arrays.\n"
            );
    }

    if (params.size() >= 1) EnableOrDisableLogCategories(params[0], true);

    if (params.size() == 2) EnableOrDisableLogCategories(params[1], false);

    UniValue result(UniValue::VOBJ);
    std::vector<CLogCategoryActive> vLogCatActive = ListActiveLogCategories();
    for (const auto& logCatActive : vLogCatActive) {
        result.pushKV(logCatActive.category, logCatActive.active);
    }

    return result;
}

