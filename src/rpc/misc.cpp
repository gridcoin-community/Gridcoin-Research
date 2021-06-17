// Copyright (c) 2012-2019 The Bitcoin developers
// Copyright (c) 2021 The Gridcoin developers.
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


UniValue listsettings(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size())
    {
        throw runtime_error(
                    "listsettings\n"
                    "Outputs all arguments/settings in JSON format.\n"
                    );
    }

    return gArgs.OutputArgs();
}

UniValue changesettings(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
    {
        throw runtime_error(
                    "changesettings <name=value> [name=value] ... [name=value]\n"
                    "\n"
                    "name=value: name and value pair for setting to store/change (1st mandatory, 2nd+ optional).\n"
                    "\n"
                    "Note that the settings should be done in the same format as config file entries.\n"
                    "\n"
                    "Example:"
                    "changesettings enable enablestakesplit=1 stakingefficiency=98 minstakesplitvalue=800\n"
                    );
    }

    // -------- name ------------ value - value_changed - immediate_effect
    std::map<std::string, std::tuple<std::string, bool, bool>> valid_settings;

    UniValue result(UniValue::VOBJ);
    UniValue settings_stored_with_no_state_change(UniValue::VARR);
    UniValue settings_immediate(UniValue::VARR);
    UniValue settings_applied_requiring_restart(UniValue::VARR);
    //UniValue invalid_settings_ignored(UniValue::VARR);

    // Validation
    for (unsigned int i = 0; i < params.size(); ++i)
    {
        std::string param = params[i].get_str();

        if (param.size() > 0 && param[0] == '-')
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Incorrectly formatted setting change: " + param);
        }

        std::string::size_type pos;
        std::string name;
        std::string value;

        if ((pos = param.find('=')) != std::string::npos)
        {
            name = param.substr(0, pos);
            value = param.substr(pos + 1);
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Incorrectly formatted setting change: " + param);
        }

        std::optional<unsigned int> flags = gArgs.GetArgFlags('-' + name);

        if (!flags)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid setting: " + param);
        }

        // TODO: Record explicit default state for settings.
        // This currently has a problem that I am not sure yet how to solve. Settings that are defaulted to true, unless
        // they are set to the contrary, such as -staking, will falsely indicate a change because the defaulted state is
        // not explicitly stored for comparison. After there is an explicit entry defined in the settings file, it works
        // correctly.

        // Also, the overloading of GetArg is NOT helpful here...
        std::string current_value;

        // It is either a string or a number.... One of these will succeed.
        try
        {
            current_value = gArgs.GetArg(name, "never_used_default");
        }
        catch (...)
        {
            // If it is a number convert back to a string.
            current_value = ToString(gArgs.GetArg(name, 1));
        }

        bool value_changed = (current_value != value);
        bool immediate_effect = *flags & ArgsManager::IMMEDIATE_EFFECT;

        auto insert_pair = valid_settings.insert(std::make_pair(
                                                     name, std::make_tuple(value, value_changed, immediate_effect)));

        if (!insert_pair.second)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "changesettings does not support more than one instance of the same "
                                                      "setting: " + param);
        }
    }

    // Now that validation is done do the update work.
    bool restart_required = false;

    for (const auto& setting : valid_settings)
    {
        const std::string& name = setting.first;
        const std::string& value = std::get<0>(setting.second);
        const bool& value_changed = std::get<1>(setting.second);
        const bool& immediate_effect = std::get<2>(setting.second);

        std::string param = name + "=" + value;

        // Regardless, store in r-w settings file.
        if (!updateRwSetting(name, value))
        {
            throw JSONRPCError(RPC_MISC_ERROR, "Error storing setting in read-write settings file.");
        }

        if (value_changed)
        {
            gArgs.ForceSetArg(name, value);

            if (immediate_effect)
            {
                settings_immediate.push_back(param);
            }
            else
            {
                settings_applied_requiring_restart.push_back(param);

                // Record if restart required.
                restart_required |= !immediate_effect;
            }
        }
        else
        {
            settings_stored_with_no_state_change.push_back(param);
        }
    }

    result.pushKV("settings_change_requires_restart", restart_required);
    result.pushKV("settings_stored_with_no_state_change", settings_stored_with_no_state_change);
    result.pushKV("settings_changed_taking_immediate_effect", settings_immediate);
    result.pushKV("settings_changed_requiring_restart", settings_applied_requiring_restart);

    return result;
}
