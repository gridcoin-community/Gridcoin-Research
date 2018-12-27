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
        std::string ExplainMagnitude(const std::string& data);
        std::string ResolveDiscrepancies(const std::string& contract);
        std::string SetPrimaryCPID(const std::string& cpid);
        int64_t IsNeuralNet();
        void SetQuorumData(const std::string& data);
        void Show();

        std::unique_ptr<QAxObject> globalcom;
    };
}
