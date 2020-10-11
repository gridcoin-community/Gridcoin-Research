// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/cpid.h"
#include "util.h"

#include <algorithm>
#include <boost/variant/apply_visitor.hpp>
#include <openssl/md5.h>

using namespace GRC;

namespace {
//!
//! \brief Gets the string representation of a mining ID object.
//!
struct MiningIdToStringVisitor : boost::static_visitor<std::string>
{
    //!
    //! \brief Call the mining ID variant type's \c ToString() method directly.
    //!
    //! \param variant The object to create a string for.
    //!
    //! \return The string representation of the mining ID.
    //!
    template<typename T>
    std::string operator()(const T& variant) const
    {
        return variant.ToString();
    }
};
} // anonymous namespace

// -----------------------------------------------------------------------------
// Class: Cpid
// -----------------------------------------------------------------------------

Cpid::Cpid(const std::vector<unsigned char>& bytes)
{
    if (bytes.size() == 16) {
        std::copy_n(bytes.begin(), 16, m_bytes.begin());
    } else {
        m_bytes.fill(0x00);
    }
}

Cpid Cpid::Parse(const std::string& hex)
{
    if (hex.size() != 32) {
        return Cpid();
    }

    return Cpid(ParseHex(hex));
}

Cpid Cpid::Hash(const std::string& internal, const std::string& email)
{
    if (internal.empty() || email.empty()) {
        return Cpid();
    }

    Cpid cpid;

    // Even though the internal CPID is hex-encoded, BOINC creates the external
    // CPID hash from the hex string, not from the encoded bytes:
    //
    std::vector<unsigned char> input(internal.begin(), internal.end());
    input.insert(input.end(), email.begin(), email.end());

    MD5(input.data(), input.size(), cpid.m_bytes.data());

    return cpid;
}

bool Cpid::IsZero() const
{
    const auto zero = [](const unsigned char& i) { return i == 0; };

    return std::all_of(m_bytes.begin(), m_bytes.end(), zero);
}

bool Cpid::Matches(const std::string& internal, const std::string& email) const
{
    return m_bytes == Cpid::Hash(internal, email).m_bytes;
}

std::string Cpid::ToString() const
{
    return HexStr(m_bytes.begin(), m_bytes.end());
}

// -----------------------------------------------------------------------------
// Class: MiningId
// -----------------------------------------------------------------------------

MiningId MiningId::Parse(const std::string& input)
{
    if (input.empty()) {
        return MiningId();
    }

    if (input == "INVESTOR" || input == "investor") {
        return MiningId::ForInvestor();
    }

    if (input.size() == 32) {
        std::vector<unsigned char> bytes = ParseHex(input);

        if (bytes.size() == 16) {
            return MiningId(Cpid(bytes));
        }
    }

    return MiningId();
}

std::string MiningId::ToString() const
{
    return boost::apply_visitor(MiningIdToStringVisitor(), m_variant);
}

// -----------------------------------------------------------------------------
// Class: MiningId::Invalid
// -----------------------------------------------------------------------------

std::string MiningId::Invalid::ToString() const
{
    return std::string();
}

// -----------------------------------------------------------------------------
// Class: MiningId::Investor
// -----------------------------------------------------------------------------

std::string MiningId::Investor::ToString() const
{
    return "INVESTOR";
}
