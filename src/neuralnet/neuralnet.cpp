#include "neuralnet.h"
#include "neuralnet_native.h"
#include "neuralnet_stub.h"
#include "util.h"



#if defined(QT_GUI) && defined(WIN32)
#include "neuralnet_win.h"
#endif

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

#if defined(QT_GUI) && defined(WIN32)
    return std::make_shared<NeuralNetWin32>();
#else
    return std::make_shared<NeuralNetStub>();
#endif
}

void NN::SetInstance(const INeuralNetPtr &obj)
{
    instance = obj;
}

INeuralNetPtr NN::GetInstance()
{
    return instance;
}
