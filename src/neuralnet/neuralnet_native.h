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
        std::string ExplainMagnitude(const std::string& cpid);
        std::string ResolveDiscrepancies(const std::string &contract);
        std::string SetPrimaryCPID(const std::string &cpid);
        int64_t IsNeuralNet();
        void SetQuorumData(const std::string& data);
        void Show();
    };
}
