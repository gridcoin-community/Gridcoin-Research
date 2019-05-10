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
        bool SynchronizeDPOR(const BeaconConsensus& beacons);
        std::string ExplainMagnitude(const std::string& cpid);
        int64_t IsNeuralNet();
    };
}
