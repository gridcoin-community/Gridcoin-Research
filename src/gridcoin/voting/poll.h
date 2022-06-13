// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_VOTING_POLL_H
#define GRIDCOIN_VOTING_POLL_H

#include "gridcoin/support/enumbytes.h"
#include "gridcoin/voting/fwd.h"
#include "serialize.h"
#include "uint256.h"

#include <string>
#include <vector>

namespace GRC {
//!
//! \brief A contract that contains a prompt for responses from people using
//! the Gridcoin network.
//!
class Poll
{
public:
    using Type = EnumByte<PollType>;
    using WeightType = EnumByte<PollWeightType>;
    using ResponseType = EnumByte<PollResponseType>;

    //!
    //! \brief Minimum duration that a poll must remain active for. This is a global rule.
    //!
    static constexpr uint32_t MIN_DURATION_DAYS = 7;

    //!
    //! \brief Maximum duration that a poll cannot remain active after. This is a global rule.
    //!
    static constexpr uint32_t MAX_DURATION_DAYS = 180;

    //!
    //! \brief Maximum allowed length of a poll title. This is a global rule.
    //!
    static constexpr size_t MAX_TITLE_SIZE = 80;

    //!
    //! \brief Maximum allowed length of a poll URL. This is a global rule.
    //!
    static constexpr size_t MAX_URL_SIZE = 100;

    //!
    //! \brief Maximum allowed length of a poll question. This is a global rule.
    //!
    static constexpr size_t MAX_QUESTION_SIZE = 100;

    //!
    //! \brief Represents an answer that a voter can choose when responding to
    //! a poll.
    //!
    class Choice
    {
    public:
        //!
        //! \brief The maximum length for a poll choice label.
        //!
        static constexpr size_t MAX_LABEL_SIZE = 100;

        std::string m_label; // UTF-8 display text that describes the choice.

        //!
        //! \brief Initialize an empty, invalid poll choice.
        //!
        Choice()
        {
        }

        //!
        //! \brief Initialize a poll choice with the specified label.
        //!
        //! \param label UTF-8 display text that describes the choice.
        //!
        Choice(std::string label) : m_label(std::move(label))
        {
        }

        //!
        //! \brief Determine whether a poll choice is complete.
        //!
        bool WellFormed() const
        {
            return !m_label.empty();
        }

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            READWRITE(LIMITED_STRING(m_label, MAX_LABEL_SIZE));
        }
    }; // Choice

    //!
    //! \brief Contains the set of answers that a voter can choose when
    //! responding to a poll.
    //!
    class ChoiceList
    {
    public:
        using const_iterator = std::vector<Choice>::const_iterator;

        //!
        //! \brief Initialize an empty choice list.
        //!
        ChoiceList()
        {
        }

        //!
        //! \brief Initialize a choice list with the supplied choices.
        //!
        ChoiceList(std::vector<Choice> choices);

        //!
        //! \brief Returns an iterator to the beginning.
        //!
        const_iterator begin() const;

        //!
        //! \brief Returns an iterator to the end.
        //!
        const_iterator end() const;

        //!
        //! \brief Get the number of choices in the poll.
        //!
        size_t size() const;

        //!
        //! \brief Determine whether the poll contains no choices.
        //!
        bool empty() const;

        //!
        //! \brief Determine whether the choices are sufficient for a poll.
        //!
        //! \return \c false if the set of choices contains invalid entries or
        //! if the size of the set exceeds the range allowed for a poll.
        //!
        bool WellFormed(const PollResponseType response_type) const;

        //!
        //! \brief Determine whether the specified offset matches an offset in
        //! the range of poll choices.
        //!
        //! \return \c true if the offset does not exceed the bounds of the
        //! container.
        //!
        bool OffsetInRange(const size_t offset) const;

        //!
        //! \brief Determine whether the specified choice label already exists.
        //!
        //! \return \c true if another choice contains a matching label.
        //!
        bool LabelExists(const std::string& label) const;

        //!
        //! \brief Get the choice offset of the specified label.
        //!
        //! \param label The choice label to find the offset of.
        //!
        //! \return An object that either contains the offset of the label or
        //! does not when no choice contains a matching label.
        //!
        std::optional<uint8_t> OffsetOf(const std::string& label) const;

        //!
        //! \brief Get the poll choice at the specified offset.
        //!
        //! \return The choice at the specified offset or a null pointer if
        //! the offset exceeds the range of the choices.
        //!
        const Choice* At(const size_t offset) const;

        //!
        //! \brief Add a choice to the poll.
        //!
        //! \param label UTF-8 display text that represents the choice.
        //!
        void Add(std::string label);

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            READWRITE(m_choices);
        }

    private:
        std::vector<Choice> m_choices; //!< The set of choices for the poll.
    }; // ChoiceList

    class AdditionalField
    {
    public:
        //!
        //! \brief The maximum length for a poll additional field name or value.
        //!
        static constexpr size_t MAX_N_OR_V_SIZE = 100;

        std::string m_name;
        std::string m_value;
        bool m_required;

        //!
        //! \brief Initialize an empty, invalid additional field.
        //!
        AdditionalField()
        {
        }

        //!
        //! \brief Initialize an additional field for the poll with the specified parameters
        //! \param name     UTF-8 name of the field
        //! \param value    UTF-8 value of the field
        //! \param required bool whether the field is required
        //!
        AdditionalField(std::string name, std::string value, bool required)
            : m_name(std::move(name))
            , m_value(std::move(value))
            , m_required(std::move(required))
        {
        }

        //!
        //! \brief Determine whether a poll additional field name value pair is complete.
        //!
        bool WellFormed() const
        {
            return !m_name.empty() && (!m_required || !m_value.empty());
        }

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            READWRITE(LIMITED_STRING(m_name, MAX_N_OR_V_SIZE));
            READWRITE(LIMITED_STRING(m_value, MAX_N_OR_V_SIZE));
            READWRITE(m_required);
        }
    }; // AdditionalField

    class AdditionalFieldList
    {
    public:
        using const_iterator = std::vector<AdditionalField>::const_iterator;

        //!
        //! \brief Initialize an empty additional field list.
        //!
        AdditionalFieldList()
        {
        }

        //!
        //! \brief Initialize an additional field list with the supplied additional fields.
        //!
        AdditionalFieldList(std::vector<AdditionalField> additional_fields);

        //!
        //! \brief Returns an iterator to the beginning.
        //!
        const_iterator begin() const;

        //!
        //! \brief Returns an iterator to the end.
        //!
        const_iterator end() const;

        //!
        //! \brief Get the number of additional fields in the poll.
        //!
        size_t size() const;

        //!
        //! \brief Determine whether the poll contains no additional fields.
        //!
        bool empty() const;

        //!
        //! \brief Determine whether the additional fields are sufficient for a poll.
        //!
        //! \return \c false if the set of additional fields contains invalid entries or
        //! if the required additional fields are missing for a poll.
        //!
        bool WellFormed(const PollType poll_type) const;

        //!
        //! \brief Determine whether the specified offset matches an offset in
        //! the range of poll additional fields.
        //!
        //! \return \c true if the offset does not exceed the bounds of the
        //! container.
        //!
        bool OffsetInRange(const size_t offset) const;

        //!
        //! \brief Determine whether the specified additional field name-value pair already exists.
        //!
        //! \return \c true if another field name-value pair is already in the vector that contains a matching name.
        //!
        bool FieldExists(const std::string& name) const;

        //!
        //! \brief Get the choice offset of the specified field name.
        //!
        //! \param label The additional field name to find the offset of.
        //!
        //! \return An object that either contains the offset of the field name or
        //! does not when no additional field contains a matching name.
        //!
        std::optional<uint8_t> OffsetOf(const std::string& name) const;

        //!
        //! \brief Get the poll additional field at the specified offset.
        //!
        //! \return The additional field at the specified offset or a null pointer if
        //! the offset exceeds the range of the additional fields.
        //!
        const AdditionalField* At(const size_t offset) const;

        //!
        //! \brief Add an additional field to the poll.
        //!
        void Add(std::string name, std::string value, bool required);

        //!
        //! \brief Add an additional field to the poll.
        //!
        void Add(AdditionalField field);

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            READWRITE(m_additional_fields);
        }

    private:
        std::vector<AdditionalField> m_additional_fields; //!< The set of additional fields for the poll.
    };

    //!
    //! \brief Used for poll (payload) version 3+
    //!
    struct PollTypeRules
    {
        uint32_t m_mininum_duration;
        uint32_t m_min_vote_percent_AVW;
        std::vector<std::string> m_required_fields;
    };

    //!
    //! \brief Allows use of the PollType enum in range based for loops.
    //!
    static constexpr GRC::PollType POLL_TYPES[] = {
        PollType::UNKNOWN,
        PollType::SURVEY,
        PollType::PROJECT,
        PollType::DEVELOPMENT,
        PollType::GOVERNANCE,
        PollType::MARKETING,
        PollType::OUTREACH,
        PollType::COMMUNITY,
        PollType::OUT_OF_BOUND
    };

    //!
    //! \brief Poll rules that are specific to poll type. Enforced for poll payload version 3+.
    //!
    static const std::vector<Poll::PollTypeRules> POLL_TYPE_RULES;

    Type m_type;                             //!< Type of the poll.
    WeightType m_weight_type;                //!< Method used to weigh votes.
    ResponseType m_response_type;            //!< Method for choosing poll answers.
    uint32_t m_duration_days;                //!< Number of days the poll remains active.
    std::string m_title;                     //!< UTF-8 title of the poll.
    std::string m_url;                       //!< UTF-8 URL of the poll discussion webpage.
    std::string m_question;                  //!< UTF-8 prompt that voters shall answer.
    ChoiceList m_choices;                    //!< The set of possible answers to the poll.
    AdditionalFieldList m_additional_fields; //!< The set of additional fields for the poll.

    // Memory only:
    int64_t m_timestamp;          //!< Time of the poll's containing transaction.

    //!
    //! \brief Initialize an empty, invalid poll instance.
    //!
    Poll();

    //!
    //! \brief Initialize a poll instance with data from a contract.
    //!
    //! \param type          Type of the poll.
    //! \param weight_type   Method used to weigh votes.
    //! \param response_type Method for choosing poll answers.
    //! \param duration_days Number of days the poll remains active.
    //! \param title         UTF-8 title of the poll.
    //! \param url           UTF-8 URL of the poll discussion webpage.
    //! \param question      UTF-8 prompt that voters shall answer.
    //! \param choices       The set of possible answers to the poll.
    //! \param timestamp     Timestamp of the poll's containing transaction.
    //!
    Poll(
        PollType type,
        PollWeightType weight_type,
        PollResponseType response_type,
        uint32_t duration_days,
        std::string title,
        std::string url,
        std::string question,
        ChoiceList choices,
        int64_t timestamp);

    //!
    //! \brief Initialize a poll instance from a contract that contains poll
    //! data in the legacy, XML-like string format.
    //!
    //! \param title    Poll title extracted from the legacy contract key.
    //! \param contract Contains the poll data in a legacy, serialized format.
    //!
    //! \return A poll matching the data in the contract, or an invalid poll
    //! instance if the contract is malformed.
    //!
    static Poll Parse(const std::string& title, const std::string& contract);

    //!
    //! \brief Determine whether a poll contains each of the required elements.
    //!
    //! \return \c true if the poll is complete.
    //!
    bool WellFormed() const;

    //!
    //! \brief Determine whether a poll contains each of the required elements
    //! for a particular poll format.
    //!
    //! \param version Version number of the serialized poll format.
    //!
    //! \return \c true if the poll is complete.
    //!
    bool WellFormed(const uint32_t version) const;

    //!
    //! \brief Determine whether a poll factors in magnitude for voting weight.
    //!
    //! \return \c true if the poll weight type includes magnitude.
    //!
    bool IncludesMagnitudeWeight() const;

    //!
    //! \brief Determine whether a poll accepts more than one response choice
    //! per vote.
    //!
    //! \return \c true if the poll has a multiple choice response type.
    //!
    bool AllowsMultipleChoices() const;

    //!
    //! \brief Get the elapsed time since poll creation.
    //!
    //! \param now Timestamp to consider as the current time.
    //!
    //! \return Poll age in seconds.
    //!
    int64_t Age(const int64_t now) const;

    //!
    //! \brief Determine whether the poll age exceeds the duration of the poll.
    //!
    //! \param now Timestamp to consider as the current time.
    //!
    //! \return \c true if a poll's age exceeds the poll duration.
    //!
    bool Expired(const int64_t now) const;

    //!
    //! \brief Get the time when the poll expires.
    //!
    //! \return Expiration time as the number of seconds since the UNIX epoch.
    //!
    int64_t Expiration() const;

    //!
    //! \brief Get the set of possible answers to the poll.
    //!
    const ChoiceList& Choices() const;

    //!
    //! \brief Get the set of additional fields in the poll.
    //!
    const AdditionalFieldList& AdditionalFields() const;

    //!
    //! \brief Get the string representation of the poll type for the poll object.
    //!
    std::string PollTypeToString() const;

    //!
    //! \brief Get the string representation of the poll type for the provided poll type.
    //! \param type
    //!
    static std::string PollTypeToString(const PollType& type, const bool& translated = true);

    //!
    //! \brief Get the poll type description string for the poll object.
    //!
    std::string PollTypeToDescString() const;

    //!
    //! \brief Get the poll type description string for the provided poll type.
    //! \param type
    //!
    static std::string PollTypeToDescString(const PollType& type);

    //!
    //! \brief Get the string representation of the poll's weight type.
    //!
    std::string WeightTypeToString() const;

    //!
    //! \brief Get the string representation of the poll's response type.
    //!
    std::string ResponseTypeToString() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_type);
        READWRITE(m_weight_type);
        READWRITE(m_response_type);
        READWRITE(m_duration_days);
        READWRITE(LIMITED_STRING(m_title, MAX_TITLE_SIZE));
        READWRITE(LIMITED_STRING(m_url, MAX_URL_SIZE));
        READWRITE(LIMITED_STRING(m_question, MAX_QUESTION_SIZE));

        if (m_response_type != PollResponseType::YES_NO_ABSTAIN) {
            READWRITE(m_choices);
        }

        // Note: this is a little dirty but works, because all polls prior to v3 are SURVEY, and the
        // additional fields for survey is an empty vector. Therefore this serialization will only
        // be operative if a poll type other than survey is used, and this cannot occur until v3+.
        // Refer to the comments in POLL_TYPE_RULES. This is necessary because the only other solution would be
        // to pass the poll payload version into the poll object, which would be problematic.
        //
        // TODO: Remove COMMUNITY after finishing isolated fork testing. (Community was used to test v3 polls
        // before the introduction of additional fields, and therefore the community polls on the isolated
        // testing fork do not have the m_additional_fields serialization and removal of the COMMUNITY below
        // will result in an serialization I/O error.
        if (m_type != PollType::SURVEY && m_type != PollType::COMMUNITY) {
            READWRITE(m_additional_fields);
        }
    }
}; // Poll
} // namespace GRC

#endif // GRIDCOIN_VOTING_POLL_H
