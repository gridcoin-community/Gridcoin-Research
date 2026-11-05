// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_PASSPHRASE_PARAMS_H
#define BITCOIN_RPC_PASSPHRASE_PARAMS_H

#include <univalue.h>

#include <string>
#include <vector>

//! Indexes of the parameters that are wallet passphrases: walletpassphrase {0},
//! walletpassphrasechange {0, 1}, encryptwallet {0}, setautounlock {0}, clearautounlock {0};
//! nullptr for every other method.
const std::vector<int>* PassphraseParamIndexes(const std::string& method);

//! memory_cleanse the string bytes of params[i] for each passphrase index i (in place; size kept).
//! A value that is not a string, or an index past the end, is left alone.
void CleansePassphraseParams(const std::string& method, const UniValue& params);

//! RAII: CleansePassphraseParams(method, params) on destruction. The params must outlive it.
class PassphraseParamsCleanser
{
public:
    PassphraseParamsCleanser(std::string method, const UniValue& params);
    ~PassphraseParamsCleanser();

    PassphraseParamsCleanser(const PassphraseParamsCleanser&) = delete;
    PassphraseParamsCleanser& operator=(const PassphraseParamsCleanser&) = delete;

private:
    std::string m_method;
    const UniValue& m_params;
};

#endif // BITCOIN_RPC_PASSPHRASE_PARAMS_H
