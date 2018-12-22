#pragma once

#include "neuralnet.h"

namespace NN
{
    //!
    //! \brief NeuralNet native implementation.
    //!
    //! The new native C++ neuralnet implementation that uses the new integrated scraper.
    //!
    class NeuralNetNative : public INeuralNet
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
    };
}
