// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2025 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/string.h>

#include <istream>
#include <streambuf>

using namespace std;

void ParseString(const string& str, char c, vector<string>& v)
{
    if (str.empty())
        return;
    string::size_type i1 = 0;
    string::size_type i2;
    while (true)
    {
        i2 = str.find(c, i1);
        if (i2 == str.npos)
        {
            v.push_back(str.substr(i1));
            return;
        }
        v.push_back(str.substr(i1, i2-i1));
        i1 = i2+1;
    }
}

std::string FromDoubleToString(const double& t, const int& precision)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::scientific << std::setprecision(precision) << t;
    return oss.str();
}

bool ReadLineBounded(std::istream& in, std::string& out, const size_t max_len, bool& overlong)
{
    out.clear();
    overlong = false;

    // Read through the streambuf rather than istream::get(char&).
    //
    // get() constructs a sentry per call -- once per BYTE -- and this runs over
    // every project part on every convergence, tens of megabytes of decompressed
    // text per pass. sbumpc() does the same work without the per-character
    // sentry.
    //
    // istream::getline(buf, n) would take the bound directly, but it sets
    // failbit on a zero-length extraction, i.e. on a blank line. Blank lines
    // occur in these files, so that would silently truncate the parse.
    std::istream::sentry sentry(in, true);   // true: do not skip leading whitespace
    if (!sentry) return false;

    std::streambuf* sb = in.rdbuf();

    for (;;) {
        const int c = sb->sbumpc();

        if (c == std::char_traits<char>::eof()) {
            in.setstate(std::ios_base::eofbit);

            // A final line without a trailing newline is still a line.
            return !out.empty();
        }

        if (c == '\n') return true;

        if (out.size() >= max_len) {
            overlong = true;
            return false;
        }

        out.push_back(static_cast<char>(c));
    }
}
