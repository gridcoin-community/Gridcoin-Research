#include "superblock.h"
#include "uint256.h"
#include "util.h"
#include "main.h"
#include "compat/endian.h"

std::string ExtractValue(std::string data, std::string delimiter, int pos);

namespace
{
    struct BinaryResearcher
    {
        std::array<unsigned char, 16> cpid;
        int16_t magnitude;
    };
    
    // Ensure that the compiler does not add padding between the cpid and the
    // magnitude. If it does it does it to align the data, at which point the
    // pointer cast in UnpackBinarySuperblock will be illegal. In such a
    // case we will have to resort to a slower unpack.
    static_assert(offsetof(struct BinaryResearcher, magnitude) ==
                  sizeof(struct BinaryResearcher) - sizeof(int16_t),
                  "Unexpected padding in BinaryResearcher");
}

std::string UnpackBinarySuperblock(std::string sBlock)
{
    // 12-21-2015: R HALFORD: If the block is not binary, return the legacy format for backward compatibility
    std::string sBinary = ExtractXML(sBlock,"<BINARY>","</BINARY>");
    if (sBinary.empty()) return sBlock;

    std::ostringstream stream;
    stream << "<AVERAGES>" << ExtractXML(sBlock,"<AVERAGES>","</AVERAGES>") << "</AVERAGES>"
           << "<QUOTES>" << ExtractXML(sBlock,"<QUOTES>","</QUOTES>") << "</QUOTES>"
           << "<MAGNITUDES>";

    // Binary data support structure:
    // Each CPID consumes 16 bytes and 2 bytes for magnitude: (Except CPIDs with zero magnitude - the count of those is stored in XML node <ZERO> to save space)
    // 1234567890123456MM
    // MM = Magnitude stored as 2 bytes
    // No delimiter between CPIDs, Step Rate = 18.
    // CPID and magnitude are stored in big endian.
    for (unsigned int x = 0; x < sBinary.length(); x += 18)
    {
        if(sBinary.length() - x < 18)
            break;

        const BinaryResearcher* researcher = reinterpret_cast<const BinaryResearcher*>(sBinary.data() + x);
        stream << HexStr(researcher->cpid.begin(), researcher->cpid.end()) << ","
               << be16toh(researcher->magnitude) << ";";
    }

    // Append zero magnitude researchers so the beacon count matches
    int num_zero_mag = atoi(ExtractXML(sBlock,"<ZERO>","</ZERO>"));
    const std::string zero_entry("0,15;");
    for(int i=0; i<num_zero_mag; ++i)
        stream << zero_entry;

    stream << "</MAGNITUDES>";
    return stream.str();
}

std::string PackBinarySuperblock(std::string sBlock)
{
    std::string sMagnitudes = ExtractXML(sBlock,"<MAGNITUDES>","</MAGNITUDES>");

    // For each CPID in the superblock, convert data to binary
    std::stringstream stream;
    int64_t num_zero_mag = 0;
    for (auto& entry : split(sMagnitudes.c_str(), ";"))
    {
        if (entry.length() < 1)
            continue;

        const std::vector<unsigned char>& binary_cpid = ParseHex(entry);
        if(binary_cpid.size() < 16)
        {
            ++num_zero_mag;
            continue;
        }

        BinaryResearcher researcher;
        std::copy_n(binary_cpid.begin(), researcher.cpid.size(), researcher.cpid.begin());

        // Ensure we do not blow out the binary space (technically we can handle 0-65535)
        double magnitude_d = strtod(ExtractValue(entry, ",", 1).c_str(), NULL);
        // Changed to 65535 for the new NN. This will still be able to be successfully unpacked by any node.
        magnitude_d = std::max(0.0, std::min(magnitude_d, 65535.0));
        researcher.magnitude = htobe16(roundint(magnitude_d));

        stream.write((const char*) &researcher, sizeof(BinaryResearcher));
    }

    std::stringstream block_stream;
    block_stream << "<ZERO>" << num_zero_mag << "</ZERO>"
                    "<BINARY>" << stream.rdbuf() << "</BINARY>"
                    "<AVERAGES>" << ExtractXML(sBlock,"<AVERAGES>","</AVERAGES>") << "</AVERAGES>"
                    "<QUOTES>" << ExtractXML(sBlock,"<QUOTES>","</QUOTES>") << "</QUOTES>";
    return block_stream.str();
}




void Superblock::PopulateReducedMaps()
{

    uint16_t iProject = 0;
    for (auto const& entry : mScraperSBStats)
    {
        if (entry.first.objecttype == statsobjecttype::byProject)
        {
            mProjectRef[entry.first.objectID] = iProject;
            mProjectStats[iProject] = std::make_pair((uint64_t) std::round(entry.second.statsvalue.dAvgRAC),
                                                             (uint64_t) std::round(entry.second.statsvalue.dRAC));
            ++iProject;
        }
    }


     // Find the single network wide NN entry and put in string.
    ScraperObjectStatsKey StatsKey;
    StatsKey.objecttype = statsobjecttype::NetworkWide;
    StatsKey.objectID = "";

    const auto& iter = mScraperSBStats.find(StatsKey);

    mProjectRef["Network"] = iProject;
    mProjectStats[iProject] = std::make_pair((uint64_t) iter->second.statsvalue.dAvgRAC,
                                         (uint64_t) iter->second.statsvalue.dRAC);

    nNetworkMagnitude = (uint64_t) std::round(iter->second.statsvalue.dMag);

    uint32_t iCPID = 0;
    nZeroMagCPIDs = 0;
    for (auto const& entry : mScraperSBStats)
    {
        if (entry.first.objecttype == statsobjecttype::byCPID)
        {
            // If the magnitude entry is zero suppress the CPID and increment the zero counter.
            if (std::round(entry.second.statsvalue.dMag) > 0)
            {
                mCPIDRef[NN::Cpid::Parse(entry.first.objectID)] = iCPID;
                mCPIDMagnitudes[iCPID] = (uint16_t) std::round(entry.second.statsvalue.dMag);

                ++iCPID;
            }
            else
            {
                nZeroMagCPIDs++;
            }
        }
    }

    for (auto const& entry : mScraperSBStats)
    {
        if (entry.first.objecttype == statsobjecttype::byCPIDbyProject)
        {

            std::vector<std::string> vObjectID = split(entry.first.objectID, ",");

            const auto& iterProject = mProjectRef.find(vObjectID[0]);
            const auto& iterCPID = mCPIDRef.find(NN::Cpid::Parse(vObjectID[1]));

            mProjectCPIDStats[iterProject->second][iterCPID->second] = std::make_tuple((uint64_t) std::round(entry.second.statsvalue.dTC),
                                                                                      (uint64_t) std::round(entry.second.statsvalue.dRAC),
                                                                                      (uint16_t) std::round(entry.second.statsvalue.dMag));
        }

    }

    fReducedMapsPopulated = true;
}

void Superblock::SerializeSuperblock(CDataStream& ss, int nType, int nVersion) const
{
    ss << mProjectRef;
    ss << mCPIDRef;

    ss << mProjectStats;
    ss << mCPIDMagnitudes;
    //ss << mProjectCPIDStats;

    ss << nNetworkMagnitude;
    ss << nZeroMagCPIDs;
    ss << nHeight;
    ss << nTime;
    ss << nSBVersion;
}

void Superblock::UnserializeSuperblock(CReaderStream& ss)
{
    ss >> mProjectRef;
    ss >> mCPIDRef;

    ss >> mProjectStats;
    ss >> mCPIDMagnitudes;
    //ss >> mProjectCPIDStats;

    ss >> nNetworkMagnitude;
    ss >> nZeroMagCPIDs;
    ss >> nHeight;
    ss >> nTime;
    ss >> nSBVersion;
}

void Superblock::SerializeSuperblock2(CDataStream& ss, int nType, int nVersion) const
{
    ss << mProjectRef;
    ss << mCPIDRef;

    ss << mProjectStats;
    ss << mCPIDMagnitudes;
    ss << mProjectCPIDStats;

    ss << nNetworkMagnitude;
    ss << nZeroMagCPIDs;
    ss << nHeight;
    ss << nTime;
    ss << nSBVersion;
}

void Superblock::UnserializeSuperblock2(CReaderStream& ss)
{
    ss >> mProjectRef;
    ss >> mCPIDRef;

    ss >> mProjectStats;
    ss >> mCPIDMagnitudes;
    ss >> mProjectCPIDStats;

    ss >> nNetworkMagnitude;
    ss >> nZeroMagCPIDs;
    ss >> nHeight;
    ss >> nTime;
    ss >> nSBVersion;
}
