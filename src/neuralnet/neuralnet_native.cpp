#include "neuralnet_native.h"
#include "version.h"
#include "util.h"

#include <string>

extern std::string GetArgument(const std::string& arg, const std::string& defaultvalue);
extern bool ScraperSynchronizeDPOR();
extern std::string ExplainMagnitude(std::string sCPID);

using namespace NN;

extern Superblock ScraperGetSuperblockContract(bool bStoreConvergedStats = false, bool bContractDirectFromStatsUpdate = false);

bool NeuralNetNative::IsEnabled()
{
    return GetArgument("disableneuralnetwork", "false") == "false";
}

std::string NeuralNetNative::GetNeuralHash()
{
    return GetSuperblockHash().ToString();
}

QuorumHash NeuralNetNative::GetSuperblockHash()
{
    return GetSuperblockContract().GetHash();
}

std::string NeuralNetNative::GetNeuralContract()
{
    return GetSuperblockContract().PackLegacy();
}

Superblock NeuralNetNative::GetSuperblockContract()
{
    return ScraperGetSuperblockContract(false, false);
}

// Note that the data argument is still used here for compatibility, but I don't think it will
// actually be used in the scraper. We will see.
bool NeuralNetNative::SynchronizeDPOR(const BeaconConsensus& beacons)
{
    return ScraperSynchronizeDPOR();
}

std::string NeuralNetNative::ExplainMagnitude(const std::string& cpid)
{
    return ::ExplainMagnitude(cpid);
}
