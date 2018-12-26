#pragma once

#include "neuralnet.h"

namespace NN
{
    //!
    //! \brief NeuralNet stub implementation.
    //!
    //! A neuralnet implementation which does nothing.
    //!
    class NeuralNetStub : public INeuralNet
    {
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
    };
}
