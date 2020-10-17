// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/support/xml.h"
#include "gridcoin/voting/vote.h"

using namespace GRC;

// -----------------------------------------------------------------------------
// Class: Vote
// -----------------------------------------------------------------------------

Vote::Vote(
    const uint32_t version,
    const uint256 poll_txid,
    std::vector<uint8_t> responses,
    VoteWeightClaim claim)
    : m_version(version)
    , m_poll_txid(poll_txid)
    , m_responses(std::move(responses))
    , m_claim(std::move(claim))
{
}

bool Vote::ResponseExists(const uint8_t offset) const
{
    return std::any_of(
        m_responses.begin(),
        m_responses.end(),
        [&](const uint8_t response) { return response == offset; });
}

// -----------------------------------------------------------------------------
// Class: LegacyVote
// -----------------------------------------------------------------------------

LegacyVote::LegacyVote(
    std::string key,
    MiningId mining_id,
    double amount,
    double magnitude,
    std::string responses)
    : m_key(std::move(key))
    , m_mining_id(mining_id)
    , m_amount(amount)
    , m_magnitude(magnitude)
    , m_responses(std::move(responses))
{
}

LegacyVote LegacyVote::Parse(const std::string& key, const std::string& value)
{
    const auto parse_double = [](const std::string& value, const double places) {
        const double scale = std::pow(10, places);
        return std::nearbyint(strtod(value.c_str(), nullptr) * scale) / scale;
    };

    return LegacyVote(
        key,
        MiningId::Parse(ExtractXML(value, "<CPID>", "</CPID>")),
        parse_double(ExtractXML(value, "<BALANCE>", "</BALANCE>"), 0),
        parse_double(ExtractXML(value, "<MAGNITUDE>", "</MAGNITUDE>"), 2),
        ExtractXML(value, "<ANSWER>", "</ANSWER>"));
}

std::vector<std::pair<uint8_t, uint64_t>>
LegacyVote::ParseResponses(const std::map<std::string, uint8_t>& choice_map) const
{
    std::vector<std::string> answers = split(m_responses, ";");
    std::vector<std::pair<uint8_t, uint64_t>> responses;

    for (auto& answer : answers) {
        boost::to_lower(answer);

        auto iter = choice_map.find(answer);

        if (iter != choice_map.end()) {
            responses.emplace_back(iter->second, 0);
        }
    }

    return responses;
}
