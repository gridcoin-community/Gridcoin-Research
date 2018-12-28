#include "neuralnet.h"
#include "neuralnet_native.h"
#include "neuralnet_stub.h"
#include "util.h"

#include <vector>

extern bool GetBoolArg(const std::string& strArg, bool fDefault);

using namespace NN;

namespace
{
    INeuralNetPtr instance;
}

INeuralNetPtr NN::CreateNeuralNet()
{
    if (GetBoolArg("-usenewnn"))
        return std::make_shared<NeuralNetNative>();

    // Fall back to stub implementation.
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
