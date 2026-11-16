// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/sendfeenegotiator.h"

SendFeeNegotiator::Step SendFeeNegotiator::onResult(const WalletModel::SendCoinsReturn& result)
{
    Step step;
    if (result.status != WalletModel::FeeConfirmationRequired) {
        step.kind = Step::Done;
        step.result = result;
        return step;
    }
    if (m_accepted_prompts >= MAX_FEE_PROMPTS) {
        // A visible error rather than Aborted: the user has been confirming
        // fees, so a silent no-op would read as a sent transaction. Nothing
        // was committed.
        step.kind = Step::Done;
        step.result = WalletModel::SendCoinsReturn(WalletModel::TransactionCreationFailed);
        return step;
    }
    m_asked_fee = result.fee;
    step.kind = Step::Confirm;
    step.fee = result.fee;
    return step;
}

SendFeeNegotiator::Step SendFeeNegotiator::onAnswer(bool yes)
{
    Step step;
    if (!yes) {
        step.kind = Step::Done;
        step.result = WalletModel::SendCoinsReturn(WalletModel::Aborted);
        return step;
    }
    // The accepted fee only ratchets up: a request asks only for a fee above
    // the one accepted so far.
    m_accepted_fee = m_asked_fee;
    ++m_accepted_prompts;
    step.kind = Step::Send;
    return step;
}
