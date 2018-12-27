#include "neuralnet_native.h"
#include "version.h"
#include "util.h"

#include <string>

extern std::string GetArgument(const std::string& arg, const std::string& defaultvalue);
extern std::string ScraperGetNeuralContract(bool bStoreConvergedStats = false, bool bContractDirectFromStatsUpdate = false);
extern std::string ScraperGetNeuralHash();
extern bool ScraperSynchronizeDPOR();
extern std::string ExplainMagnitude(std::string sCPID);

using namespace NN;

bool NeuralNetNative::IsEnabled()
{
    return GetArgument("disableneuralnetwork", "false") == "false";
}

// This is for compatibility
std::string NeuralNetNative::GetNeuralVersion()
{
    int64_t neural_id = IsNeuralNet();
    return std::to_string(CLIENT_VERSION_MINOR) + "." + std::to_string(neural_id);
}

std::string NeuralNetNative::GetNeuralHash()
{
    return ScraperGetNeuralHash();
}

std::string NeuralNetNative::GetNeuralContract()
{
    return ScraperGetNeuralContract(false, false);
}

// Note that the data argument is still used here for compatibility, but I don't think it will
// actually be used in the scraper. We will see.
bool NeuralNetNative::SynchronizeDPOR(const std::string& data)
{
    return ScraperSynchronizeDPOR();
}

std::string NeuralNetNative::ExplainMagnitude(const std::string& cpid)
{
    return ::ExplainMagnitude(cpid);
}

std::string NeuralNetNative::ResolveDiscrepancies(const std::string &contract)
{
    // Preserved for backward compatibility for now.
    return std::string("SUCCESS");
}

std::string NeuralNetNative::SetPrimaryCPID(const std::string &cpid)
{
    return std::string();
}

int64_t NeuralNetNative::IsNeuralNet()
{
    // This is the NN version number. TODO: Consider different number for new NN?
    int64_t nNeuralNetVersion = 1999;
    return nNeuralNetVersion;
}

void NeuralNetNative::SetQuorumData(const std::string& data)
{}

void NeuralNetNative::Show()
{}
