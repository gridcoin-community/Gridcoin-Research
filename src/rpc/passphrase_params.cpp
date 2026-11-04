// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/passphrase_params.h>

#include <support/cleanse.h>

#include <map>
#include <utility>

const std::vector<int>* PassphraseParamIndexes(const std::string& method)
{
    static const std::map<std::string, std::vector<int>> table{
        {"walletpassphrase", {0}},
        {"walletpassphrasechange", {0, 1}},
        {"encryptwallet", {0}},
    };

    const auto it = table.find(method);

    return it == table.end() ? nullptr : &it->second;
}

void CleansePassphraseParams(const std::string& method, const UniValue& params)
{
    const std::vector<int>* indexes = PassphraseParamIndexes(method);

    if (indexes == nullptr || !params.isArray()) return;

    for (const int index : *indexes) {
        if (index < 0 || static_cast<size_t>(index) >= params.size()) continue;

        const UniValue& value = params[static_cast<size_t>(index)];

        if (!value.isStr()) continue;

        // The bytes are overwritten where they are held, so the const is cast away on purpose.
        const std::string& text = value.get_str();
        if (!text.empty()) memory_cleanse(const_cast<char*>(text.data()), text.size());
    }
}

PassphraseParamsCleanser::PassphraseParamsCleanser(std::string method, const UniValue& params)
    : m_method(std::move(method)),
      m_params(params)
{
}

PassphraseParamsCleanser::~PassphraseParamsCleanser()
{
    CleansePassphraseParams(m_method, m_params);
}
