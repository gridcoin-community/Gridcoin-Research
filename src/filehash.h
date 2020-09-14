#ifndef FILEHASH_H
#define FILEHASH_H

#include "serialize.h"
#include "streams.h"
#include "hash.h"

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

#endif // FILEHASH_H
