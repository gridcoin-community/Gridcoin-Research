// Copyright (c) 2012-2019 The Bitcoin developers
// Copyright (c) 2021 The Gridcoin developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "init.h"
#include "protocol.h"
#include <rpc/util.h>
#include <util.h>

#include <univalue.h>
#include <stdexcept>

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

static const RPCHelpMan logging_help{
    "logging",
    "Gets and sets the logging configuration.\n"
    "\n"
    "When called without an argument, returns the list of categories with status that are currently being\n"
    "debug logged or not.\n"
    "When called with arguments, adds or removes categories from debug logging and returns the lists above.\n"
    "The arguments are evaluated in order \"include\", \"exclude\".\n"
    "If an item is both included and excluded, it will thus end up being excluded.\n"
    "\n"
    "The valid logging categories are the ones reported when this command is called without arguments.\n"
    "In addition, the following are available as category names with special meanings:\n"
    "  all  or 1: represent all logging categories.\n"
    "  none or 0: even if other logging categories are specified, ignore all of them.\n"
    "\n"
    "Note that unlike Bitcoin, we don't yet process JSON arrays correctly as arguments for the command line,\n"
    "so, for the rpc cli, it is limited to one enable and/or one disable category. Using CURL works with the\n"
    "full arrays.",
    {
        {"include", RPCArg::Type::ARR, RPCArg::Optional::OMITTED,
            "JSON array of categories to enable (or a single category string).",
            {
                {"category", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "A logging category name."},
            }},
        {"exclude", RPCArg::Type::ARR, RPCArg::Optional::OMITTED,
            "JSON array of categories to disable (or a single category string).",
            {
                {"category", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "A logging category name."},
            }},
    },
    RPCResult{RPCResult::Type::OBJ_DYN, "", "Mapping of category name to active state",
        {
            {RPCResult::Type::BOOL, "category", "Whether the named category is currently logged"},
        }},
    RPCExamples{
        HelpExampleCli("logging", "all net") +
        HelpExampleCli("logging", "\"\" all") +
        HelpExampleRpc("logging", "[\"all\"], [\"net\"]")},
};
const RPCHelpMan& logging_helpman() { return logging_help; }

UniValue logging(const UniValue& params)
{
    if (params.size() >= 1) EnableOrDisableLogCategories(params[0], true);

    if (params.size() == 2) EnableOrDisableLogCategories(params[1], false);

    UniValue result(UniValue::VOBJ);
    std::vector<CLogCategoryActive> vLogCatActive = ListActiveLogCategories();
    for (const auto& logCatActive : vLogCatActive) {
        result.pushKV(logCatActive.category, logCatActive.active);
    }

    return result;
}


static const RPCHelpMan listsettings_help{
    "listsettings",
    "Outputs all arguments/settings in JSON format.",
    {},
    RPCResult{RPCResult::Type::OBJ_DYN, "", "Mapping of setting name to its current value",
        {
            {RPCResult::Type::ELISION, "", "Current value (string, numeric, boolean, or array)"},
        }},
    RPCExamples{
        HelpExampleCli("listsettings", "") +
        HelpExampleRpc("listsettings", "")},
};
const RPCHelpMan& listsettings_helpman() { return listsettings_help; }

UniValue listsettings(const UniValue& params)
{
    return gArgs.OutputArgs();
}

// Variadic: declared signature is a single setting=value pair but
// callers may pass additional name=value pairs as trailing positionals.
// The body iterates over all params; MarkVariadic() opts out of the
// dispatcher's IsValidNumArgs pre-check.
static const RPCHelpMan changesettings_help = RPCHelpMan{
    "changesettings",
    "Store or change one or more configuration settings.\n"
    "\n"
    "Settings must be passed in the same format as config file entries (name=value).\n"
    "Additional name=value pairs may be supplied as further positional arguments.",
    {
        {"setting", RPCArg::Type::STR, RPCArg::Optional::NO,
            "Setting to store/change in the form name=value. Pass additional positional arguments "
            "to change more than one setting in a single call."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::BOOL, "settings_change_requires_restart",
                "True if at least one of the changed settings does not take effect until restart."},
            {RPCResult::Type::ARR, "settings_stored_with_no_state_change", "",
                {
                    {RPCResult::Type::STR, "setting", "Stored setting whose value did not change."},
                }},
            {RPCResult::Type::ARR, "settings_changed_taking_immediate_effect", "",
                {
                    {RPCResult::Type::STR, "setting", "Setting whose change is now in effect."},
                }},
            {RPCResult::Type::ARR, "settings_changed_requiring_restart", "",
                {
                    {RPCResult::Type::STR, "setting", "Setting whose change requires a restart."},
                }},
        }},
    RPCExamples{
        HelpExampleCli("changesettings", "enablestakesplit=1 stakingefficiency=98 minstakesplitvalue=800") +
        HelpExampleRpc("changesettings", "\"enablestakesplit=1\"")},
}.MarkVariadic();
const RPCHelpMan& changesettings_helpman() { return changesettings_help; }

UniValue changesettings(const UniValue& params)
{
    // Variadic positional args: at least one setting required, no upper bound. RPCHelpMan does
    // not model unbounded variadic, so retain a body-level lower-bound check after the dispatcher
    // has handled the help-rendering and (best-effort) arity-upper-bound paths.
    if (params.size() < 1)
        throw runtime_error(changesettings_helpman().ToString());

    // Parse the "name=value" positionals into pairs. The shared ChangeSettings()
    // core (src/init.cpp) does validation, persistence to gridcoinsettings.json,
    // the ForceSetArg, and the immediate side-effect application -- the same code
    // path interfaces::Node::changeSettings() uses for the GUI, so the two never
    // diverge.
    std::vector<std::pair<std::string, std::string>> settings;
    settings.reserve(params.size());

    for (unsigned int i = 0; i < params.size(); ++i)
    {
        const std::string param = params[i].get_str();
        const std::string::size_type pos = param.find('=');

        if (param.empty() || param[0] == '-' || pos == std::string::npos)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Incorrectly formatted setting change: " + param);
        }

        settings.emplace_back(param.substr(0, pos), param.substr(pos + 1));
    }

    bool restart_required = false;
    std::vector<std::string> no_change, immediate, requiring_restart;
    std::string error;

    if (!ChangeSettings(settings, restart_required, no_change, immediate, requiring_restart, error))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, error);
    }

    UniValue settings_stored_with_no_state_change(UniValue::VARR);
    for (const auto& s : no_change) settings_stored_with_no_state_change.push_back(s);
    UniValue settings_immediate(UniValue::VARR);
    for (const auto& s : immediate) settings_immediate.push_back(s);
    UniValue settings_applied_requiring_restart(UniValue::VARR);
    for (const auto& s : requiring_restart) settings_applied_requiring_restart.push_back(s);

    UniValue result(UniValue::VOBJ);
    result.pushKV("settings_change_requires_restart", restart_required);
    result.pushKV("settings_stored_with_no_state_change", settings_stored_with_no_state_change);
    result.pushKV("settings_changed_taking_immediate_effect", settings_immediate);
    result.pushKV("settings_changed_requiring_restart", settings_applied_requiring_restart);

    return result;
}
