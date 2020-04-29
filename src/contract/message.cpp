#include "base58.h"
#include "message.h"
#include "neuralnet/contract.h"
#include "rpcserver.h"
#include "wallet.h"

extern CWallet* pwalletMain;

std::pair<CWalletTx, std::string> SendContract(NN::Contract contract, int64_t min_balance, double fee)
{
    CWalletTx wtx;

    wtx.vContracts.push_back(contract);

    std::string error = pwalletMain->SendMoneyToDestinationWithMinimumBalance(
        CBitcoinAddress(NN::Contract::BurnAddress()).Get(),
        AmountFromValue(fee),
        AmountFromValue(min_balance),
        wtx);

    return std::make_pair(wtx, error);
}

std::string SendPublicContract(NN::Contract contract)
{
    if (!contract.SignWithMessageKey()) {
        return "Failed to sign contract with shared message key.";
    }

    std::pair<CWalletTx, std::string> result = SendContract(contract, 0.00001, 1);

    return std::get<1>(result);
}
