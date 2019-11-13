#include "neuralnet_native.h"
#include "version.h"
#include "util.h"

#include <string>

extern std::string GetArgument(const std::string& arg, const std::string& defaultvalue);
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

std::string NeuralNetNative::ExplainMagnitude(const std::string& cpid)
{
    return ::ExplainMagnitude(cpid);
}
