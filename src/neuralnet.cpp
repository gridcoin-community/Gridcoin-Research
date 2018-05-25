#include "neuralnet.h"
#include "util.h"
#include "version.h"
#include "sync.h"

#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>

#include <functional>
#include <future>
#include <fstream>
#include <map>
#include <array>
#include <cstdio>
#include <string>

// Old VB based NeuralNet.
extern std::string qtGetNeuralHash(std::string data);
extern std::string qtGetNeuralContract(std::string data);
extern double qtExecuteGenericFunction(std::string function,std::string data);
extern std::string qtExecuteDotNetStringFunction(std::string function,std::string data);
extern void qtSyncWithDPORNodes(std::string data);
int64_t IsNeural();

// While transitioning to dotnet the NeuralNet implementation has been split
// into 3 implementations; Win32 with Qt, Win32 without Qt and the rest.
// After the transition both Win32 implementations can be removed.
namespace NN
{
    // Win32 with Qt enabled.
    bool IsEnabled()
    {
        return GetArgument("disableneuralnetwork", "false") == "false";
    }

    std::string GetNeuralVersion()
    {
        int neural_id = static_cast<int>(IsNeural());
        return std::to_string(CLIENT_VERSION_MINOR) + "." + std::to_string(neural_id);
    }

    std::string GetNeuralHash()
    {
        return qtGetNeuralHash("");
    }

    std::string GetNeuralContract()
    {
        return qtGetNeuralContract("");
    }

    bool SetTestnetFlag(bool onTestnet)
    {
        return qtExecuteGenericFunction("SetTestNetFlag", onTestnet ? "TESTNET" : "MAINNET") != 0;
    }

    bool SynchronizeDPOR(const std::string& data)
    {
        qtSyncWithDPORNodes(data);
        return true;
    }

    std::string ExecuteDotNetStringFunction(std::string function, std::string data)
    {
        return qtExecuteDotNetStringFunction(function, data);
    }

    int64_t IsNeuralNet()
    {
       return IsNeural();
    }
}
