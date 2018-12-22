#include "neuralnet_native.h"

#include <string>

using namespace NN;

bool NeuralNetNative::IsEnabled()
{
    return false;
}

std::string NeuralNetNative::GetNeuralVersion()
{
    return "0";
}

std::string NeuralNetNative::GetNeuralHash()
{
    return std::string();
}

std::string NeuralNetNative::GetNeuralContract()
{
    return std::string();
}

bool NeuralNetNative::SynchronizeDPOR(const std::string& data)
{
    return false;
}

std::string NeuralNetNative::ExecuteDotNetStringFunction(std::string function, std::string data)
{
    return std::string();
}

int64_t NeuralNetNative::IsNeuralNet()
{
    return 0;
}
