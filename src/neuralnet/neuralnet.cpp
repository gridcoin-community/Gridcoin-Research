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

    typedef std::vector<Factory> FactoryCollection;
    FactoryCollection& GetFactoryCollection()
    {
        static FactoryCollection factories;
        return factories;
    }
}

void NN::RegisterFactory(const Factory &factory)
{
    GetFactoryCollection().push_back(factory);
}

INeuralNetPtr NN::CreateNeuralNet()
{
    if (GetBoolArg("-usenewnn"))
        return std::make_shared<NeuralNetNative>();

    // Try to instantiate via injected factories
    for(auto factory : GetFactoryCollection())
    {
        INeuralNetPtr obj = factory();
        if(obj)
            return obj;
    }

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
