#pragma once

#include "project.h"
#include "superblock.h"

#include <string>
#include <memory>
#include <functional>

namespace NN
{
    //!
    //! \brief NeuralNet interface.
    //!
    struct INeuralNet
    {
        //!
        //! \brief Destructor.
        //!
        virtual ~INeuralNet() {}

        //!
        //! \brief Check is current system supports neural net operations.
        //!
        //! Validates the currently running system to see if neural network is
        //! supported by thte system and not disabled by the user.
        //!
        //! \note Calling further functions on a disabled neural network is
        //! undefined behavior.
        //!
        //! \return \c true if neural network is enabled, \c false otherwise.
        //!
        virtual bool IsEnabled() = 0;

        //!
        //! \brief Get current neural hash from neural net.
        //!
        //! \note This is a synchoronous operation.
        //!
        //! \return Current neural hash. This might be empty if no has has
        //! been calculated yet.
        //!
        virtual std::string GetNeuralHash() = 0;

        //!
        //! \brief Get current superblock hash from scraper.
        //!
        //! \note This is a synchoronous operation.
        //!
        //! \return Current superblock hash. This might be empty if it has
        //! not been calculated yet.
        //!
        virtual NN::QuorumHash GetSuperblockHash() = 0;

        //!
        //! \brief Get the most recently updated neural network contract.
        //!
        //! Synchronously queries the neural network process for the current
        //! neural contract.
        //!
        //! \return Most recent neural contract if available.
        //!
        virtual std::string GetNeuralContract() = 0;

        //!
        //! \brief Get the most recently updated superblock contract.
        //!
        //! Synchronously queries the scraper for the current
        //! superblock contract.
        //!
        //! \return Most recent superblock contract if available.
        //!
        virtual NN::Superblock GetSuperblockContract() = 0;

        virtual std::string ExplainMagnitude(const std::string& cpid) = 0;
    };

    //!
    //! \brief INeuralNet smart pointer.
    //!
    typedef std::shared_ptr<INeuralNet> INeuralNetPtr;

    //!
    //! \brief Neuralnet factory.
    //!
    //! Evaluates host platform and configuration flags to instantiate an
    //! appropriate neuralnet object.
    //!
    //! This creates an object using the following criterias and order:
    //!  1) New neuralnet if enabled
    //!  2) Old neuralnet if factory has been registered, see RegisterFactory().
    //!  3) A neural net stub.
    //!
    //! \return A new INeuralNet instance.
    //!
    INeuralNetPtr CreateNeuralNet();

    //!
    //! \brief Set global neuralnet object.
    //!
    //! Sets the global object used for neuralnet access. This should be called
    //! during application initialization:
    //!
    //! \code
    //! NN::SetInstance(NN::CreateNeuralNet());
    //! \endcode
    //!
    //! It can also be used to inject mocks in unit tests.
    //!
    //! \param obj New global neuralnet instance.
    //!
    void SetInstance(const INeuralNetPtr& obj);

    //!
    //! \brief Get global neuralnet instance.
    //! \return Current global neuralnet instance.
    //!
    INeuralNetPtr GetInstance();

    //!
    //! \brief Attempt to process an \c A (addition) contract action with a
    //! neural network component.
    //!
    //! \param type      Contract type. Determines how to handle the message.
    //! \param key       The key as stored in the contract message.
    //! \param value     The value as stored in the contract message.
    //! \param timestamp The contract message timestamp.
    //!
    //! \return \c true if the message contains a contract type handled by the
    //! NN. If \c false, the calling code must process the message elsewhere.
    //!
    bool AddContract(
        const std::string& type,
        const std::string& key,
        const std::string& value,
        const int64_t& timestamp);

    //!
    //! \brief Attempt to process a \c D (deletion) contract action with a
    //! neural network component.
    //!
    //! \param type      Contract type. Determines how to handle the message.
    //! \param key       The key as stored in the contract message.
    //!
    //! \return \c true if the message contains a contract type handled by the
    //! NN. If \c false, the calling code must process the message elsewhere.
    //!
    bool DeleteContract(const std::string& type, const std::string& key);
}
