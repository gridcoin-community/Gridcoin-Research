#include "neuralnet_stub.h"

#include <string>

using namespace NN;

bool NeuralNetStub::IsEnabled()
{
    return false;
}

std::string NeuralNetStub::GetNeuralVersion()
{
    return "0";
}

std::string NeuralNetStub::GetNeuralHash()
{
    return std::string();
}

NN::QuorumHash NeuralNetStub::GetSuperblockHash()
{
    NN::QuorumHash EmptyQuorumHash;

    //Initialize a purposefully empty (invalid) hash.
    return EmptyQuorumHash;
}

std::string NeuralNetStub::GetNeuralContract()
{
    return std::string();
}

NN::Superblock NeuralNetStub::GetSuperblockContract()
{
    NN::Superblock EmptySuperblock;

    return EmptySuperblock;
}


bool NeuralNetStub::SynchronizeDPOR(const BeaconConsensus& beacons)
{
    return false;
}

std::string NeuralNetStub::ExplainMagnitude(const std::string& data)
{
    return std::string();
}

int64_t NeuralNetStub::IsNeuralNet()
{
    return 0;
}
