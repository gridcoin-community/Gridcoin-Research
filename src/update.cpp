#include "update.h"
#include "univalue.h"
#include "version.h"
#include "ui_interface.h"
#include "util.h"

#include <string>
#include <vector>

extern void IsUpdateAvailable();

bool update::VersionCheck()
{
    if (fTestNet)
    {
        LogPrintf("Update Check: Skipped, client running testnet");

        return true;
    }

    // Init SSL
    if (!sslinit())
    {
        LogPrintf("Update Check: Failed to initilize SSL");

        return false;
    }

    // IP look up for api.github.com
    if (!addresslookup())
    {
        LogPrintf("Update Check: Failed to look up host");

        return false;
    }

    // Init Socket
    if (!socketinit())
    {
        LogPrintf("Update Check: Failed to initilize Socket");

        return false;
    }

    // Connect Socket
    if (!socketconnect())
    {
        LogPrintf("Update Check: Failed to connect SSL socket (%d)", statuscode);

        return false;
    }

    // Send Request
    if (!requestdata())
    {
        LogPrintf("Update Check: Failed to request data (%d)", statuscode);

        return false;
    }

    // Receive Reply
    if (!recvreply())
    {
        LogPrintf("Update Check: Failed to receive reply (%d)", statuscode);

        return false;
    }

    // Strip reply
    stripreply();

    clearstatus();

    UniValue json (UniValue::VOBJ);

    // Read into UniValue
    if (!json.read(strippedreply))
    {
        LogPrintf("Update Check: Failed to parse reply into json");

        return false;
    }

    if (!json.isObject())
    {
        LogPrintf("Update Check: It appears the reply is not a json object");

        return false;
    }

    // We have json and a valid object
    // tag_name has version
    // body has change log

    std::string ghversion = find_value(json, "tag_name").get_str();
    std::string ghbody = find_value(json, "body").get_str();

    std::vector<std::string> githubversion;
    std::vector<int> localversion;

    ParseString(ghversion, '.', githubversion);

    localversion.push_back(CLIENT_VERSION_MAJOR);
    localversion.push_back(CLIENT_VERSION_MINOR);
    localversion.push_back(CLIENT_VERSION_REVISION);

    if (githubversion.size() != 4)
    {
        LogPrintf("Update Check: Got malformed version (%s)", ghversion);

        return false;
    }

    bool newupdate = false;

    try
    {
        // Left to right version numbers.
        // 3 numbers to check for production.
        for (unsigned int x = 0; x < 3; x++)
        {
            if (newupdate)
                break;

            if (std::stoi(githubversion[x]) > localversion[x])
                newupdate = true;
        }
    }

    catch (std::exception& ex)
    {
        LogPrintf("Update Check: Exception occured checking client version against github version (%s)", ToString(ex.what()));

        return false;
    }

    if (newupdate)
    {
        std::string updatemessage = "Local Version: ";

        updatemessage.append(ToString(CLIENT_VERSION_MAJOR));
        updatemessage.append(".");
        updatemessage.append(ToString(CLIENT_VERSION_MINOR));
        updatemessage.append(".");
        updatemessage.append(ToString(CLIENT_VERSION_REVISION));
        updatemessage.append(".0\r\n");
        updatemessage.append("Github Version: ");
        updatemessage.append(ghversion);
        updatemessage.append("\r\n\r\n");
        updatemessage.append(ghbody);

        uiInterface.UpdateMessageBox(updatemessage);
    }

    else
        LogPrintf("Update Check: Client Up To Date");

    return true;
}

void IsUpdateAvailable()
{
    update check;

    if (!check.VersionCheck())
        LogPrintf("Update Check: Failed to check for an update");
}
