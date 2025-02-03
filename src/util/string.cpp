// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2025 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/string.h>

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
