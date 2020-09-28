// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "serialize.h"
#include "streams.h"
#include "hash.h"

namespace GRC {
class CAutoHasherFile : public CAutoFile, public CHashWriter
{
public:
    explicit CAutoHasherFile(FILE* filenew, int nTypeIn, int nVersionIn) :
        CAutoFile(filenew, nTypeIn, nVersionIn),
        CHashWriter(nTypeIn, nVersionIn) {};


    void read(char* pch, size_t nSize)
    {
        CAutoFile::read(pch, nSize);
        CHashWriter::write(pch, nSize);
    }

    void write(const char *pch, size_t nSize)
    {
        CAutoFile::write(pch, nSize);
        CHashWriter::write(pch, nSize);
    }

    int GetType() const { return CAutoFile::GetType(); }
    int GetVersion() const { return CAutoFile::GetVersion(); }

    template <typename T>
    CAutoHasherFile& operator<<(const T& obj)
    {
        ::Serialize(*this, obj);
        return *this;
    }

    template <typename T>
    CAutoHasherFile& operator>>(T& obj)
    {
        ::Unserialize(*this, obj);
        return *this;
    }
};
} // namespace GRC
