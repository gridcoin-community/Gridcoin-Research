#include "neuralnet.h"
#include "neuralnet_native.h"
#include "neuralnet_stub.h"
#include "util.h"

#include <vector>

extern bool GetBoolArg(const std::string& strArg, bool fDefault);
extern bool fExplorer;

using namespace NN;

namespace
{
    INeuralNetPtr instance;
}

INeuralNetPtr NN::CreateNeuralNet()
{
    bool fRunNN = false;

    // The purpose of the complicated defaulting below is that if not running
    // the scraper the new nn should run by default. If running the scraper,
    // then the new NN should not run unless explicitly specified to do so.

    // For example. gridcoinresearch(d) with no args will run the NN but not the scraper.
    // gridcoinresearch(d) -scraper will run the scraper but not the NN components.
    // gridcoinresearch(d) -scraper -usenewnn will run both the scraper and the NN.
    // -disablenn overrides the -usenewnn directive.

    // If -disablenn is NOT specified or set to false...
    if (!GetBoolArg("-disablenn", false))
    {
        // Then if -scraper is specified (set to true)...
        if (GetBoolArg("-scraper", false))
        {
            // Activate explorer extended features if -explorer is set
            if (GetBoolArg("-explorer", false)) fExplorer = true;

            // And -usenewnn is specified (set to true)...
            if (GetBoolArg("-usenewnn", false)) fRunNN = true;
        }
        // Else if -scraper is NOT specified (or set to true),
        // and -usenewnn is NOT specified (defaults to true here) or set to true
        else if (GetBoolArg("-usenewnn", true)) fRunNN = true;
    }

    if (fRunNN)
    {
        LogPrintf("INFO: NN::CreateNeuralNet(): Native C++ neural network is active.");
        return std::make_shared<NeuralNetNative>();
    }
    else
        // Fall back to stub implementation.
        LogPrintf("INFO: NN::CreateNeuralNet(): Native C++ neural network is inactive.");
        return std::make_shared<NeuralNetStub>();
}

void NN::SetInstance(const INeuralNetPtr &obj)
{
    instance = obj;
}

INeuralNetPtr NN::GetInstance()
{
    return instance;
}

bool NN::AddContract(
    const std::string& type,
    const std::string& key,
    const std::string& value,
    const int64_t& timestamp)
{
    if (type == "project" || type == "projectmapping") {
        GetWhitelist().Add(key, value, timestamp);
    } else {
        return false;
    }

    return true;
}

bool NN::DeleteContract(const std::string& type, const std::string& key)
{
    if (type == "project" || type == "projectmapping") {
        GetWhitelist().Delete(key);
    } else {
        return false;
    }

    return true;
}
