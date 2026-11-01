// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_RWSETTINGS_H
#define GRIDCOIN_UTIL_RWSETTINGS_H

#include <univalue.h>

#include <string>
#include <vector>

//! One key of updateRwSettingsAndForcedArgs(): its read-write value and what
//! happens to its forced (running) value, which outranks every other source.
struct RwSettingUpdate {
    enum class Forced {
        KEEP,  //!< Leave the forced value as it is.
        SET,   //!< Force forced_value. A null forced_value drops it, as CLEAR does.
        CLEAR, //!< Drop the forced value, so the key reads through the command
               //!< line, the read-write settings, the config file and its default.
    };
    std::string name;          //!< The setting name without the leading dash.
    UniValue value;            //!< The read-write value; null erases the key.
    Forced forced{Forced::KEEP};
    UniValue forced_value;     //!< The value SET forces.
};

//! The same contract as updateRwSettings() in util/system.h, and each key's
//! forced value is changed as its update says, in the same step: one hold of the
//! settings lock covers the read-write values, the forced values and the
//! settings file write, so no other writer can act between them. On failure or
//! a throw every key's read-write and forced value is restored.
//! RwSettingsUpdated is emitted once, after the outcome and outside the lock, so
//! a listener reads the final state of both.
bool updateRwSettingsAndForcedArgs(const std::vector<RwSettingUpdate>& updates);

#endif // GRIDCOIN_UTIL_RWSETTINGS_H
