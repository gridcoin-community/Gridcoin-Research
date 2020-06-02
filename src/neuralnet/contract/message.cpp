#include "neuralnet/contract/message.h"
#include "neuralnet/contract/contract.h"
#include "script.h"
#include "wallet.h"

#include "coincontrol.h"

using namespace NN;

extern CWallet* pwalletMain;

namespace {
//!
//! \brief Configure coin control for a transaction to select inputs from the
//! set of UTXOs associated with the master key address and to send the change
//! back to the same address.
//!
//! \param coin_control Stores the selected input/output configuration.
//!
//! \return \c true if the wallet owns UTXOs for the master key address.
//!
bool SelectMasterInputOutput(CCoinControl& coin_control)
{
    const CTxDestination master_address = CWallet::MasterAddress().Get();

    // Send change back to the master address:
    coin_control.destChange = master_address;

    std::vector<COutput> unspent_coins;
    pwalletMain->AvailableCoins(unspent_coins, true, &coin_control, false);

    for (const auto& coin : unspent_coins) {
        CTxDestination dest;

        if (!ExtractDestination(coin.tx->vout[coin.i].scriptPubKey, dest)) {
            continue;
        }

        // Select all UTXOs for the master address as inputs to consolidate the
        // change back into one output:
        if (dest == master_address) {
            COutPoint out_point(coin.tx->GetHash(), coin.i);
            coin_control.Select(out_point);
        }
    }

    return coin_control.HasSelected();
}

//!
//! \brief Configure a contract transaction and set up inputs and outputs.
//!
//! \param wtx_new     A new transaction with a contract.
//! \param reserve_key Key reserved for any change.
//! \param burn_fee    Total burn fee required for contracts in the transaction.
//!
//! \return \c true if coin selection succeeded.
//!
bool CreateContractTx(CWalletTx& wtx_out, CReserveKey reserve_key, int64_t burn_fee)
{
    CCoinControl coin_control_out;
    int64_t applied_fee_out; // Unused
    bool admin = false;

    for (const auto& contract : wtx_out.vContracts) {
        admin |= contract.RequiresMasterKey();
    }

    // Configure inputs/outputs for the address associated with the master key.
    // Nodes validate administrative contracts by checking that the containing
    // transactions include an input signed by the master key, so select coins
    // from the master address and send any change back to it:
    //
    if (admin && !SelectMasterInputOutput(coin_control_out)) {
        return false;
    }

    // Burn the output. For reference, the old burn addresses for contracts are:
    //
    //   mainnet: S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB
    //   testnet: mk1e432zWKH1MW57ragKywuXaWAtHy1AHZ
    //
    CScript scriptPubKey;
    scriptPubKey << OP_RETURN;

    return pwalletMain->CreateTransaction(
        { std::make_pair(std::move(scriptPubKey), burn_fee) },
        wtx_out,
        reserve_key,
        applied_fee_out,
        &coin_control_out);
}

//!
//! \brief Send a transaction that contains a contract.
//!
//! \param wtx_new A new transaction with a contract.
//!
//! \return An empty string when successful or a description of the error that
//! occurred. TODO: Refactor to remove string-based signaling.
//!
std::string SendContractTx(CWalletTx& wtx_new)
{
    CReserveKey reserve_key(pwalletMain);

    if (pwalletMain->IsLocked()) {
        std::string strError = _("Error: Wallet locked, unable to create transaction.");
        LogPrintf("%s: %s", __func__, strError);
        return strError;
    }

    if (fWalletUnlockStakingOnly) {
        std::string strError = _("Error: Wallet unlocked for staking only, unable to create transaction.");
        LogPrintf("%s: %s", __func__, strError);
        return strError;
    }

    int64_t balance = pwalletMain->GetBalance();
    int64_t burn_fee = 0;

    for (const auto& contract : wtx_new.vContracts) {
        burn_fee += contract.RequiredBurnAmount();
    }

    if (balance < COIN || balance < burn_fee + nTransactionFee) {
        std::string strError = _("Balance too low to create a contract.");
        LogPrintf("%s: %s", __func__, strError);
        return strError;
    }

    if (!CreateContractTx(wtx_new, reserve_key, burn_fee)) {
        std::string strError = _("Error: Transaction creation failed.");
        LogPrintf("%s: %s", __func__, strError);
        return strError;
    }

    if (!pwalletMain->CommitTransaction(wtx_new, reserve_key)) {
        std::string strError = _(
            "Error: The transaction was rejected. This might happen if some of "
            "the coins in your wallet were already spent, such as if you used "
            "a copy of wallet.dat and coins were spent in the copy but not "
            "marked as spent here.");

        LogPrintf("%s: %s", __func__, strError);
        return strError;
    }

    for (const auto& contract : wtx_new.GetContracts()) {
        LogPrintf(
            "%s: %s %s in %s",
            __func__,
            contract.m_action.ToString(),
            contract.m_type.ToString(),
            wtx_new.GetHash().ToString());
    }

    return "";
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

std::pair<CWalletTx, std::string> NN::SendContract(Contract contract)
{
    CWalletTx wtx;

    // TODO: remove this after the v11 mandatory block. We don't need to sign
    // version 2 contracts:
    if (!IsV11Enabled(nBestHeight + 1)) {
        contract = contract.ToLegacy();

        if (contract.RequiresMessageKey() && !contract.SignWithMessageKey()) {
            return std::make_pair(
                std::move(wtx),
                "Failed to sign contract with shared message key.");
        }

        // Convert any binary contracts to the legacy string representation.
        //
        wtx.hashBoinc = contract.ToString();
    }

    contract.m_tx_timestamp = wtx.nTime;
    wtx.vContracts.emplace_back(std::move(contract));

    std::string error = SendContractTx(wtx);

    return std::make_pair(std::move(wtx), std::move(error));
}
