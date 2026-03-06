// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_VALIDATION_H
#define BITCOIN_CONSENSUS_VALIDATION_H

#include <string>

/** Capture information about block/transaction validation. */
class CValidationState
{
private:
    enum mode_state {
        MODE_VALID,   //!< everything ok
        MODE_INVALID, //!< network rule violation (DoS value may be set)
        MODE_ERROR,   //!< run-time error
    } mode;
    int nDoS;
    std::string strRejectReason;

public:
    CValidationState() : mode(MODE_VALID), nDoS(0) {}

    bool DoS(int level, bool ret = false,
             const std::string& strRejectReasonIn = "")
    {
        nDoS += level;
        mode = MODE_INVALID;
        strRejectReason = strRejectReasonIn;
        return ret;
    }

    bool Invalid(bool ret = false,
                 const std::string& strRejectReasonIn = "")
    {
        return DoS(0, ret, strRejectReasonIn);
    }

    bool Error(const std::string& strRejectReasonIn = "")
    {
        mode = MODE_ERROR;
        strRejectReason = strRejectReasonIn;
        return false;
    }

    bool IsValid() const
    {
        return mode == MODE_VALID;
    }

    bool IsInvalid() const
    {
        return mode == MODE_INVALID;
    }

    bool IsInvalid(int& nDoSOut) const
    {
        if (IsInvalid()) {
            nDoSOut = nDoS;
            return true;
        }
        return false;
    }

    bool IsError() const
    {
        return mode == MODE_ERROR;
    }

    int GetDoS() const { return nDoS; }

    std::string GetRejectReason() const { return strRejectReason; }
};

#endif // BITCOIN_CONSENSUS_VALIDATION_H
