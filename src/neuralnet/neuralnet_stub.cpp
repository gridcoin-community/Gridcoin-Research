#include "neuralnet_stub.h"

#include <string>

using namespace NN;

bool NeuralNetStub::IsEnabled()
{
    return false;
}

std::string NeuralNetStub::GetNeuralVersion()
{
    return "0";
}

std::string NeuralNetStub::GetNeuralHash()
{
    return std::string();
}

std::string NeuralNetStub::GetNeuralContract()
{
    return std::string();
}

bool NeuralNetStub::SynchronizeDPOR(const std::string& data)
{
    return false;
}

std::string NeuralNetStub::ExecuteDotNetStringFunction(std::string function, std::string data)
{
    return std::string();
}

int64_t NeuralNetStub::IsNeuralNet()
{
    return 0;
}
