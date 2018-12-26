#pragma once

#include "neuralnet/neuralnet.h"

#include <qglobal.h>
#include <memory>

QT_BEGIN_NAMESPACE
class QAxObject;
QT_END_NAMESPACE

namespace NN
{
    //!
    //! \brief Win32 neuralnet implementation.
    //!
    //! A neuralnet implementation bridge which calls the original .NET
    //! implementation for data scraping and processing.
    //!
    class NeuralNetWin32 : public INeuralNet
    {
    public:
        // Constructor
        NeuralNetWin32();

    private:
        // Documentation in interface.
        bool IsEnabled();
        std::string GetNeuralVersion();
        std::string GetNeuralHash();
        std::string GetNeuralContract();
        bool SynchronizeDPOR(const std::string& data);
        std::string ExecuteDotNetStringFunction(std::string function, std::string data);
        int64_t IsNeuralNet();
        void Show();

        std::unique_ptr<QAxObject> globalcom;
    };
}
