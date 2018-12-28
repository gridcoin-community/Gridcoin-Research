#pragma once

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
        //! \brief Get application neural version.
        //!
        //! Fetches the application version with the neural network magic suffix
        //! (\c 1999) if the neural net is enabled.
        //!
        //! \return Current application version with proper neural suffix.
        //!
        virtual std::string GetNeuralVersion() = 0;

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
        //! \brief Get the most recently updated neural network contract.
        //!
        //! Synchronously queries the neural network process for the current
        //! neural contract.
        //!
        //! \return Most recent neural contract if available.
        //!
        virtual std::string GetNeuralContract() = 0;

        //!
        //! \brief Synchronize DPOR data.
        //!
        //! Asynchronously asks the neural net to download BOINC statistic files
        //! and calculate CPID magnitudes. If called while synchronization is
        //! already in progress this will do nothing.
        //!
        //! \param data CPID and quorum data to pass to the neural net.
        //!
        virtual bool SynchronizeDPOR(const std::string& data) = 0;

        virtual std::string ExplainMagnitude(const std::string& cpid) = 0;

        virtual int64_t IsNeuralNet() = 0;
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
    //! \brief Get globl neuralnet instance.
    //! \return Current global neuralnet instance.
    //!
    INeuralNetPtr GetInstance();
}
