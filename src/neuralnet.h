#pragma once

#include <string>

extern std::string qtGetNeuralHash(std::string data);
extern std::string qtGetNeuralContract(std::string data);
extern double qtExecuteGenericFunction(std::string function,std::string data);
extern void qtSyncWithDPORNodes(std::string data);


namespace NN
{
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
    bool IsEnabled();

    //!
    //! \brief Get application neural version.
    //!
    //! Fetches the application version with the neural network magic suffix
    //! (\c 1999) if the neural net is enabled.
    //!
    //! \return Current application version with proper neural suffix.
    //!
    std::string GetNeuralVersion();

    //!
    //! \brief Get current neural hash from neural net.
    //!
    //! \note This is a synchoronous operation.
    //!
    //! \return Current neural hash. This might be empty if no has has
    //! been calculated yet.
    //!
    std::string GetNeuralHash();

    //!
    //! \brief Get the most recently updated neural network contract.
    //!
    //! Synchronously queries the neural network process for the current
    //! neural contract.
    //!
    //! \return Most recent neural contract if available.
    //!
    std::string GetNeuralContract();

    //!
    //! \brief Enable/disable testnet in neural net.
    //! \param onTestnet New testnet flag.
    //!
    bool SetTestnetFlag(bool onTestnet);

    //!
    //! \brief Synchronize DPOR data.
    //!
    //! Asynchronously asks the neural net to download BOINC statistic files
    //! and calculate CPID magnitudes. If called while synchronization is
    //! already in progress this will do nothing.
    //!
    //! \param data CPID and quorum data to pass to the neural net.
    //!
    bool SynchronizeDPOR(const std::string& data);

    std::string ExecuteDotNetStringFunction(std::string function, std::string data);

    int64_t IsNeuralNet();
}
