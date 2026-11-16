// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SENDFEENEGOTIATOR_H
#define BITCOIN_QT_SENDFEENEGOTIATOR_H

#include "qt/walletmodel.h"

#include <QtGlobal>

//! The fee-confirmation loop of one send, as state: the fee the user has
//! accepted so far and how many fee prompts were accepted. An attempt that
//! needs a higher fee asks for it; at most MAX_FEE_PROMPTS are accepted per
//! send, and the next request for a higher fee ends the send as
//! TransactionCreationFailed. No widgets and no strings: each caller asks the
//! question itself.
class SendFeeNegotiator
{
public:
    //! The most fee prompts one send accepts.
    static constexpr int MAX_FEE_PROMPTS = 10;

    //! What the caller does next.
    struct Step {
        enum Kind {
            //! Send again, with acceptedFee().
            Send,
            //! Ask the user to accept `fee`, then pass the answer to onAnswer().
            Confirm,
            //! The send is over; `result` is its outcome.
            Done,
        };
        Kind kind{Send};
        qint64 fee{0};
        WalletModel::SendCoinsReturn result;
    };

    //! The fee to pass to the next attempt.
    qint64 acceptedFee() const { return m_accepted_fee; }

    //! Takes an attempt's result. A result that does not ask for a fee is Done
    //! with that result unchanged (status, fee and hex). A fee request once
    //! MAX_FEE_PROMPTS prompts were accepted is Done with
    //! TransactionCreationFailed; any other is Confirm with the fee.
    Step onResult(const WalletModel::SendCoinsReturn& result);
    //! Takes the answer to the last Confirm: no is Done with Aborted; yes
    //! accepts the fee and is Send.
    Step onAnswer(bool yes);

private:
    qint64 m_accepted_fee{0};
    qint64 m_asked_fee{0};
    int m_accepted_prompts{0};
};

#endif // BITCOIN_QT_SENDFEENEGOTIATOR_H
