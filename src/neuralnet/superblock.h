#pragma once

#include "scraper/fwd.h"
#include <string>
#include "key.h"
#include "neuralnet/cpid.h"

std::string UnpackBinarySuperblock(std::string block);
std::string PackBinarySuperblock(std::string sBlock);

namespace NN {

// TODO: move this out into a common location...?
template<typename integer_t>
struct Compact
{
    Compact() : m_value(0)
    {
    }

    Compact(integer_t value) : m_value(value)
    {
    }

    integer_t m_value;

    // TODO: implement the other general-purpose operators...

    integer_t operator*() const
    {
        return m_value;
    }

    bool operator<(Compact<integer_t> other) const
    {
        return m_value < other.m_value;
    }

    bool operator<(integer_t other) const
    {
        return m_value < other;
    }

    void operator=(integer_t value)
    {
        m_value = value;
    }

    Compact<integer_t>& operator++()
    {
        m_value++;

        return *this;
    }

    Compact<integer_t> operator++(int)
    {
        Compact<integer_t> original = *this;
        ++*this;

        return original;
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        if (m_value < 253) {
            return sizeof(unsigned char);
        }

        if (m_value <= std::numeric_limits<unsigned short>::max()) {
            return sizeof(unsigned char) + sizeof(unsigned short);
        }

        if (m_value <= std::numeric_limits<unsigned int>::max()) {
            return sizeof(unsigned char) + sizeof(unsigned int);
        }

        return sizeof(unsigned char) + sizeof(uint64_t);
    }

    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        if (m_value < 253) {
            unsigned char chSize = m_value;
            WRITEDATA(s, chSize);
        } else if (m_value <= std::numeric_limits<unsigned short>::max()) {
            unsigned char chSize = 253;
            unsigned short xSize = m_value;
            WRITEDATA(s, chSize);
            WRITEDATA(s, xSize);
        } else if (m_value <= std::numeric_limits<unsigned int>::max()) {
            unsigned char chSize = 254;
            unsigned int xSize = m_value;
            WRITEDATA(s, chSize);
            WRITEDATA(s, xSize);
        } else {
            unsigned char chSize = 255;
            uint64_t xSize = m_value;
            WRITEDATA(s, chSize);
            WRITEDATA(s, xSize);
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        unsigned char chSize;

        // TODO: constrain max value to the size of the integer type in the
        // template parameter.
        uint64_t nSizeRet = 0;

        READDATA(s, chSize);

        if (chSize < 253) {
            nSizeRet = chSize;
        } else if (chSize == 253) {
            unsigned short xSize;
            READDATA(s, xSize);
            nSizeRet = xSize;

            if (nSizeRet < 253) {
                throw std::ios_base::failure("non-canonical Compact<T>::Unserialize()");
            }
        } else if (chSize == 254) {
            unsigned int xSize;
            READDATA(s, xSize);
            nSizeRet = xSize;

            if (nSizeRet < 0x10000u) {
                throw std::ios_base::failure("non-canonical Compact<T>::Unserialize()");
            }
        } else {
            uint64_t xSize;
            READDATA(s, xSize);
            nSizeRet = xSize;

            if (nSizeRet < 0x100000000LLu) {
                throw std::ios_base::failure("non-canonical Compact<T>::Unserialize()");
            }
        }

        m_value = nSizeRet;
    }
};

class Superblock
{
public:
    struct CpidStats
    {
        CpidStats();

        CpidStats(uint64_t total_credit, uint64_t rac, uint16_t magnitude);

        Compact<uint64_t> m_total_credit;
        Compact<uint64_t> m_rac;
        Compact<uint16_t> m_magnitude;

        IMPLEMENT_SERIALIZE
        (
            READWRITE(m_total_credit);
            READWRITE(m_rac);
            READWRITE(m_magnitude);
        )
    };

    struct ProjectStats
    {
        ProjectStats();

        ProjectStats(uint64_t average_rac, uint64_t rac);

        Compact<uint64_t> m_average_rac;
        Compact<uint64_t> m_rac;
        std::map<Compact<uint32_t>, CpidStats> m_cpids;

        IMPLEMENT_SERIALIZE
        (
            READWRITE(m_average_rac);
            READWRITE(m_rac);

            // Omit full project stats to compare old content size:
            // TODO: remove the condition?
            if (nVersion > 1) {
                READWRITE(m_cpids);
            }
        )
    };

    struct NetworkStats
    {
        Compact<uint64_t> m_average_rac;
        Compact<uint64_t> m_rac;
        Compact<uint64_t> m_magnitude;
        Compact<uint32_t> m_zero_mag_cpid_count;

        IMPLEMENT_SERIALIZE
        (
            READWRITE(m_average_rac);
            READWRITE(m_rac);
            READWRITE(m_magnitude);
            READWRITE(m_zero_mag_cpid_count);
        )
    };

    uint32_t m_version;
    std::map<NN::Cpid, Compact<uint16_t>> m_cpids;
    std::map<std::string, ProjectStats> m_projects;
    NetworkStats m_network;
    //std::vector<BeaconAcknowledgement> m_verified_beacons;

    int64_t nTime;
    int64_t nHeight;

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH)) {
            READWRITE(m_version);
        }

        nVersion = m_version;

        READWRITE(m_cpids);
        READWRITE(m_projects);
        READWRITE(m_network);
        //READWRITE(m_verified_beacons);
    )

public: /* public methods */

    void LoadStats(const ScraperStats& stats);

    /*

    Superblock();
    int64_t Age() const;

    static Superblock UnpackLegacy(const std::string& binary);
    std::string PackLegacy() const;

    QuorumHash ComputeQuorumHash() const;
    double ComputeAverageMagnitude() const;

    */
};
}
