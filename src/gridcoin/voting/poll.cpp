// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/protocol.h"
#include "main.h"
#include "gridcoin/support/xml.h"
#include "gridcoin/voting/poll.h"
#include "span.h"
#include "node/ui_interface.h"
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
        uint32_t parsed = 0;

        if (!ParseUInt32(value, &parsed) || parsed > Poll::WeightType::MAX) {
            return PollWeightType::UNKNOWN;
        }

        return static_cast<PollWeightType>(parsed);
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
    uint32_t duration = 0;

    if (!ParseUInt32(value, &duration)) {
        error("%s: Unable to parse poll duration from legacy poll contract. Input string is %s.",
              __func__, value);
    }

    return duration;
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
    , m_title({})
    , m_url({})
    , m_question({})
    , m_choices(ChoiceList())
    , m_timestamp(0)
    , m_magnitude_weight_factor(Fraction())
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
    int64_t timestamp,
    Fraction magnitude_weight_factor)
    : m_type(type)
    , m_weight_type(weight_type)
    , m_response_type(response_type)
    , m_duration_days(duration_days)
    , m_title(std::move(title))
    , m_url(std::move(url))
    , m_question(std::move(question))
    , m_choices(std::move(choices))
    , m_timestamp(timestamp)
    , m_magnitude_weight_factor(magnitude_weight_factor)
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
        0,
        Params().GetConsensus().DefaultMagnitudeWeightFactor);
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

    if (version >= 3 && !m_additional_fields.WellFormed(m_type.Value())) {
        return false;
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

uint32_t Poll::Duration() const
{
    return m_duration_days;
}

bool Poll::Expired(const int64_t now) const
{
    return Age(now) > m_duration_days * 86400;
}

int64_t Poll::Expiration() const
{
    return m_timestamp + (m_duration_days * 86400);
}

Fraction Poll::ResolveMagnitudeWeightFactor(CBlockIndex *index) const
{
    if (index == nullptr) {
        return Fraction();
    }

    Fraction magnitude_weight_factor = Params().GetConsensus().DefaultMagnitudeWeightFactor;

    // Before V13 magnitude weight factor is 1 / 5.67.
    if (!IsV13Enabled(index->nHeight)) {
        return Fraction(100, 567);
    }

    // Find the current protocol entry value for Magnitude Weight Factor, if it exists.
    ProtocolEntryOption protocol_entry = GetProtocolRegistry().TryLastBeforeTimestamp("magnitudeweightfactor", m_timestamp);

    // If their is an entry prior or equal in timestemp to the start of the poll and it is active then set the magnitude weight
    // factor to that value. If the last entry is not active (i.e. deleted), then leave at the default.
    if (protocol_entry != nullptr && protocol_entry->m_status == ProtocolEntryStatus::ACTIVE) {
        magnitude_weight_factor = Fraction().FromString(protocol_entry->m_value);
    }

    return magnitude_weight_factor;
}

const Poll::ChoiceList& Poll::Choices() const
{
    if (m_response_type == PollResponseType::YES_NO_ABSTAIN) {
        return g_yes_no_abstain_choices;
    }

    return m_choices;
}

const Poll::AdditionalFieldList& Poll::AdditionalFields() const
{
    return m_additional_fields;
}

std::string Poll::PollTypeToString() const
{
    return PollTypeToString(m_type.Value());
}

std::string Poll::PollTypeToString(const PollType& type, const bool& translated)
{
    if (translated) {
        switch(type) {
        case PollType::UNKNOWN:         return _("Unknown");
        case PollType::SURVEY:          return _("Survey");
        case PollType::PROJECT:         return _("Project Listing");
        case PollType::DEVELOPMENT:     return _("Protocol Development");
        case PollType::GOVERNANCE:      return _("Governance");
        case PollType::MARKETING:       return _("Marketing");
        case PollType::OUTREACH:        return _("Outreach");
        case PollType::COMMUNITY:       return _("Community");
        case PollType::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    } else {
        // The untranslated versions are really meant to serve as the string equivalent of the enum values.
        switch(type) {
        case PollType::UNKNOWN:         return "unknown";
        case PollType::SURVEY:          return "survey";
        case PollType::PROJECT:         return "project";
        case PollType::DEVELOPMENT:     return "development";
        case PollType::GOVERNANCE:      return "governance";
        case PollType::MARKETING:       return "marketing";
        case PollType::OUTREACH:        return "outreach";
        case PollType::COMMUNITY:       return "community";
        case PollType::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    }

    // This will never be reached. Put it in anyway to prevent control reaches end of non-void function warning
    // from some compiler versions.
    return std::string{};
}

std::string Poll::PollTypeToDescString() const
{
    return PollTypeToDescString(m_type.Value());
}


std::string Poll::PollTypeToDescString(const PollType& type)
{
    switch(type) {
    case PollType::UNKNOWN:         return _("Unknown poll type. This should never happen.");
    case PollType::SURVEY:          return _("For opinion or casual polls without any particular requirements.");
    case PollType::PROJECT:         return _("Propose additions or removals of computing projects for research reward "
                                             "eligibility.");
    case PollType::DEVELOPMENT:     return _("Propose a change to Gridcoin at the protocol level.");
    case PollType::GOVERNANCE:      return _("Proposals related to Gridcoin management like poll requirements and funding.");
    case PollType::MARKETING:       return _("Propose marketing initiatives like ad campaigns.");
    case PollType::OUTREACH:        return _("For polls about community representation, public relations, and "
                                             "communications.");
    case PollType::COMMUNITY:       return _("For initiatives related to the Gridcoin community not covered by other "
                                             "poll types.");
    case PollType::OUT_OF_BOUND:    break;
    }

    assert(false); // Suppress warning
}

std::string Poll::WeightTypeToString() const
{
    switch (m_weight_type.Value()) {
    case PollWeightType::UNKNOWN:               return std::string{};
    case PollWeightType::MAGNITUDE:             return _("Magnitude");
    case PollWeightType::BALANCE:               return _("Balance");
    case PollWeightType::BALANCE_AND_MAGNITUDE: return _("Magnitude+Balance");
    case PollWeightType::CPID_COUNT:            return _("CPID Count");
    case PollWeightType::PARTICIPANT_COUNT:     return _("Participant Count");
    case PollWeightType::OUT_OF_BOUND:          break;
    }

    assert(false); // Suppress warning
}

std::string Poll::ResponseTypeToString() const
{
    switch (m_response_type.Value()) {
    case PollResponseType::UNKNOWN:         return std::string{};
    case PollResponseType::YES_NO_ABSTAIN:  return _("Yes/No/Abstain");
    case PollResponseType::SINGLE_CHOICE:   return _("Single Choice");
    case PollResponseType::MULTIPLE_CHOICE: return _("Multiple Choice");
    case PollResponseType::OUT_OF_BOUND:    break;
    }

    assert(false); // Suppress warning
}

const std::vector<Poll::PollTypeRules> Poll::POLL_TYPE_RULES = {
    // These must be kept in the order that corresponds to the PollType enum.
    // { min duration, min vote percent AVW, { vector of required additional fieldnames } }
    {  0,  0, {} },                                // PollType::UNKNOWN
    // Note that any poll type that has a min vote percent AVW requirement must
    // also require the weight type of BALANCE_AND_MAGNITUDE, so therefore the
    // only poll type that can actually use BALANCE is SURVEY. All other WeightTypes are deprecated.
    {  7,  0, {} },                                // PollType::SURVEY
    { 21, 20, { "project_name", "project_url" } }, // PollType::PROJECT
    { 42, 35, {} },                                // PollType::DEVELOPMENT
    { 21, 15, {} },                                // PollType::GOVERNANCE
    { 21, 20, {} },                                // PollType::MARKETING
    { 21, 20, {} },                                // PollType::OUTREACH
    { 21, 10, {} }                                 // PollType::COMMUNITY
};

// -----------------------------------------------------------------------------
// Class: Poll::AdditionalFieldList
// -----------------------------------------------------------------------------
using AdditionalFieldList = Poll::AdditionalFieldList;
using AdditionalField = Poll::AdditionalField;

AdditionalFieldList::AdditionalFieldList(std::vector<AdditionalField> additional_fields)
    : m_additional_fields(std::move(additional_fields))
{
}

AdditionalFieldList::const_iterator AdditionalFieldList::begin() const
{
    return m_additional_fields.begin();
}

AdditionalFieldList::const_iterator AdditionalFieldList::end() const
{
    return m_additional_fields.end();
}

size_t AdditionalFieldList::size() const
{
    return m_additional_fields.size();
}

bool AdditionalFieldList::empty() const
{
    return m_additional_fields.empty();
}

bool AdditionalFieldList::WellFormed(const PollType poll_type) const
{
    if (m_additional_fields.size() > POLL_MAX_ADDITIONAL_FIELDS_SIZE) {
        return false;
    }

    const std::vector<std::string> required_field_names = Poll::POLL_TYPE_RULES[(int) poll_type].m_required_fields;

    for (const auto& iter : required_field_names) {
        std::optional<uint8_t> offset = OffsetOf(iter);
        // If the field name (entry) does not exist, return false.
        if (!offset) return false;

        // If the field name (entry) m_required flag is not set properly then return false.
        if (At(*offset)->m_required != true) return false;

        // If the field value is empty, return false. A required field cannot have an empty value.
        if (At(*offset)->m_value.empty()) return false;
    }

    // We check to ensure that each field is well formed. We also need to check whether fields that are NOT required are
    // marked accordingly. This requires us to iterate through the m_additional_fields. If a field entry is not well formed
    // or if not found in the required fields list and the field entry is marked required, then return false.
    for (const auto& iter : m_additional_fields) {
        if (!iter.WellFormed()) {
            return false;
        }

        if (std::find(required_field_names.begin(), required_field_names.end(), iter.m_name) == required_field_names.end()
                && iter.m_required) {
            return false;
        }
    }

    return true;
}

bool AdditionalFieldList::OffsetInRange(const size_t offset) const
{
    return offset < m_additional_fields.size();
}

bool AdditionalFieldList::FieldExists(const std::string& name) const
{
    return OffsetOf(name).operator bool();
}

std::optional<uint8_t> AdditionalFieldList::OffsetOf(const std::string& name) const
{
    const auto iter = std::find_if(
        m_additional_fields.begin(),
        m_additional_fields.end(),
        [&](const AdditionalField& additional_field) { return additional_field.m_name == name; });

    if (iter == m_additional_fields.end()) {
        return std::nullopt;
    }

    return std::distance(m_additional_fields.begin(), iter);
}

const AdditionalField* AdditionalFieldList::At(const size_t offset) const
{
    if (offset >= m_additional_fields.size()) {
        return nullptr;
    }

    return &m_additional_fields[offset];
}

void AdditionalFieldList::Add(std::string name, std::string value, bool required)
{
    AdditionalField additional_field { name, value, required };

    m_additional_fields.emplace_back(std::move(additional_field));
}

void AdditionalFieldList::Add(AdditionalField field)
{
    m_additional_fields.emplace_back(std::move(field));
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

std::optional<uint8_t> ChoiceList::OffsetOf(const std::string& label) const
{
    const auto iter = std::find_if(
        m_choices.begin(),
        m_choices.end(),
        [&](const Choice& choice) { return choice.m_label == label; });

    if (iter == m_choices.end()) {
        return std::nullopt;
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
