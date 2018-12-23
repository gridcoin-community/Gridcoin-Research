#include "neuralnet_native.h"

#include <string>

extern std::string GetArgument(const std::string& arg, const std::string& defaultvalue);
extern std::string ScraperGetNeuralContract(bool bStoreConvergedStats);
extern std::string ScraperGetNeuralHash();
extern bool ScraperSynchronizeDPOR();

using namespace NN;

bool NeuralNetNative::IsEnabled()
{
    return GetArgument("disableneuralnetwork", "false") == "false";
}

std::string NeuralNetNative::GetNeuralVersion()
{
    // This is the NN magic version number. TODO: Consider different number for new NN?
    return std::to_string(1999);
}

std::string NeuralNetNative::GetNeuralHash()
{
    return ScraperGetNeuralHash();
}

std::string NeuralNetNative::GetNeuralContract()
{
    return ScraperGetNeuralContract(false);
}

// Note that the data argument is still used here for compatibility, but I don't think it will
// actually be used in the scraper. We will see.
bool NeuralNetNative::SynchronizeDPOR(const std::string& data)
{
    return ScraperSynchronizeDPOR();
}

std::string NeuralNetNative::ExecuteDotNetStringFunction(std::string function, std::string data)
{
    return std::string();
}

int64_t NeuralNetNative::IsNeuralNet()
{
    return 0;
}
