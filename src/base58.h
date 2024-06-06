// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

/**
 * Why base-58 instead of standard base-64 encoding?
 * - Don't want 0OIl characters that look the same in some fonts and
 *      could be used to create visually identical looking data.
 * - A string with non-alphanumeric characters is not as easily accepted as input.
 * - E-mail usually won't line-break if there's no punctuation to break at.
 * - Double-clicking selects the whole string as one word if it's all alphanumeric.
 */
#ifndef BITCOIN_BASE58_H
#define BITCOIN_BASE58_H

#include <attributes.h>
#include <span.h>

#include <string>
#include <vector>

#include "key.h"
#include "script.h"

/**
 * Encode a byte span as a base58-encoded string
 */
std::string EncodeBase58(Span<const unsigned char> input);

/**
 * Encode a byte sequence as a base58-encoded string. (Retained for compatibility until other code is
 * converted to use spans.)
 */
std::string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend);

/**
 * Decode a base58-encoded string (str) into a byte vector (vchRet).
 * return true if decoding is successful.
 */
[[nodiscard]] bool DecodeBase58(const std::string& str, std::vector<unsigned char>& vchRet,
                                int max_ret_len = std::numeric_limits<int>::max());

/**
 * Encode a byte span into a base58-encoded string, including checksum
 */
std::string EncodeBase58Check(Span<const unsigned char> input);

/**
 * Encode a byte vector to a base58-encoded string, including checksum. (Retained for compatibility until other code
 * is converted to use spans.)
 */
std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn);

/**
 * Decode a base58-encoded string (str) that includes a checksum into a byte
 * vector (vchRet), return true if decoding is successful. The max_ret_len is not used yet and defaulted
 * to the integer numeric limit max - 4. This is reserved for more Bitcoin upstream porting.
 */
[[nodiscard]] bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet,
                                     int max_ret_len = std::numeric_limits<int>::max() - 4);

#endif
