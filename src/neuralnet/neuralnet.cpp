#include "neuralnet.h"
#include "neuralnet_stub.h"

#if defined(QT_GUI) && defined(WIN32)
#include "neuralnet_win.h"
#endif

using namespace NN;

namespace
{
    INeuralNetPtr instance;
}

INeuralNetPtr NN::CreateNeuralNet()
{
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
