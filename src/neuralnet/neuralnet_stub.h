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
        std::string GetNeuralHash();
        NN::QuorumHash GetSuperblockHash();
        std::string GetNeuralContract();
        NN::Superblock GetSuperblockContract();
        std::string ExplainMagnitude(const std::string& data);
    };
}
