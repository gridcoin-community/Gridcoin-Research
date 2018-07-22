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
        magnitude_d = std::max(0.0, std::min(magnitude_d, 32767.0));
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
