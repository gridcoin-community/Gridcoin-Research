#pragma once

#include "neuralnet/contract/contract.h"

namespace NN
{
//!
//! \brief Stores and manages researcher beacons.
//!
class BeaconRegistry : public IContractHandler
{
public:
    //!
    //! \brief Register a beacon from contract data.
    //!
    //! \param contract Contains information about the beacon to add.
    //!
    void Add(const Contract& contract) override;

    //!
    //! \brief Deregister the beacon specified by contract data.
    //!
    //! \param contract Contains information about the beacon to remove.
    //!
    void Delete(const Contract& contract) override;

    //!
    //! \brief Reverse a beacon registration or deregistration.
    //!
    //! \param contract Contains the action and CPID of the beacon entry to
    //! reverse.
    //!
    void Revert(const Contract& contract) override;
};

//!
//! \brief Get the global beacon registry.
//!
//! \return Current global beacon manager instance.
//!
BeaconRegistry& GetBeaconRegistry();
}
