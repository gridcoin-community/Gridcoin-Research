#pragma once

#include "neuralnet.h"

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
