// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <string>

#include "gridcoin/support/xml.h"
#include "gridcoin/tx_message.h"

std::string GetMessage(const CTransaction& tx)
{
    if (tx.nVersion <= 1) {
        return ExtractXML(tx.hashBoinc, "<MESSAGE>", "</MESSAGE>");
    }

    if (tx.vContracts.empty()) {
        return std::string();
    }

    if (tx.vContracts.front().m_type != GRC::ContractType::MESSAGE) {
        return std::string();
    }

    const auto payload = tx.vContracts.front().SharePayloadAs<GRC::TxMessage>();

    return payload->m_message;
} 
