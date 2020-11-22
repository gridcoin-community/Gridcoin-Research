// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

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
    //! \brief Minimum duration that a poll must remain active for.
    //!
    static constexpr uint32_t MIN_DURATION_DAYS = 7;

    //!
    //! \brief Maximum duration that a poll cannot remain active after.
    //!
    static constexpr uint32_t MAX_DURATION_DAYS = 180;

    //!
    //! \brief Maximum allowed length of a poll title.
    //!
    static constexpr size_t MAX_TITLE_SIZE = 80;

    //!
    //! \brief Maximum allowed length of a poll URL.
    //!
    static constexpr size_t MAX_URL_SIZE = 100;

    //!
    //! \brief Maximum allowed length of a poll question.
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
        boost::optional<uint8_t> OffsetOf(const std::string& label) const;

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

    Type m_type;                  //!< Type of the poll.
    WeightType m_weight_type;     //!< Method used to weigh votes.
    ResponseType m_response_type; //!< Method for choosing poll answers.
    uint32_t m_duration_days;     //!< Number of days the poll remains active.
    std::string m_title;          //!< UTF-8 title of the poll.
    std::string m_url;            //!< UTF-8 URL of the poll discussion webpage.
    std::string m_question;       //!< UTF-8 prompt that voters shall answer.
    ChoiceList m_choices;         //!< The set of possible answers to the poll.

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
    }
}; // Poll
}
