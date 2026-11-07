// Copyright (c) 2020-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/gridcoin-config.h>
#endif

#include <tinyformat.h>
#include <util/syserror.h>

#include <algorithm>
#include <cstring>
#ifdef WIN32
#include <system_error>
#endif

std::string SysErrorString(int err)
{
    char buf[1024];
    /* Too bad there are three incompatible implementations of the
     * thread-safe strerror. */
    const char *s = nullptr;
#ifdef WIN32
    if (strerror_s(buf, sizeof(buf), err) == 0) s = buf;
#else
#ifdef STRERROR_R_CHAR_P /* GNU variant can return a pointer outside the passed buffer */
    s = strerror_r(err, buf, sizeof(buf));
#else /* POSIX variant always returns message in buffer */
    if (strerror_r(err, buf, sizeof(buf)) == 0) s = buf;
#endif
#endif
    if (s != nullptr) {
        return strprintf("%s (%d)", s, err);
    } else {
        return strprintf("Unknown error (%d)", err);
    }
}

std::string PrintableAscii(std::string message)
{
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ' ||
                                message.back() == '.')) {
        message.pop_back();
    }

    const bool printable = std::all_of(message.begin(), message.end(), [](char c) { return c >= 0x20 && c < 0x7f; });

    return printable ? message : std::string();
}

#ifdef WIN32
std::string Win32ErrorText(unsigned long error)
{
    const std::string message = PrintableAscii(std::system_category().message(static_cast<int>(error)));

    if (message.empty()) return strprintf("error %lu", error);

    return strprintf("%s (error %lu)", message, error);
}
#endif
