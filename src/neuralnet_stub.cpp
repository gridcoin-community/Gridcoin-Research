#include <string>

#include "neuralnet.h"
namespace NN
{
    bool IsEnabled()
    {
        return false;
    }

    std::string GetNeuralVersion()
    {
        return "0";
    }

    std::string GetNeuralHash()
    {
        return std::string();
    }

    std::string GetNeuralContract()
    {
        return std::string();
    }

    bool SetTestnetFlag(bool onTestnet)
    {
        return false;
    }

    bool SynchronizeDPOR(const std::string& data)
    {
        return false;
    }

    std::string ExecuteDotNetStringFunction(std::string function, std::string data)
    {
        return std::string();
    }

    int64_t IsNeuralNet()
    {
        return 0;
    }
}
