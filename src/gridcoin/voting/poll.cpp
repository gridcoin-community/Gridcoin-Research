// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "gridcoin/support/xml.h"
#include "gridcoin/voting/poll.h"
#include "span.h"
#include "ui_interface.h"
#include "util.h"

#include <boost/algorithm/string/join.hpp>

using namespace GRC;

namespace {
//!
//! \brief Parse the weight type from a legacy poll contract.
//!
//! \param value A numeric value extracted from the "<SHARETYPE>" field that
//! matches a value enumerated on \c PollWeightType.
//!
//! \return The matching weight type or the value of "unknown" when the string
//! does not contain a valid weight type number.
//!
PollWeightType ParseWeightType(const std::string& value)
{
    try {
        const uint32_t parsed = std::stoul(value);

        if (parsed > Poll::WeightType::MAX) {
            return PollWeightType::UNKNOWN;
        }

        return static_cast<PollWeightType>(parsed);
    } catch (...) {
        return PollWeightType::UNKNOWN;
    }
}

//!
//! \brief Parse the poll duration from a legacy poll contract.
//!
//! \param value A numeric value extracted from the "<DAYS>" field.
//!
//! \return The parsed number of days or zero when the string does not contain
//! a valid number.
//!
uint32_t ParseDurationDays(const std::string& value)
{
    try {
        return std::stoul(value);
    } catch (...) {
        return 0;
    }
}

//!
//! \brief Parse the set of available choices from a legacy poll contract.
//!
//! \param value A semicolon-delimited string of labels extracted from the
//! "<ANSWERS>" field. Each label represents one choice.
//!
//! \return The parsed set of poll choices.
//!
Poll::ChoiceList ParseChoices(const std::string& value)
{
    Poll::ChoiceList choices;

    for (auto& label : split(value, ";")) {
        choices.Add(std::move(label));
    }

    return choices;
}

//!
//! \brief Poll choices to return for polls with a yes/no/abstain response type.
//!
const Poll::ChoiceList g_yes_no_abstain_choices(std::vector<Poll::Choice> {
    { "Yes"     },
    { "No"      },
    { "Abstain" },
});
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Class: Poll
// -----------------------------------------------------------------------------

Poll::Poll()
    : m_type(PollType::UNKNOWN)
    , m_weight_type(PollWeightType::UNKNOWN)
    , m_response_type(PollResponseType::UNKNOWN)
    , m_duration_days(0)
    , m_timestamp(0)
{
}

Poll::Poll(
    PollType type,
    PollWeightType weight_type,
    PollResponseType response_type,
    uint32_t duration_days,
    std::string title,
    std::string url,
    std::string question,
    ChoiceList choices,
    int64_t timestamp)
    : m_type(type)
    , m_weight_type(weight_type)
    , m_response_type(response_type)
    , m_duration_days(duration_days)
    , m_title(std::move(title))
    , m_url(std::move(url))
    , m_question(std::move(question))
    , m_choices(std::move(choices))
    , m_timestamp(timestamp)
{
}

Poll Poll::Parse(const std::string& title, const std::string& contract)
{
    return Poll(
        PollType::SURVEY,
        ParseWeightType(ExtractXML(contract, "<SHARETYPE>", "</SHARETYPE>")),
        // Legacy contracts only behaved as multiple-choice polls:
        PollResponseType::MULTIPLE_CHOICE,
        ParseDurationDays(ExtractXML(contract, "<DAYS>", "</DAYS>")),
        title,
        ExtractXML(contract, "<URL>", "</URL>"),
        ExtractXML(contract, "<QUESTION>", "</QUESTION>"),
        ParseChoices(ExtractXML(contract, "<ANSWERS>", "</ANSWERS>")),
        0);
}

bool Poll::WellFormed() const
{
    return m_type != PollType::UNKNOWN
        && m_type != PollType::OUT_OF_BOUND
        && m_weight_type != PollWeightType::UNKNOWN
        && m_weight_type != PollWeightType::OUT_OF_BOUND
        && m_response_type != PollResponseType::UNKNOWN
        && m_response_type != PollResponseType::OUT_OF_BOUND
        && m_duration_days >= MIN_DURATION_DAYS
        && m_duration_days <= MAX_DURATION_DAYS
        && !m_title.empty()
        && !m_url.empty()
        && m_choices.WellFormed(m_response_type.Value());
}

bool Poll::WellFormed(const uint32_t version) const
{
    if (!WellFormed()) {
        return false;
    }

    // Version 2+ poll contracts in block version 11 and greater disallow the
    // deprecated legacy vote weighing methods:
    //
    if (version >= 2) {
        switch (m_weight_type.Value()) {
            case PollWeightType::MAGNITUDE:
            case PollWeightType::CPID_COUNT:
            case PollWeightType::PARTICIPANT_COUNT:
                return false;
            default:
                break;
        }
    }

    return true;
}

bool Poll::IncludesMagnitudeWeight() const
{
    return m_weight_type == PollWeightType::BALANCE_AND_MAGNITUDE
        || m_weight_type == PollWeightType::MAGNITUDE;
}

bool Poll::AllowsMultipleChoices() const
{
    return m_response_type == PollResponseType::MULTIPLE_CHOICE;
}

int64_t Poll::Age(const int64_t now) const
{
    return now - m_timestamp;
}

bool Poll::Expired(const int64_t now) const
{
    return Age(now) > m_duration_days * 86400;
}

int64_t Poll::Expiration() const
{
    return m_timestamp + (m_duration_days * 86400);
}

const Poll::ChoiceList& Poll::Choices() const
{
    if (m_response_type == PollResponseType::YES_NO_ABSTAIN) {
        return g_yes_no_abstain_choices;
    }

    return m_choices;
}

std::string Poll::WeightTypeToString() const
{
    switch (m_weight_type.Value()) {
        case PollWeightType::UNKNOWN:
        case PollWeightType::OUT_OF_BOUND:          return _("Unknown");
        case PollWeightType::MAGNITUDE:             return _("Magnitude");
        case PollWeightType::BALANCE:               return _("Balance");
        case PollWeightType::BALANCE_AND_MAGNITUDE: return _("Magnitude+Balance");
        case PollWeightType::CPID_COUNT:            return _("CPID Count");
        case PollWeightType::PARTICIPANT_COUNT:     return _("Participant Count");
    }

    assert(false); // Suppress warning
}

std::string Poll::ResponseTypeToString() const
{
    switch (m_response_type.Value()) {
        case PollResponseType::UNKNOWN:
        case PollResponseType::OUT_OF_BOUND:    return _("Unknown");
        case PollResponseType::YES_NO_ABSTAIN:  return _("Yes/No/Abstain");
        case PollResponseType::SINGLE_CHOICE:   return _("Single Choice");
        case PollResponseType::MULTIPLE_CHOICE: return _("Multiple Choice");
    }

    assert(false); // Suppress warning
}

// -----------------------------------------------------------------------------
// Class: Poll::ChoiceList
// -----------------------------------------------------------------------------

using ChoiceList = Poll::ChoiceList;
using Choice = Poll::Choice;

ChoiceList::ChoiceList(std::vector<Choice> choices)
    : m_choices(std::move(choices))
{
}

ChoiceList::const_iterator ChoiceList::begin() const
{
    return m_choices.begin();
}

ChoiceList::const_iterator ChoiceList::end() const
{
    return m_choices.end();
}

size_t ChoiceList::size() const
{
    return m_choices.size();
}

bool ChoiceList::empty() const
{
    return m_choices.empty();
}

bool ChoiceList::WellFormed(const PollResponseType response_type) const
{
    if (response_type == PollResponseType::YES_NO_ABSTAIN) {
        return m_choices.empty();
    }

    return !m_choices.empty()
        && m_choices.size() <= POLL_MAX_CHOICES_SIZE
        && std::all_of(
            m_choices.begin(),
            m_choices.end(),
            [](const Choice& choice) { return choice.WellFormed(); });
}

bool ChoiceList::OffsetInRange(const size_t offset) const
{
    return offset < m_choices.size();
}

bool ChoiceList::LabelExists(const std::string& label) const
{
    return OffsetOf(label).operator bool();
}

boost::optional<uint8_t> ChoiceList::OffsetOf(const std::string& label) const
{
    const auto iter = std::find_if(
        m_choices.begin(),
        m_choices.end(),
        [&](const Choice& choice) { return choice.m_label == label; });

    if (iter == m_choices.end()) {
        return boost::none;
    }

    return std::distance(m_choices.begin(), iter);
}

const Choice* ChoiceList::At(const size_t offset) const
{
    if (offset >= m_choices.size()) {
        return nullptr;
    }

    return &m_choices[offset];
}

void ChoiceList::Add(std::string label)
{
    m_choices.emplace_back(std::move(label));
}
