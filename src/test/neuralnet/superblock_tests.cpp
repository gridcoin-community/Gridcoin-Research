#include "compat/endian.h"
#include "neuralnet/superblock.h"
#include "streams.h"

#include <array>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <openssl/md5.h>
#include <vector>

#include "test/data/superblock.txt.h"
#include "test/data/superblock_packed.bin.h"
#include "test/data/superblock_unpacked.txt.h"

std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);
std::string ExtractValue(std::string data, std::string delimiter, int pos);

namespace {
//!
//! \brief Legacy functions used to test backward compatibility with the old
//! superblock contract format.
//!
//! We may eventually remove these functions, so we'll redefine them here for
//! the test cases that check legacy support.
//!
struct Legacy
{
    struct BinaryResearcher
    {
        std::array<unsigned char, 16> cpid;
        int16_t magnitude;
    };

    static std::string UnpackBinarySuperblock(std::string sBlock)
    {
        // 12-21-2015: R HALFORD: If the block is not binary, return the legacy format for backward compatibility
        std::string sBinary = ExtractXML(sBlock,"<BINARY>","</BINARY>");
        if (sBinary.empty()) return sBlock;

        std::ostringstream stream;
        stream << "<AVERAGES>" << ExtractXML(sBlock,"<AVERAGES>","</AVERAGES>") << "</AVERAGES>"
               << "<QUOTES>" << ExtractXML(sBlock,"<QUOTES>","</QUOTES>") << "</QUOTES>"
               << "<MAGNITUDES>";

        // Binary data support structure:
        // Each CPID consumes 16 bytes and 2 bytes for magnitude: (Except CPIDs
        // with zero magnitude - the count of those is stored in XML node
        // <ZERO> to save space)
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

    static std::string PackBinarySuperblock(std::string sBlock)
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

    static std::string RetrieveMd5(std::string s1)
    {
        try
        {
            const char* chIn = s1.c_str();
            unsigned char digest2[16];
            MD5((unsigned char*)chIn, strlen(chIn), (unsigned char*)&digest2);
            char mdString2[33];
            for(int i = 0; i < 16; i++) {
                sprintf(&mdString2[i*2], "%02x", (unsigned int)digest2[i]);
            }
            std::string xmd5(mdString2);
            return xmd5;
        }
        catch (std::exception &e)
        {
            LogPrintf("MD5 INVALID!");
            return "";
        }
    }

    static std::string CPIDHash(double dMagIn, std::string sCPID)
    {
        std::string sMag = RoundToString(dMagIn,0);
        double dMagLength = (double)sMag.length();
        double dExponent = pow(dMagLength,5);
        std::string sMagComponent1 = RoundToString(dMagIn/(dExponent+.01),0);
        std::string sSuffix = RoundToString(dMagLength * dExponent, 0);
        std::string sHash = sCPID + sMagComponent1 + sSuffix;
        return sHash;
    }

    static std::string GetQuorumHash(const std::string& data)
    {
        //Data includes the Magnitudes, and the Projects:
        std::string sMags = ExtractXML(data,"<MAGNITUDES>","</MAGNITUDES>");
        std::vector<std::string> vMags = split(sMags.c_str(),";");
        std::string sHashIn = "";

        for (unsigned int x = 0; x < vMags.size(); x++)
        {
            std::vector<std::string> vRow = split(vMags[x].c_str(),",");

            // Each row should consist of two fields, CPID and magnitude.
            if(vRow.size() < 2)
                continue;

            // First row (CPID) must be exactly 32 bytes.
            const std::string& sCPID = vRow[0];
            if(sCPID.size() != 32)
                continue;

            double dMag = RoundFromString(vRow[1],0);
            sHashIn += CPIDHash(dMag, sCPID) + "<COL>";
        }

        return RetrieveMd5(sHashIn);
    }
}; // Legacy

ScraperStats GetTestScraperStats()
{
    ScraperStats stats;
    std::string cpid1 = "00010203040506070809101112131415";
    std::string cpid2 = "15141312111009080706050403020100";
    std::string project1 = "project_1";
    std::string project2 = "project_2";

    ScraperObjectStats p1c1;
    p1c1.statskey.objecttype = statsobjecttype::byCPIDbyProject;
    p1c1.statskey.objectID = project1 + "," + cpid1;
    p1c1.statsvalue.dTC = 1000;
    p1c1.statsvalue.dRAC = 101;
    p1c1.statsvalue.dAvgRAC = 1003;
    p1c1.statsvalue.dMag = 1004;
    stats.emplace(p1c1.statskey, p1c1);

    ScraperObjectStats p1c2;
    p1c2.statskey.objecttype = statsobjecttype::byCPIDbyProject;
    p1c2.statskey.objectID = project1 + "," + cpid2;
    p1c2.statsvalue.dTC = 2000;
    p1c2.statsvalue.dRAC = 102;
    p1c2.statsvalue.dAvgRAC = 2003;
    p1c2.statsvalue.dMag = 2004;
    stats.emplace(p1c2.statskey, p1c2);

    ScraperObjectStats p2c1;
    p2c1.statskey.objecttype = statsobjecttype::byCPIDbyProject;
    p2c1.statskey.objectID = project2 + "," + cpid1;
    p2c1.statsvalue.dTC = 3000;
    p2c1.statsvalue.dRAC = 103;
    p2c1.statsvalue.dAvgRAC = 3003;
    p2c1.statsvalue.dMag = 3004;
    stats.emplace(p2c1.statskey, p2c1);

    ScraperObjectStats p2c2;
    p2c2.statskey.objecttype = statsobjecttype::byCPIDbyProject;
    p2c2.statskey.objectID = project2 + "," + cpid2;
    p2c2.statsvalue.dTC = 4000;
    p2c2.statsvalue.dRAC = 104;
    p2c2.statsvalue.dAvgRAC = 4003;
    p2c2.statsvalue.dMag = 4004;
    stats.emplace(p2c2.statskey, p2c2);

    ScraperObjectStats c1;
    c1.statskey.objecttype = statsobjecttype::byCPID;
    c1.statskey.objectID = cpid1;
    c1.statsvalue.dTC = p1c1.statsvalue.dTC + p2c1.statsvalue.dTC;
    c1.statsvalue.dRAC = p1c1.statsvalue.dRAC + p2c1.statsvalue.dRAC;
    c1.statsvalue.dAvgRAC = c1.statsvalue.dRAC;
    c1.statsvalue.dMag = p1c1.statsvalue.dMag + p2c1.statsvalue.dMag;
    stats.emplace(c1.statskey, c1);

    ScraperObjectStats c2;
    c2.statskey.objecttype = statsobjecttype::byCPID;
    c2.statskey.objectID = cpid2;
    c2.statsvalue.dTC = p1c2.statsvalue.dTC + p2c2.statsvalue.dTC;
    c2.statsvalue.dRAC = p1c2.statsvalue.dRAC + p2c2.statsvalue.dRAC;
    c2.statsvalue.dAvgRAC = c2.statsvalue.dRAC;
    c2.statsvalue.dMag = p1c2.statsvalue.dMag + p2c2.statsvalue.dMag;
    stats.emplace(c2.statskey, c2);

    ScraperObjectStats p1;
    p1.statskey.objecttype = statsobjecttype::byProject;
    p1.statskey.objectID = project1;
    p1.statsvalue.dTC = p1c1.statsvalue.dTC + p1c2.statsvalue.dTC;
    p1.statsvalue.dRAC = p1c1.statsvalue.dRAC + p1c2.statsvalue.dRAC;
    p1.statsvalue.dAvgRAC = p1.statsvalue.dRAC / 2;
    p1.statsvalue.dMag = p1c1.statsvalue.dMag + p1c2.statsvalue.dMag;
    stats.emplace(p1.statskey, p1);

    ScraperObjectStats p2;
    p2.statskey.objecttype = statsobjecttype::byProject;
    p2.statskey.objectID = project2;
    p2.statsvalue.dTC = p2c1.statsvalue.dTC + p2c2.statsvalue.dTC;
    p2.statsvalue.dRAC = p2c1.statsvalue.dRAC + p2c2.statsvalue.dRAC;
    p2.statsvalue.dAvgRAC = p2.statsvalue.dRAC / 2;
    p2.statsvalue.dMag = p2c1.statsvalue.dMag + p2c2.statsvalue.dMag;
    stats.emplace(p2.statskey, p2);

    return stats;
}

ConvergedScraperStats GetTestConvergence(const bool by_parts = false)
{
    ConvergedScraperStats convergence;

    convergence.mScraperConvergedStats = GetTestScraperStats();

    convergence.Convergence.bByParts = by_parts;
    convergence.Convergence.nContentHash
        = uint256("1111111111111111111111111111111111111111111111111111111111111111");
    convergence.Convergence.nUnderlyingManifestContentHash
        = uint256("2222222222222222222222222222222222222222222222222222222222222222");

    // Add some project parts with the same names as the projects in the stats.
    // The part data doesn't matter, so we just add empty containers.
    //
    convergence.Convergence.ConvergedManifestPartsMap.emplace("project_1", CSerializeData());
    convergence.Convergence.ConvergedManifestPartsMap.emplace("project_2", CSerializeData());

    return convergence;
}
} // anonymous namespace

// -----------------------------------------------------------------------------
// Legacy Superblock Test Cases
// -----------------------------------------------------------------------------

extern std::string GetQuorumHash(const std::string& data);

BOOST_AUTO_TEST_CASE(gridcoin_VerifyGetQuorumHash)
{
    const std::string contract =
        "<MAGNITUDES>"
            "0390450eff5f5cd6d7a7d95a6d898d8d,1480;"
            "1878ecb566ac8e62beb7d141e1922460,6.25;"
            "1963a6f109ea770c195a0e1afacd2eba,70;"
            "285ff8d5014ef73cc83580338a9c0345,820;"
            "46f64d69eb8c5ee9cd24178b589af83f,12.5;"
            "0,15;"
            "4f0fecd04be3a74c46aa9678f780d028,750;"
            "55cd02be28521073d367f7ca38615682,720;"
            "58e565221db80d168621187c36c26c3e,12.5;"
            "59900fe7ef44fe33aa2afdf98301ec1c,530;"
            "5a094d7d93f6d6370e78a2ac8c008407,1400;"
            "0,15;"
            "7d0d73fe026d66fd4ab8d5d8da32a611,84000;"
            "8cfe9864e18db32a334b7de997f5a4f2,35;"
            "8f2a530cf6f73647af4c680c3471ea65,90;"
            "96c18bb4a02d15c90224a7138a540cf7,4520;"
            "9b67756a05f76842de1e88226b79deb9,0;"
            "9ce6f19e20f69790601c9bf9c0b03928,3.75;"
            "9ff2b091f67327b7d8e5b75fb5337514,310;"
            "a7a537ff8ad4d8fff4b3dad444e681ef,0;"
            "a914eba952be5dfcf73d926b508fd5fa,6720;"
            "d5924e4750f0f1c1c97b9635914afb9e,0;"
            "db250f4451dc39632e52e157f034316d,5;"
            "e7f90818e3e87c0bbefe83ad3cfe27e1,13500;"
        "</MAGNITUDES>"
        "<QUOTES>btc,0;grc,0;</QUOTES>"
        "<AVERAGES>"
            "amicable numbers,536000,5900000;"
            "asteroids@home,158666.67,793333.33;"
            "citizen science grid,575333.33,2881428.57;"
            "collatz conjecture,4027142.86,36286380;"
            "cosmology@home,47000,282666.67;"
            "einstein@home,435333.33,5661428.57;"
            "gpugrid,1804285.71,9035714.29;"
            "leiden classical,2080,10500;"
            "lhc@home classic,26166.67,210000;"
            "milkyway@home,2094285.71,8395714.29;"
            "moowrap,996666.67,7981428.57;"
            "nfs@home,96000,385333.33;"
            "numberfields@home,89333.33,626666.67;"
            "primegrid,248000,1735714.29;"
            "seti@home,52333.33,367333.33;"
            "srbase,89666.67,896666.67;"
            "sztaki desktop grid,8320,41666.67;"
            "theskynet pogs,45500,409333.33;"
            "tn-grid,39500,514666.67;"
            "universe@home,47833.33,335333.33;"
            "vgtu project@home,20666.67,124000;"
            "world community grid,29166.67,263333.33;"
            "yafu,93000,838666.67;"
            "yoyo@home,7040,56333.33;"
            "NeuralNetwork,2000000,20000000;"
        "</AVERAGES>";

    BOOST_CHECK_EQUAL(GetQuorumHash(contract), "0f099cab261bb562ff553f3b9c7bf942");
}

BOOST_AUTO_TEST_CASE(gridcoin_QuorumHashShouldBeCorrectAfterPackingAndUnpackingBinarySuperblock)
{
    const std::string contract(superblock_txt, superblock_txt + superblock_txt_len);
    const std::string packed = PackBinarySuperblock(contract);
    const std::string unpacked = UnpackBinarySuperblock(packed);
    //BOOST_CHECK_EQUAL(unpacked, contract);
    BOOST_CHECK_EQUAL(GetQuorumHash(contract), GetQuorumHash(unpacked));
}

BOOST_AUTO_TEST_CASE(gridcoin_ValidatePackBinarySuperblock)
{
    const std::string contract(superblock_txt, superblock_txt + superblock_txt_len);
    const std::string expected(superblock_packed_bin, superblock_packed_bin + superblock_packed_bin_len);
    BOOST_CHECK_EQUAL(PackBinarySuperblock(contract), expected);
}

BOOST_AUTO_TEST_CASE(gridcoin_ValidateUnpackBinarySuperblock)
{
    const std::string packed(superblock_packed_bin, superblock_packed_bin + superblock_packed_bin_len);
    const std::string expected(superblock_unpacked_txt, superblock_unpacked_txt + superblock_unpacked_txt_len);
    BOOST_CHECK_EQUAL(UnpackBinarySuperblock(packed), expected);
}

// -----------------------------------------------------------------------------
// Superblock
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Superblock)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_superblock)
{
    NN::Superblock superblock;

    BOOST_CHECK(superblock.m_version == NN::Superblock::CURRENT_VERSION);
    BOOST_CHECK(superblock.m_convergence_hint == 0);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0);

    BOOST_CHECK(superblock.m_cpids.empty() == true);
    BOOST_CHECK(superblock.m_cpids.TotalMagnitude() == 0);
    BOOST_CHECK(superblock.m_cpids.AverageMagnitude() == 0);

    BOOST_CHECK(superblock.m_projects.empty() == true);
    BOOST_CHECK(superblock.m_projects.TotalRac() == 0);
    BOOST_CHECK(superblock.m_projects.AverageRac() == 0);

    BOOST_CHECK(superblock.m_height == 0);
    BOOST_CHECK(superblock.m_timestamp == 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_the_specified_version)
{
    NN::Superblock superblock(1);

    BOOST_CHECK(superblock.m_version == 1);
    BOOST_CHECK(superblock.m_convergence_hint == 0);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0);

    BOOST_CHECK(superblock.m_cpids.empty() == true);
    BOOST_CHECK(superblock.m_cpids.TotalMagnitude() == 0);
    BOOST_CHECK(superblock.m_cpids.AverageMagnitude() == 0);

    BOOST_CHECK(superblock.m_projects.empty() == true);
    BOOST_CHECK(superblock.m_projects.TotalRac() == 0);
    BOOST_CHECK(superblock.m_projects.AverageRac() == 0);

    BOOST_CHECK(superblock.m_height == 0);
    BOOST_CHECK(superblock.m_timestamp == 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_from_a_provided_set_of_scraper_statistics)
{
    NN::Superblock superblock = NN::Superblock::FromStats(GetTestScraperStats());

    BOOST_CHECK(superblock.m_version == NN::Superblock::CURRENT_VERSION);
    BOOST_CHECK(superblock.m_convergence_hint == 0);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0);

    auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == 2);
    BOOST_CHECK(cpids.TotalMagnitude() == 10016);
    BOOST_CHECK(cpids.AverageMagnitude() == 5008);
    BOOST_CHECK(cpids.At(0)->first.ToString() == "00010203040506070809101112131415");
    BOOST_CHECK(cpids.At(0)->second == 4008);
    BOOST_CHECK(cpids.At(1)->first.ToString() == "15141312111009080706050403020100");
    BOOST_CHECK(cpids.At(1)->second == 6008);

    auto& projects = superblock.m_projects;
    BOOST_CHECK(projects.size() == 2);
    BOOST_CHECK(projects.TotalRac() == 410);
    BOOST_CHECK(projects.AverageRac() == 205.0);

    if (const auto project_1 = projects.Try("project_1")) {
        BOOST_CHECK(project_1->m_total_credit == 3000);
        BOOST_CHECK(project_1->m_average_rac == 102);
        BOOST_CHECK(project_1->m_rac == 203);
        BOOST_CHECK(project_1->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 1 not found in superblock.");
    }

    if (const auto project_2 = projects.Try("project_2")) {
        BOOST_CHECK(project_2->m_total_credit == 7000);
        BOOST_CHECK(project_2->m_average_rac == 104);
        BOOST_CHECK(project_2->m_rac == 207);
        BOOST_CHECK(project_2->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 2 not found in superblock.");
    }
}

BOOST_AUTO_TEST_CASE(it_initializes_from_a_provided_scraper_convergnce)
{
    NN::Superblock superblock = NN::Superblock::FromConvergence(GetTestConvergence());

    BOOST_CHECK(superblock.m_version == NN::Superblock::CURRENT_VERSION);

    // This initialization mode must set the convergence hint derived from
    // the content hash of the convergence:
    BOOST_CHECK(superblock.m_convergence_hint == 0x11111111);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0x22222222);

    auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == 2);
    BOOST_CHECK(cpids.TotalMagnitude() == 10016);
    BOOST_CHECK(cpids.AverageMagnitude() == 5008);
    BOOST_CHECK(cpids.At(0)->first.ToString() == "00010203040506070809101112131415");
    BOOST_CHECK(cpids.At(0)->second == 4008);
    BOOST_CHECK(cpids.At(1)->first.ToString() == "15141312111009080706050403020100");
    BOOST_CHECK(cpids.At(1)->second == 6008);

    auto& projects = superblock.m_projects;
    BOOST_CHECK(projects.m_converged_by_project == false);
    BOOST_CHECK(projects.size() == 2);
    BOOST_CHECK(projects.TotalRac() == 410);
    BOOST_CHECK(projects.AverageRac() == 205.0);

    if (const auto project_1 = projects.Try("project_1")) {
        BOOST_CHECK(project_1->m_total_credit == 3000);
        BOOST_CHECK(project_1->m_average_rac == 102);
        BOOST_CHECK(project_1->m_rac == 203);
        BOOST_CHECK(project_1->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 1 not found in superblock.");
    }

    if (const auto project_2 = projects.Try("project_2")) {
        BOOST_CHECK(project_2->m_total_credit == 7000);
        BOOST_CHECK(project_2->m_average_rac == 104);
        BOOST_CHECK(project_2->m_rac == 207);
        BOOST_CHECK(project_2->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 2 not found in superblock.");
    }
}

BOOST_AUTO_TEST_CASE(it_initializes_from_a_fallback_by_project_scraper_convergnce)
{
    NN::Superblock superblock = NN::Superblock::FromConvergence(
        GetTestConvergence(true)); // Set fallback by project flag

    BOOST_CHECK(superblock.m_version == NN::Superblock::CURRENT_VERSION);
    BOOST_CHECK(superblock.m_convergence_hint == 0x11111111);
    // Manifest content hint not set for fallback convergence:
    BOOST_CHECK(superblock.m_manifest_content_hint == 0x00000000);

    auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == 2);
    BOOST_CHECK(cpids.TotalMagnitude() == 10016);
    BOOST_CHECK(cpids.AverageMagnitude() == 5008);
    BOOST_CHECK(cpids.At(0)->first.ToString() == "00010203040506070809101112131415");
    BOOST_CHECK(cpids.At(0)->second == 4008);
    BOOST_CHECK(cpids.At(1)->first.ToString() == "15141312111009080706050403020100");
    BOOST_CHECK(cpids.At(1)->second == 6008);

    auto& projects = superblock.m_projects;

    // By project flag must be true in a fallback-to-project convergence:
    BOOST_CHECK(projects.m_converged_by_project == true);
    BOOST_CHECK(projects.size() == 2);
    BOOST_CHECK(projects.TotalRac() == 410);
    BOOST_CHECK(projects.AverageRac() == 205.0);

    if (const auto project_1 = projects.Try("project_1")) {
        BOOST_CHECK(project_1->m_total_credit == 3000);
        BOOST_CHECK(project_1->m_average_rac == 102);
        BOOST_CHECK(project_1->m_rac == 203);

        // The convergence hint must be set in fallback-to-project convergence.
        // This is derived from the hash of an empty part data:
        BOOST_CHECK(project_1->m_convergence_hint == 0xd3591376);
    } else {
        BOOST_FAIL("Project 1 not found in superblock.");
    }

    if (const auto project_2 = projects.Try("project_2")) {
        BOOST_CHECK(project_2->m_total_credit == 7000);
        BOOST_CHECK(project_2->m_average_rac == 104);
        BOOST_CHECK(project_2->m_rac == 207);

        // The convergence hint must be set in fallback-to-project convergence.
        // This is derived from the hash of an empty part data:
        BOOST_CHECK(project_2->m_convergence_hint == 0xd3591376);
    } else {
        BOOST_FAIL("Project 2 not found in superblock.");
    }
}

BOOST_AUTO_TEST_CASE(it_initializes_by_unpacking_a_legacy_binary_contract)
{
    std::string cpid1 = "00000000000000000000000000000000";
    std::string cpid2 = "00010203040506070809101112131415";
    std::string cpid3 = "15141312111009080706050403020100";

    NN::Superblock superblock = NN::Superblock::UnpackLegacy(
        Legacy::PackBinarySuperblock(
            "<MAGNITUDES>"
                + cpid1 + ",0;"   // Include valid CPID with zero mag
                + cpid2 + ",100;" // Include valid CPID with non-zero mag
                + cpid3 + ",200;" // Include valid CPID with non-zero mag
                "15,0;"           // Add placeholder to zero-mag CPID count
                "15,0;"           // Add placeholder to zero-mag CPID count
                "invalid,123;"    // Add invalid CPID to zero-mag CPID count
                "invalid,;"       // Add missing mag to zero-mag CPID count
                ",123;"           // Add missing CPID to zero-mag CPID count
                ";"               // Drop empty record
            "</MAGNITUDES>"
            "<AVERAGES>"
                "project_1,123,456;"
                "project_2,234,567;"
                "NeuralNetwork,1000,2000;"
            "</AVERAGES>"
            "<QUOTES>btc,0;grc,0;</QUOTES>"
        )
    );

    // Legacy string-packed superblocks unpack to version 1:
    BOOST_CHECK(superblock.m_version == 1);
    BOOST_CHECK(superblock.m_convergence_hint == 0);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0);

    BOOST_CHECK(superblock.m_cpids.size() == 3);
    BOOST_CHECK(superblock.m_cpids.Zeros() == 5);
    BOOST_CHECK(superblock.m_cpids.At(0)->first.ToString() == cpid1);
    BOOST_CHECK(superblock.m_cpids.At(0)->second == 0);
    BOOST_CHECK(superblock.m_cpids.At(1)->first.ToString() == cpid2);
    BOOST_CHECK(superblock.m_cpids.At(1)->second == 100);
    BOOST_CHECK(superblock.m_cpids.At(2)->first.ToString() == cpid3);
    BOOST_CHECK(superblock.m_cpids.At(2)->second == 200);

    BOOST_CHECK(superblock.m_projects.m_converged_by_project == false);
    BOOST_CHECK(superblock.m_projects.size() == 2);
    BOOST_CHECK(superblock.m_projects.TotalRac() == 1023);
    BOOST_CHECK(superblock.m_projects.AverageRac() == 511.5);

    if (const auto project_1 = superblock.m_projects.Try("project_1")) {
        BOOST_CHECK(project_1->m_total_credit == 0);
        BOOST_CHECK(project_1->m_average_rac == 123);
        BOOST_CHECK(project_1->m_rac == 456);
        BOOST_CHECK(project_1->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 1 not found in superblock.");
    }

    if (const auto project_2 = superblock.m_projects.Try("project_2")) {
        BOOST_CHECK(project_2->m_total_credit == 0);
        BOOST_CHECK(project_2->m_average_rac == 234);
        BOOST_CHECK(project_2->m_rac == 567);
        BOOST_CHECK(project_2->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 2 not found in superblock.");
    }
}

BOOST_AUTO_TEST_CASE(it_initializes_by_unpacking_a_legacy_text_contract)
{
    std::string cpid1 = "00000000000000000000000000000000";
    std::string cpid2 = "00010203040506070809101112131415";
    std::string cpid3 = "15141312111009080706050403020100";

    NN::Superblock superblock = NN::Superblock::UnpackLegacy(
        "<MAGNITUDES>"
            + cpid1 + ",0;"   // Include valid CPID with zero mag
            + cpid2 + ",100;" // Include valid CPID with non-zero mag
            + cpid3 + ",200;" // Include valid CPID with non-zero mag
            "15,0;"           // Drop placeholder
            "15,0;"           // Drop placeholder
            "invalid,123;"    // Drop invalid CPID
            "invalid,;"       // Drop missing mag
            ",123;"           // Drop missing CPID
            ";"               // Drop empty record
        "</MAGNITUDES>"
        "<AVERAGES>"
            "project_1,123,456;"
            "project_2,234,567;"
            "NeuralNetwork,1000,2000;"
        "</AVERAGES>"
        "<QUOTES>btc,0;grc,0;</QUOTES>"
    );

    // Legacy string-packed superblocks unpack to version 1:
    BOOST_CHECK(superblock.m_version == 1);
    BOOST_CHECK(superblock.m_convergence_hint == 0);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0);

    BOOST_CHECK(superblock.m_cpids.size() == 3);
    BOOST_CHECK(superblock.m_cpids.Zeros() == 0);
    BOOST_CHECK(superblock.m_cpids.At(0)->first.ToString() == cpid1);
    BOOST_CHECK(superblock.m_cpids.At(0)->second == 0);
    BOOST_CHECK(superblock.m_cpids.At(1)->first.ToString() == cpid2);
    BOOST_CHECK(superblock.m_cpids.At(1)->second == 100);
    BOOST_CHECK(superblock.m_cpids.At(2)->first.ToString() == cpid3);
    BOOST_CHECK(superblock.m_cpids.At(2)->second == 200);

    BOOST_CHECK(superblock.m_projects.m_converged_by_project == false);
    BOOST_CHECK(superblock.m_projects.size() == 2);
    BOOST_CHECK(superblock.m_projects.TotalRac() == 1023);
    BOOST_CHECK(superblock.m_projects.AverageRac() == 511.5);

    if (const auto project_1 = superblock.m_projects.Try("project_1")) {
        BOOST_CHECK(project_1->m_total_credit == 0);
        BOOST_CHECK(project_1->m_average_rac == 123);
        BOOST_CHECK(project_1->m_rac == 456);
        BOOST_CHECK(project_1->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 1 not found in superblock.");
    }

    if (const auto project_2 = superblock.m_projects.Try("project_2")) {
        BOOST_CHECK(project_2->m_total_credit == 0);
        BOOST_CHECK(project_2->m_average_rac == 234);
        BOOST_CHECK(project_2->m_rac == 567);
        BOOST_CHECK(project_2->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 2 not found in superblock.");
    }
}

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_superblock_for_empty_strings)
{
    NN::Superblock superblock = NN::Superblock::UnpackLegacy("");

    BOOST_CHECK(superblock.m_version == 1);
    BOOST_CHECK(superblock.m_cpids.empty());
    BOOST_CHECK(superblock.m_projects.empty());
}

BOOST_AUTO_TEST_CASE(it_provides_backward_compatibility_for_legacy_contracts)
{
    const std::string legacy_contract(
        superblock_txt,
        superblock_txt + superblock_txt_len);

    const std::string legacy_packed = Legacy::PackBinarySuperblock(legacy_contract);
    const std::string legacy_unpacked = Legacy::UnpackBinarySuperblock(legacy_packed);
    const std::string expected_hash = Legacy::GetQuorumHash(legacy_contract);

    NN::Superblock superblock = NN::Superblock::UnpackLegacy(legacy_packed);

    BOOST_CHECK(superblock.m_version == 1);
    BOOST_CHECK(NN::QuorumHash::Hash(superblock).ToString() == expected_hash);

    // Check the first few CPIDs:
    auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == 1906);
    BOOST_CHECK(cpids.At(0)->first.ToString() == "002a9d6f3832d0b0028606d907e09d97");
    BOOST_CHECK(cpids.At(0)->second == 1);
    BOOST_CHECK(cpids.At(1)->first.ToString() == "002d383a19b63d698a3201793e7e3750");
    BOOST_CHECK(cpids.At(1)->second == 24);

    const std::string expected_packed(
        superblock_packed_bin,
        superblock_packed_bin + superblock_packed_bin_len);

    const std::string packed = superblock.PackLegacy();
    const std::string unpacked = Legacy::UnpackBinarySuperblock(packed);

    // Note: Superblock::PackLegacy is not guaranteed to pack a legacy contract
    // exactly as passed to Superblock::UnpackLegacy even though this test data
    // exists in a format that permits it to. The quorum hash must always match.
    //
    BOOST_CHECK(packed == expected_packed);
    BOOST_CHECK(unpacked == legacy_unpacked);
    BOOST_CHECK(Legacy::GetQuorumHash(unpacked) == expected_hash);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_it_represents_a_complete_superblock)
{
    NN::Superblock valid;

    valid.m_cpids.Add(NN::Cpid(), 123);
    valid.m_projects.Add("name", NN::Superblock::ProjectStats());

    BOOST_CHECK(valid.WellFormed() == true);

    NN::Superblock invalid = valid;

    invalid.m_version = 0;
    BOOST_CHECK(invalid.WellFormed() == false);

    invalid.m_version = std::numeric_limits<decltype(invalid.m_version)>::max();
    BOOST_CHECK(invalid.WellFormed() == false);

    invalid = valid;

    invalid.m_cpids = NN::Superblock::CpidIndex();
    BOOST_CHECK(invalid.WellFormed() == false);

    invalid = valid;

    invalid.m_projects = NN::Superblock::ProjectIndex();
    BOOST_CHECK(invalid.WellFormed() == false);
}

BOOST_AUTO_TEST_CASE(it_checks_whether_it_was_created_from_fallback_convergence)
{
    NN::Superblock superblock;

    BOOST_CHECK(superblock.ConvergedByProject() == false);

    superblock.m_projects.Add("project_name", NN::Superblock::ProjectStats());
    superblock.m_projects.SetHint("project_name", CSerializeData());

    BOOST_CHECK(superblock.ConvergedByProject() == true);
}

BOOST_AUTO_TEST_CASE(it_calculates_its_age)
{
    NN::Superblock superblock;

    superblock.m_timestamp = GetAdjustedTime() - 1;

    BOOST_CHECK(superblock.Age() > 0);
    BOOST_CHECK(superblock.Age() < GetAdjustedTime());
}

BOOST_AUTO_TEST_CASE(it_generates_its_quorum_hash)
{
    NN::Superblock superblock;

    BOOST_CHECK(superblock.GetHash() == NN::QuorumHash::Hash(superblock));
}

BOOST_AUTO_TEST_CASE(it_caches_its_quorum_hash)
{
    NN::Superblock superblock;

    // Cache the hash:
    NN::QuorumHash original_hash = superblock.GetHash();

    // Change the resulting hash:
    superblock.m_cpids.Add(NN::Cpid(), 123);

    // The cached hash should not change:
    BOOST_CHECK(superblock.GetHash() == original_hash);
}

BOOST_AUTO_TEST_CASE(it_regenerates_its_cached_quorum_hash)
{
    NN::Superblock superblock;

    // Cache the hash:
    superblock.GetHash();

    // Change the resulting hash:
    superblock.m_cpids.Add(NN::Cpid(), 123);

    // Regenrate the hash:
    BOOST_CHECK(superblock.GetHash(true) == NN::QuorumHash::Hash(superblock));
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    std::vector<unsigned char> expected {
        0x02, 0x00, 0x00, 0x00,                         // Version
        0x11, 0x11, 0x11, 0x11,                         // Convergence hint
        0x22, 0x22, 0x22, 0x22,                         // Manifest content hint
        0x02,                                           // CPIDs size
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // CPID 1
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, // ...
        0xfd, 0xa8, 0x0f,                               // Magnitude
        0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x09, 0x08, // CPID 2
        0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, // ...
        0xfd, 0x78, 0x17,                               // Magnitude
        0x00,                                           // Zero count (VARINT)
        0x00,                                           // By-project flag
        0x02,                                           // Projects size
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_1" key
        0x5f, 0x31,                                     // ...
        0x96, 0x38,                                     // Total credit (VARINT)
        0x66,                                           // Average RAC (VARINT)
        0x80, 0x4b,                                     // Total RAC (VARINT)
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_2" key
        0x5f, 0x32,                                     // ...
        0xb5, 0x58,                                     // Total credit (VARINT)
        0x68,                                           // Average RAC (VARINT)
        0x80, 0x4f,                                     // Total RAC (VARINT)
    };

    NN::Superblock superblock = NN::Superblock::FromConvergence(GetTestConvergence());

    BOOST_CHECK(GetSerializeSize(superblock, SER_NETWORK, 1) == expected.size());

    CDataStream stream(SER_NETWORK, 1);
    stream << superblock;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == expected);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    std::vector<unsigned char> bytes {
        0x02, 0x00, 0x00, 0x00,                         // Version
        0x11, 0x11, 0x11, 0x11,                         // Convergence hint
        0x22, 0x22, 0x22, 0x22,                         // Manifest content hint
        0x02,                                           // CPIDs size
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // CPID 1
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, // ...
        0xfd, 0xa8, 0x0f,                               // Magnitude
        0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x09, 0x08, // CPID 2
        0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, // ...
        0xfd, 0x78, 0x17,                               // Magnitude
        0x00,                                           // Zero count (VARINT)
        0x00,                                           // By-project flag
        0x02,                                           // Projects size
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_1" key
        0x5f, 0x31,                                     // ...
        0x96, 0x38,                                     // Total credit (VARINT)
        0x66,                                           // Average RAC (VARINT)
        0x80, 0x4b,                                     // Total RAC (VARINT)
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_2" key
        0x5f, 0x32,                                     // ...
        0xb5, 0x58,                                     // Total credit (VARINT)
        0x68,                                           // Average RAC (VARINT)
        0x80, 0x4f,                                     // Total RAC (VARINT)
    };

    NN::Superblock superblock;

    CDataStream stream(bytes, SER_NETWORK, 1);
    stream >> superblock;

    BOOST_CHECK(superblock.m_version == 2);
    BOOST_CHECK(superblock.m_convergence_hint == 0x11111111);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0x22222222);

    const auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == 2);
    BOOST_CHECK(cpids.Zeros() == 0);
    BOOST_CHECK(cpids.TotalMagnitude() == 10016);
    BOOST_CHECK(cpids.AverageMagnitude() == 5008.0);
    BOOST_CHECK(cpids.At(0)->first.ToString() == "00010203040506070809101112131415");
    BOOST_CHECK(cpids.At(0)->second == 4008);
    BOOST_CHECK(cpids.At(1)->first.ToString() == "15141312111009080706050403020100");
    BOOST_CHECK(cpids.At(1)->second == 6008);

    const auto& projects = superblock.m_projects;
    BOOST_CHECK(projects.m_converged_by_project == false);
    BOOST_CHECK(projects.size() == 2);
    BOOST_CHECK(projects.TotalRac() == 410);
    BOOST_CHECK(projects.AverageRac() == 205.0);

    if (const auto project1 = projects.Try("project_1")) {
        BOOST_CHECK(project1->m_total_credit == 3000);
        BOOST_CHECK(project1->m_average_rac == 102);
        BOOST_CHECK(project1->m_rac == 203);
        BOOST_CHECK(project1->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 1 not found in index.");
    }

    if (const auto project2 = projects.Try("project_2")) {
        BOOST_CHECK(project2->m_total_credit == 7000);
        BOOST_CHECK(project2->m_average_rac == 104);
        BOOST_CHECK(project2->m_rac == 207);
        BOOST_CHECK(project2->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 2 not found in index.");
    }
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_fallback_convergences)
{
    // Superblocks generated from fallback-by-project convergences include
    // convergence hints with a by-project flag set to 1:
    //
    std::vector<unsigned char> expected {
        0x02, 0x00, 0x00, 0x00,                         // Version
        0x11, 0x11, 0x11, 0x11,                         // Convergence hint
        0x00, 0x00, 0x00, 0x00,                         // Manifest content hint
        0x02,                                           // CPIDs size
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // CPID 1
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, // ...
        0xfd, 0xa8, 0x0f,                               // Magnitude
        0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x09, 0x08, // CPID 2
        0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, // ...
        0xfd, 0x78, 0x17,                               // Magnitude
        0x00,                                           // Zero count (VARINT)
        0x01,                                           // By-project flag
        0x02,                                           // Projects size
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_1" key
        0x5f, 0x31,                                     // ...
        0x96, 0x38,                                     // Total credit (VARINT)
        0x66,                                           // Average RAC (VARINT)
        0x80, 0x4b,                                     // Total RAC (VARINT)
        0x76, 0x13, 0x59, 0xd3,                         // Convergence hint
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_2" key
        0x5f, 0x32,                                     // ...
        0xb5, 0x58,                                     // Total credit (VARINT)
        0x68,                                           // Average RAC (VARINT)
        0x80, 0x4f,                                     // Total RAC (VARINT)
        0x76, 0x13, 0x59, 0xd3,                         // Convergence hint
    };

    NN::Superblock superblock = NN::Superblock::FromConvergence(
        GetTestConvergence(true)); // Set fallback by project flag

    BOOST_CHECK(GetSerializeSize(superblock, SER_NETWORK, 1) == expected.size());

    CDataStream stream(SER_NETWORK, 1);
    stream << superblock;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == expected);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_fallback_convergence)
{
    // Superblocks generated from fallback-by-project convergences include
    // convergence hints with a by-project flag set to 1:
    //
    std::vector<unsigned char> bytes {
        0x02, 0x00, 0x00, 0x00,                         // Version
        0x11, 0x11, 0x11, 0x11,                         // Convergence hint
        0x22, 0x22, 0x22, 0x22,                         // Manifest content hint
        0x02,                                           // CPIDs size
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // CPID 1
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, // ...
        0xfd, 0xa8, 0x0f,                               // Magnitude
        0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x09, 0x08, // CPID 2
        0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, // ...
        0xfd, 0x78, 0x17,                               // Magnitude
        0x00,                                           // Zero count (VARINT)
        0x01,                                           // By-project flag
        0x02,                                           // Projects size
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_1" key
        0x5f, 0x31,                                     // ...
        0x96, 0x38,                                     // Total credit (VARINT)
        0x66,                                           // Average RAC (VARINT)
        0x80, 0x4b,                                     // Total RAC (VARINT)
        0x76, 0x13, 0x59, 0xd3,                         // Convergence hint
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_2" key
        0x5f, 0x32,                                     // ...
        0xb5, 0x58,                                     // Total credit (VARINT)
        0x68,                                           // Average RAC (VARINT)
        0x80, 0x4f,                                     // Total RAC (VARINT)
        0x76, 0x13, 0x59, 0xd3,                         // Convergence hint
    };

    NN::Superblock superblock;

    CDataStream stream(bytes, SER_NETWORK, 1);
    stream >> superblock;

    BOOST_CHECK(superblock.m_version == 2);
    BOOST_CHECK(superblock.m_convergence_hint == 0x11111111);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0x22222222);

    const auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == 2);
    BOOST_CHECK(cpids.Zeros() == 0);
    BOOST_CHECK(cpids.TotalMagnitude() == 10016);
    BOOST_CHECK(cpids.AverageMagnitude() == 5008.0);
    BOOST_CHECK(cpids.At(0)->first.ToString() == "00010203040506070809101112131415");
    BOOST_CHECK(cpids.At(0)->second == 4008);
    BOOST_CHECK(cpids.At(1)->first.ToString() == "15141312111009080706050403020100");
    BOOST_CHECK(cpids.At(1)->second == 6008);

    const auto& projects = superblock.m_projects;
    BOOST_CHECK(projects.m_converged_by_project == true);
    BOOST_CHECK(projects.size() == 2);
    BOOST_CHECK(projects.TotalRac() == 410);
    BOOST_CHECK(projects.AverageRac() == 205.0);

    if (const auto project1 = projects.Try("project_1")) {
        BOOST_CHECK(project1->m_total_credit == 3000);
        BOOST_CHECK(project1->m_average_rac == 102);
        BOOST_CHECK(project1->m_rac == 203);
        BOOST_CHECK(project1->m_convergence_hint == 0xd3591376);
    } else {
        BOOST_FAIL("Project 1 not found in index.");
    }

    if (const auto project2 = projects.Try("project_2")) {
        BOOST_CHECK(project2->m_total_credit == 7000);
        BOOST_CHECK(project2->m_average_rac == 104);
        BOOST_CHECK(project2->m_rac == 207);
        BOOST_CHECK(project2->m_convergence_hint == 0xd3591376);
    } else {
        BOOST_FAIL("Project 2 not found in index.");
    }
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Superblock::CpidIndex
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Superblock__CpidIndex)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_index)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.size() == 0);
    BOOST_CHECK(cpids.Zeros() == 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_a_zero_magnitude_cpid_count)
{
    // Used to initialize the index for a legacy superblock:
    NN::Superblock::CpidIndex cpids(123);

    BOOST_CHECK(cpids.size() == 0);
    BOOST_CHECK(cpids.Zeros() == 123);
}

BOOST_AUTO_TEST_CASE(it_adds_a_cpid_magnitude_to_the_index)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.size() == 0);

    cpids.Add(NN::Cpid(), 123);

    BOOST_CHECK(cpids.size() == 1);
}

BOOST_AUTO_TEST_CASE(it_ignores_insertion_of_a_duplicate_cpid)
{
    NN::Superblock::CpidIndex cpids;
    NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");

    cpids.Add(cpid, 123);
    cpids.Add(cpid, 456);

    BOOST_CHECK(cpids.size() == 1);
}

BOOST_AUTO_TEST_CASE(it_fetches_a_cpid_pair_by_offset)
{
    NN::Superblock::CpidIndex cpids;

    NN::Cpid cpid1 = NN::Cpid::Parse("00010203040506070809101112131415");
    NN::Cpid cpid2 = NN::Cpid::Parse("15141312111009080706050403020100");

    cpids.Add(cpid1, 123);
    cpids.Add(cpid2, 456);

    BOOST_CHECK(cpids.At(0)->first == cpid1);
    BOOST_CHECK(cpids.At(0)->second == 123);
    BOOST_CHECK(cpids.At(1)->first == cpid2);
    BOOST_CHECK(cpids.At(1)->second == 456);
}

BOOST_AUTO_TEST_CASE(it_fetches_the_magnitude_of_a_specific_cpid)
{
    NN::Superblock::CpidIndex cpids;
    NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");

    cpids.Add(cpid, 123);

    BOOST_CHECK(cpids.MagnitudeOf(cpid) == 123);
}

BOOST_AUTO_TEST_CASE(it_assumes_zero_magnitude_for_a_nonexistent_cpid)
{
    NN::Superblock::CpidIndex cpids;
    NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");

    BOOST_CHECK(cpids.MagnitudeOf(cpid) == 0);
}

BOOST_AUTO_TEST_CASE(it_stores_cpids_in_lexicographical_order)
{
    // The order is important to ensure consistent ordering of CPID records in
    // superblocks for consensus and to access CPIDs by offset.

    NN::Superblock::CpidIndex cpids;

    NN::Cpid cpid1 = NN::Cpid::Parse("99999999999999999999999999999999");
    NN::Cpid cpid2 = NN::Cpid::Parse("ffffffffffffffffffffffffffffffff");
    NN::Cpid cpid3 = NN::Cpid::Parse("00000000000000000000000000000000");

    cpids.Add(cpid1, 123);
    cpids.Add(cpid2, 456);
    cpids.Add(cpid3, 789);

    BOOST_CHECK(cpids.At(0)->first == cpid3);
    BOOST_CHECK(cpids.At(0)->second == 789);
    BOOST_CHECK(cpids.At(1)->first == cpid1);
    BOOST_CHECK(cpids.At(1)->second == 123);
    BOOST_CHECK(cpids.At(2)->first == cpid2);
    BOOST_CHECK(cpids.At(2)->second == 456);
}

BOOST_AUTO_TEST_CASE(it_counts_the_number_of_active_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.size() == 0);

    cpids.Add(NN::Cpid(), 123);

    BOOST_CHECK(cpids.size() == 1);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_it_contains_any_active_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.empty() == true);

    cpids.Add(NN::Cpid(), 123);

    BOOST_CHECK(cpids.empty() == false);
}

BOOST_AUTO_TEST_CASE(it_tallies_the_number_of_zero_magnitude_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.Zeros() == 0);

    // Add only one zero-magnitude CPID:
    cpids.Add(NN::Cpid::Parse("00010203040506070809101112131415"), 0);
    cpids.Add(NN::Cpid::Parse("15141312111009080706050403020100"), 123);

    BOOST_CHECK(cpids.Zeros() == 1);
}

BOOST_AUTO_TEST_CASE(it_skips_tallying_zero_magnitudes_for_legacy_superblocks)
{
    // Legacy superblocks embed the number of zero-magnitude CPIDs in the
    // contract, so we won't count them upon insertion if explicitly set.

    NN::Superblock::CpidIndex cpids(123);

    BOOST_CHECK(cpids.Zeros() == 123);

    cpids.Add(NN::Cpid::Parse("00010203040506070809101112131415"), 0);

    BOOST_CHECK(cpids.Zeros() == 123);
}

BOOST_AUTO_TEST_CASE(it_sums_the_count_of_both_active_and_zero_magnitude_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.TotalCount() == 0);

    // Add only one zero-magnitude CPID:
    cpids.Add(NN::Cpid::Parse("00010203040506070809101112131415"), 0);
    cpids.Add(NN::Cpid::Parse("15141312111009080706050403020100"), 123);

    BOOST_CHECK(cpids.TotalCount() == 2);
}

BOOST_AUTO_TEST_CASE(it_tallies_the_sum_of_the_magnitudes_of_active_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.TotalMagnitude() == 0);

    cpids.Add(NN::Cpid(), 0);
    cpids.Add(NN::Cpid::Parse("00010203040506070809101112131415"), 123);
    cpids.Add(NN::Cpid::Parse("15141312111009080706050403020100"), 456);

    BOOST_CHECK(cpids.TotalMagnitude() == 579);
}

BOOST_AUTO_TEST_CASE(it_skips_tallying_the_magnitudes_of_duplicate_cpids)
{
    NN::Superblock::CpidIndex cpids;

    cpids.Add(NN::Cpid::Parse("00010203040506070809101112131415"), 123);

    BOOST_CHECK(cpids.TotalMagnitude() == 123);

    cpids.Add(NN::Cpid::Parse("00010203040506070809101112131415"), 456);

    BOOST_CHECK(cpids.TotalMagnitude() == 123);
}

BOOST_AUTO_TEST_CASE(it_calculates_the_average_magnitude_of_active_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.AverageMagnitude() == 0);

    cpids.Add(NN::Cpid(), 0);
    cpids.Add(NN::Cpid::Parse("00010203040506070809101112131415"), 123);
    cpids.Add(NN::Cpid::Parse("15141312111009080706050403020100"), 456);

    BOOST_CHECK(cpids.AverageMagnitude() == 289.5);
}

BOOST_AUTO_TEST_CASE(it_is_iterable)
{
    NN::Superblock::CpidIndex cpids;

    const NN::Cpid cpid1 = NN::Cpid::Parse("00010203040506070809101112131415");
    const NN::Cpid cpid2 = NN::Cpid::Parse("15141312111009080706050403020100");

    cpids.Add(cpid1, 123);
    cpids.Add(cpid2, 123);

    size_t counter = 0;

    for (auto const& cpid : cpids) {
        BOOST_CHECK(cpid.first == cpid1 || cpid.first == cpid2);
        counter++;
    }

    BOOST_CHECK(counter == 2);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const std::vector<unsigned char> expected {
        0x01,                                            // Active CPID size
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  // CPID
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,  // ...
        0x7b,                                            // Magnitude (123)
        0x01,                                            // Zero count (VARINT)
    };

    NN::Superblock::CpidIndex cpids;

    cpids.Add(NN::Cpid::Parse("00010203040506070809101112131415"), 123);
    cpids.Add(NN::Cpid(), 0);

    BOOST_CHECK(GetSerializeSize(cpids, SER_NETWORK, 1) == expected.size());

    CDataStream stream(SER_NETWORK, 1);
    stream << cpids;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == expected);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    const std::vector<unsigned char> bytes {
        0x01,                                            // Active CPID size
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  // CPID
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,  // ...
        0x7b,                                            // Magnitude (123)
        0x01,                                            // Zero count (VARINT)
    };

    NN::Superblock::CpidIndex cpids;

    CDataStream stream(bytes, SER_NETWORK, 1);
    stream >> cpids;

    const NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");

    BOOST_CHECK(cpids.size() == 1);
    BOOST_CHECK(cpids.Zeros() == 1);
    BOOST_CHECK(cpids.At(0)->first == cpid);
    BOOST_CHECK(cpids.MagnitudeOf(cpid) == 123);
    BOOST_CHECK(cpids.TotalMagnitude() == 123);
    BOOST_CHECK(cpids.AverageMagnitude() == 123);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Superblock::ProjectStats
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Superblock__ProjectStats)

BOOST_AUTO_TEST_CASE(it_initializes_to_a_zero_statistics_object)
{
    NN::Superblock::ProjectStats stats;

    BOOST_CHECK(stats.m_total_credit == 0);
    BOOST_CHECK(stats.m_average_rac == 0);
    BOOST_CHECK(stats.m_rac == 0);
    BOOST_CHECK(stats.m_convergence_hint == 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_the_supplied_statistics)
{
    NN::Superblock::ProjectStats stats(123, 456, 789);

    BOOST_CHECK(stats.m_total_credit == 123);
    BOOST_CHECK(stats.m_average_rac == 456);
    BOOST_CHECK(stats.m_rac == 789);
    BOOST_CHECK(stats.m_convergence_hint == 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_supplied_legacy_superblock_statistics)
{
    NN::Superblock::ProjectStats stats(123, 456);

    BOOST_CHECK(stats.m_total_credit == 0);
    BOOST_CHECK(stats.m_average_rac == 123);
    BOOST_CHECK(stats.m_rac == 456);
    BOOST_CHECK(stats.m_convergence_hint == 0);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const std::vector<unsigned char> expected {
        0x01, // Total credit (VARINT)
        0x02, // Average RAC (VARINT)
        0x03, // Total RAC (VARINT)
    };

    NN::Superblock::ProjectStats project(1, 2, 3);

    BOOST_CHECK(GetSerializeSize(project, SER_NETWORK, 1) == expected.size());

    CDataStream stream(SER_NETWORK, 1);
    stream << project;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == expected);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    const std::vector<unsigned char> bytes {
        0x01, // Total credit (VARINT)
        0x02, // Average RAC (VARINT)
        0x03, // Total RAC (VARINT)
    };

    NN::Superblock::ProjectStats project;

    CDataStream stream(bytes, SER_NETWORK, 1);
    stream >> project;

    BOOST_CHECK(project.m_total_credit == 1);
    BOOST_CHECK(project.m_average_rac == 2);
    BOOST_CHECK(project.m_rac == 3);
    BOOST_CHECK(project.m_convergence_hint == 0);
}

BOOST_AUTO_TEST_SUITE_END()

// -----------------------------------------------------------------------------
// Superblock::ProjectIndex
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Superblock__ProjectIndex)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_index)
{
    NN::Superblock::ProjectIndex projects;

    BOOST_CHECK(projects.m_converged_by_project == false);
    BOOST_CHECK(projects.size() == 0);
}

BOOST_AUTO_TEST_CASE(it_adds_a_project_statistics_entry_to_the_index)
{
    NN::Superblock::ProjectIndex projects;

    BOOST_CHECK(projects.size() == 0);

    projects.Add("project_name", NN::Superblock::ProjectStats());

    BOOST_CHECK(projects.size() == 1);
}

BOOST_AUTO_TEST_CASE(it_ignores_insertion_of_a_duplicate_project)
{
    NN::Superblock::ProjectIndex projects;

    projects.Add("project_1", NN::Superblock::ProjectStats(123, 123));
    projects.Add("project_1", NN::Superblock::ProjectStats(456, 456));

    BOOST_CHECK(projects.size() == 1);
}

BOOST_AUTO_TEST_CASE(it_ignores_insertion_of_a_project_with_an_empty_name)
{
    NN::Superblock::ProjectIndex projects;

    projects.Add("", NN::Superblock::ProjectStats(123, 123));

    BOOST_CHECK(projects.size() == 0);
}

BOOST_AUTO_TEST_CASE(it_fetches_the_statistics_of_a_specific_project)
{
    NN::Superblock::ProjectIndex projects;

    projects.Add("project_name", NN::Superblock::ProjectStats(123, 456, 789));

    if (const auto project = projects.Try("project_name")) {
        BOOST_CHECK(project->m_total_credit == 123);
        BOOST_CHECK(project->m_average_rac == 456);
        BOOST_CHECK(project->m_rac == 789);
        BOOST_CHECK(project->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project not found in index.");
    }
}

BOOST_AUTO_TEST_CASE(it_sets_a_project_part_convergence_hint)
{
    NN::Superblock::ProjectIndex projects;

    projects.Add("project_name", NN::Superblock::ProjectStats());
    projects.SetHint("project_name", CSerializeData());

    BOOST_CHECK(projects.m_converged_by_project == true);

    if (const auto project = projects.Try("project_name")) {
        // Hint derived from the hash of an empty part data:
        BOOST_CHECK(project->m_convergence_hint == 0xd3591376);
    } else {
        BOOST_FAIL("Project not found in index.");
    }
}

BOOST_AUTO_TEST_CASE(it_stores_projects_in_lexicographical_order_by_name)
{
    // The order is important to ensure consistent ordering of project records
    // in superblocks for consensus.

    NN::Superblock::ProjectIndex projects;

    projects.Add("project_3", NN::Superblock::ProjectStats(123, 123));
    projects.Add("project_1", NN::Superblock::ProjectStats(456, 456));
    projects.Add("project_2", NN::Superblock::ProjectStats(789, 789));

    // The project index doesn't provide an offset accessor yet, so we'll
    // check the order by looping:
    size_t offset = 0;
    for (const auto& project_pair : projects) {
        switch (offset) {
            case 0:
                BOOST_CHECK(project_pair.first == "project_1");
                BOOST_CHECK(project_pair.second.m_rac == 456);
                break;
            case 1:
                BOOST_CHECK(project_pair.first == "project_2");
                BOOST_CHECK(project_pair.second.m_rac == 789);
                break;
            case 2:
                BOOST_CHECK(project_pair.first == "project_3");
                BOOST_CHECK(project_pair.second.m_rac == 123);
                break;
            default:
                BOOST_FAIL("Unexpected project at offset.");
                break;
        }

        offset++;
    }

    BOOST_CHECK(offset == projects.size());
}

BOOST_AUTO_TEST_CASE(it_counts_the_number_of_projects)
{
    NN::Superblock::ProjectIndex projects;

    BOOST_CHECK(projects.size() == 0);

    projects.Add("project_name", NN::Superblock::ProjectStats());

    BOOST_CHECK(projects.size() == 1);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_it_contains_any_projects)
{
    NN::Superblock::ProjectIndex projects;

    BOOST_CHECK(projects.empty() == true);

    projects.Add("project_name", NN::Superblock::ProjectStats());

    BOOST_CHECK(projects.empty() == false);
}

BOOST_AUTO_TEST_CASE(it_tallies_the_sum_of_the_rac_for_all_projects)
{
    NN::Superblock::ProjectIndex projects;

    BOOST_CHECK(projects.TotalRac() == 0);

    projects.Add("project_1", NN::Superblock::ProjectStats(123, 123));
    projects.Add("project_2", NN::Superblock::ProjectStats(456, 456));
    projects.Add("project_3", NN::Superblock::ProjectStats(789, 789));

    BOOST_CHECK(projects.TotalRac() == 1368);
}

BOOST_AUTO_TEST_CASE(it_skips_tallying_the_rac_of_duplicate_projects)
{
    NN::Superblock::ProjectIndex projects;

    projects.Add("project_1", NN::Superblock::ProjectStats(123, 123));

    BOOST_CHECK(projects.TotalRac() == 123);

    projects.Add("project_1", NN::Superblock::ProjectStats(456, 456));

    BOOST_CHECK(projects.TotalRac() == 123);
}

BOOST_AUTO_TEST_CASE(it_skips_tallying_the_rac_of_projects_with_empty_names)
{
    NN::Superblock::ProjectIndex projects;

    projects.Add("", NN::Superblock::ProjectStats(123, 123));

    BOOST_CHECK(projects.TotalRac() == 0);
}

BOOST_AUTO_TEST_CASE(it_calculates_the_average_rac_of_all_projects)
{
    NN::Superblock::ProjectIndex projects;

    BOOST_CHECK(projects.AverageRac() == 0);

    projects.Add("project_1", NN::Superblock::ProjectStats(123, 123));
    projects.Add("project_2", NN::Superblock::ProjectStats(456, 456));
    projects.Add("project_3", NN::Superblock::ProjectStats(789, 789));

    BOOST_CHECK(projects.AverageRac() == 456.0);
}

BOOST_AUTO_TEST_CASE(it_is_iterable)
{
    NN::Superblock::ProjectIndex projects;

    projects.Add("project_1", NN::Superblock::ProjectStats());
    projects.Add("project_2", NN::Superblock::ProjectStats());

    size_t counter = 0;

    for (auto const& project : projects) {
        BOOST_CHECK(boost::starts_with(project.first, "project_") == true);
        counter++;
    }

    BOOST_CHECK(counter == 2);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const std::vector<unsigned char> expected {
        0x00,                                           // By-project flag
        0x02,                                           // Projects size
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_1" key
        0x5f, 0x31,                                     // ...
        0x01,                                           // Total credit (VARINT)
        0x02,                                           // Average RAC (VARINT)
        0x03,                                           // Total RAC (VARINT)
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_2" key
        0x5f, 0x32,                                     // ...
        0x01,                                           // Total credit (VARINT)
        0x02,                                           // Average RAC (VARINT)
        0x03,                                           // Total RAC (VARINT)
    };

    NN::Superblock::ProjectIndex projects;

    projects.Add("project_1", NN::Superblock::ProjectStats(1, 2, 3));
    projects.Add("project_2", NN::Superblock::ProjectStats(1, 2, 3));

    BOOST_CHECK(GetSerializeSize(projects, SER_NETWORK, 1) == expected.size());

    CDataStream stream(SER_NETWORK, 1);
    stream << projects;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == expected);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    const std::vector<unsigned char> bytes {
        0x00,                                           // By-project flag
        0x02,                                           // Projects size
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_1" key
        0x5f, 0x31,                                     // ...
        0x01,                                           // Total credit (VARINT)
        0x02,                                           // Average RAC (VARINT)
        0x03,                                           // Total RAC (VARINT)
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_2" key
        0x5f, 0x32,                                     // ...
        0x01,                                           // Total credit (VARINT)
        0x02,                                           // Average RAC (VARINT)
        0x03,                                           // Total RAC (VARINT)
    };

    NN::Superblock::ProjectIndex projects;

    CDataStream stream(bytes, SER_NETWORK, 1);
    stream >> projects;

    BOOST_CHECK(projects.m_converged_by_project == false);
    BOOST_CHECK(projects.size() == 2);

    if (const auto project1 = projects.Try("project_1")) {
        BOOST_CHECK(project1->m_total_credit == 1);
        BOOST_CHECK(project1->m_average_rac == 2);
        BOOST_CHECK(project1->m_rac == 3);
        BOOST_CHECK(project1->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 1 not found in index.");
    }

    if (const auto project2 = projects.Try("project_2")) {
        BOOST_CHECK(project2->m_total_credit == 1);
        BOOST_CHECK(project2->m_average_rac == 2);
        BOOST_CHECK(project2->m_rac == 3);
        BOOST_CHECK(project2->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 2 not found in index.");
    }

    BOOST_CHECK(projects.TotalRac() == 6);
    BOOST_CHECK(projects.AverageRac() == 3);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_fallback_convergences)
{
    // A project index generated from fallback-by-project convergences includes
    // convergence hints with a by-project flag set to 1:
    //
    const std::vector<unsigned char> expected {
        0x01,                                           // By-project flag
        0x02,                                           // Projects size
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_1" key
        0x5f, 0x31,                                     // ...
        0x01,                                           // Total credit (VARINT)
        0x02,                                           // Average RAC (VARINT)
        0x03,                                           // Total RAC (VARINT)
        0x76, 0x13, 0x59, 0xd3,                         // Convergence hint
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_2" key
        0x5f, 0x32,                                     // ...
        0x01,                                           // Total credit (VARINT)
        0x02,                                           // Average RAC (VARINT)
        0x03,                                           // Total RAC (VARINT)
        0x76, 0x13, 0x59, 0xd3,                         // Convergence hint
    };

    NN::Superblock::ProjectIndex projects;

    projects.Add("project_1", NN::Superblock::ProjectStats(1, 2, 3));
    projects.Add("project_2", NN::Superblock::ProjectStats(1, 2, 3));

    projects.SetHint("project_1", CSerializeData());
    projects.SetHint("project_2", CSerializeData());

    BOOST_CHECK(GetSerializeSize(projects, SER_NETWORK, 1) == expected.size());

    CDataStream stream(SER_NETWORK, 1);
    stream << projects;
    std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output == expected);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_fallback_convergence)
{
    // A project index generated from fallback-by-project convergences includes
    // convergence hints with a by-project flag set to 1:
    //
    const std::vector<unsigned char> bytes {
        0x01,                                           // By-project flag
        0x02,                                           // Projects size
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_1" key
        0x5f, 0x31,                                     // ...
        0x01,                                           // Total credit (VARINT)
        0x02,                                           // Average RAC (VARINT)
        0x03,                                           // Total RAC (VARINT)
        0x76, 0x13, 0x59, 0xd3,                         // Convergence hint
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, // "project_2" key
        0x5f, 0x32,                                     // ...
        0x01,                                           // Total credit (VARINT)
        0x02,                                           // Average RAC (VARINT)
        0x03,                                           // Total RAC (VARINT)
        0x76, 0x13, 0x59, 0xd3,                         // Convergence hint
    };

    NN::Superblock::ProjectIndex projects;

    CDataStream stream(bytes, SER_NETWORK, 1);
    stream >> projects;

    BOOST_CHECK(projects.m_converged_by_project == true);
    BOOST_CHECK(projects.size() == 2);

    if (const auto project1 = projects.Try("project_1")) {
        BOOST_CHECK(project1->m_total_credit == 1);
        BOOST_CHECK(project1->m_average_rac == 2);
        BOOST_CHECK(project1->m_rac == 3);
        BOOST_CHECK(project1->m_convergence_hint == 0xd3591376);
    } else {
        BOOST_FAIL("Project 1 not found in index.");
    }

    if (const auto project2 = projects.Try("project_2")) {
        BOOST_CHECK(project2->m_total_credit == 1);
        BOOST_CHECK(project2->m_average_rac == 2);
        BOOST_CHECK(project2->m_rac == 3);
        BOOST_CHECK(project2->m_convergence_hint == 0xd3591376);
    } else {
        BOOST_FAIL("Project 2 not found in index.");
    }

    BOOST_CHECK(projects.TotalRac() == 6);
    BOOST_CHECK(projects.AverageRac() == 3);
}

BOOST_AUTO_TEST_SUITE_END();

// -----------------------------------------------------------------------------
// QuorumHash
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(QuorumHash)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_invalid_hash)
{
    NN::QuorumHash hash;

    BOOST_CHECK(hash.Valid() == false);
    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::INVALID);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_a_sha256_hash)
{
    NN::QuorumHash hash(uint256(0));

    BOOST_CHECK(hash.Valid() == true);
    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::SHA256);
}

BOOST_AUTO_TEST_CASE(it_initializes_with_a_legacy_md5_hash)
{
    NN::QuorumHash hash(std::array<unsigned char, 16> { });

    BOOST_CHECK(hash.Valid() == true);
    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::MD5);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_the_supplied_bytes)
{
    NN::QuorumHash hash_invalid(std::vector<unsigned char> { 0x00 });

    BOOST_CHECK(hash_invalid.Valid() == false);
    BOOST_CHECK(hash_invalid.Which() == NN::QuorumHash::Kind::INVALID);

    const std::vector<unsigned char> sha256_bytes {
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    NN::QuorumHash hash_sha256(sha256_bytes);

    BOOST_CHECK(hash_sha256.Valid() == true);
    BOOST_CHECK(hash_sha256.Which() == NN::QuorumHash::Kind::SHA256);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        sha256_bytes.begin(),
        sha256_bytes.end(),
        hash_sha256.Raw(),
        hash_sha256.Raw() + 32);

    const std::vector<unsigned char> md5_bytes {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    NN::QuorumHash hash_md5(md5_bytes);

    BOOST_CHECK(hash_md5.Valid() == true);
    BOOST_CHECK(hash_md5.Which() == NN::QuorumHash::Kind::MD5);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        md5_bytes.begin(),
        md5_bytes.end(),
        hash_md5.Raw(),
        hash_md5.Raw() + 16);
}

BOOST_AUTO_TEST_CASE(it_hashes_a_superblock)
{
    NN::Superblock superblock;

    auto& cpids = superblock.m_cpids;
    cpids.Add(NN::Cpid::Parse("00010203040506070809101112131415"), 1);
    cpids.Add(NN::Cpid::Parse("15141312111009080706050403020100"), 1);

    auto& projects = superblock.m_projects;
    projects.Add("project_1", NN::Superblock::ProjectStats(0, 0, 0));
    projects.Add("project_2", NN::Superblock::ProjectStats(0, 0, 0));

    // Note: convergence hints embedded in a superblock are NOT considered
    // when generating the superblock hash, and the container sizes aren't
    // either:
    //
    std::vector<unsigned char> input {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  // CPID 1
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,  // ...
        0x01,                                            // Magnitude
        0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x09, 0x08,  // CPID 2
        0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,  // ...
        0x01,                                            // Magnitude
        0x00,                                            // Zero-mag count
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74,  // "project_1" key
        0x5f, 0x31,                                      // ...
        0x00,                                            // Total credit
        0x00,                                            // Average RAC
        0x00,                                            // Total RAC
        0x09, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74,  // "project_2" key
        0x5f, 0x32,                                      // ...
        0x00,                                            // Total credit
        0x00,                                            // Average RAC
        0x00,                                            // Total RAC
    };

    uint256 expected = Hash(input.begin(), input.end());
    NN::QuorumHash hash = NN::QuorumHash::Hash(superblock);

    BOOST_CHECK(hash.Valid() == true);
    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::SHA256);
    BOOST_CHECK(hash == expected);
    BOOST_CHECK(hash.ToString() == expected.ToString());
}

BOOST_AUTO_TEST_CASE(it_parses_a_sha256_hash_string)
{
    const std::vector<unsigned char> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    NN::QuorumHash hash = NN::QuorumHash::Parse(HexStr(expected));

    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::SHA256);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        expected.begin(),
        expected.end(),
        hash.Raw(),
        hash.Raw() + 32);
}

BOOST_AUTO_TEST_CASE(it_parses_a_legacy_md5_hash_string)
{
    const std::vector<unsigned char> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    NN::QuorumHash hash = NN::QuorumHash::Parse(HexStr(expected));

    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::MD5);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        expected.begin(),
        expected.end(),
        hash.Raw(),
        hash.Raw() + 16);
}

BOOST_AUTO_TEST_CASE(it_parses_an_invalid_quorum_hash_to_an_invalid_variant)
{
    // Empty:
    NN::QuorumHash hash = NN::QuorumHash::Parse("");
    BOOST_CHECK(hash.Valid() == false);

    // Too short MD5: 31 characters
    hash = NN::QuorumHash::Parse("0001020304050607080910111213141");
    BOOST_CHECK(hash.Valid() == false);

    // Too long MD5: 33 characters
    hash = NN::QuorumHash::Parse("000102030405060708091011121314155");
    BOOST_CHECK(hash.Valid() == false);

    // Non-hex character at the end:
    hash = NN::QuorumHash::Parse("0001020304050607080910111213141Z");
    BOOST_CHECK(hash.Valid() == false);

    // Too short SHA256: 63 characters
    hash = NN::QuorumHash::Parse(
        "000102030405060708091011121314150001020304050607080910111213141");
    BOOST_CHECK(hash.Valid() == false);

    // Too long SHA256: 65 characters
    hash = NN::QuorumHash::Parse(
        "00010203040506070809101112131415000102030405060708091011121314155");
    BOOST_CHECK(hash.Valid() == false);

    // Non-hex character at the end:
    hash = NN::QuorumHash::Parse(
        "000102030405060708091011121314150001020304050607080910111213141Z");
    BOOST_CHECK(hash.Valid() == false);
}

BOOST_AUTO_TEST_CASE(it_hashes_cpid_magnitudes_from_a_legacy_superblock)
{
    // Version 1 superblocks hash with the legacy MD5-based algorithm:
    NN::Superblock superblock(1);

    std::string cpid1 = "00010203040506070809101112131415";
    std::string cpid2 = "15141312111009080706050403020100";

    superblock.m_cpids.Add(NN::Cpid::Parse(cpid1), 100);
    superblock.m_cpids.Add(NN::Cpid::Parse(cpid2), 200);

    NN::QuorumHash hash = NN::QuorumHash::Hash(superblock);

    BOOST_CHECK(hash.Valid() == true);
    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::MD5);
    BOOST_CHECK(hash.ToString() == "939715690b64b53edb2a79755eca4ae1");

    BOOST_CHECK(hash.ToString() == Legacy::GetQuorumHash(
        "<MAGNITUDES>"
            + cpid1 + ",100;"
            + cpid2 + ",200;"
        "</MAGNITUDES>"
    ));
}

BOOST_AUTO_TEST_CASE(it_compares_another_quorum_hash_for_equality)
{
    NN::Superblock superblock;
    NN::Superblock legacy_superblock(1);

    NN::QuorumHash invalid;
    NN::QuorumHash sha256 = NN::QuorumHash::Hash(superblock);
    NN::QuorumHash md5 = NN::QuorumHash::Hash(legacy_superblock);

    BOOST_CHECK(invalid == NN::QuorumHash());
    BOOST_CHECK(sha256 == NN::QuorumHash::Hash(superblock));
    BOOST_CHECK(md5 == NN::QuorumHash::Hash(legacy_superblock));

    BOOST_CHECK(sha256 != invalid);
    BOOST_CHECK(md5 != invalid);
    BOOST_CHECK(sha256 != md5);
}

BOOST_AUTO_TEST_CASE(it_compares_a_sha256_hash_for_equality)
{
    NN::Superblock superblock;

    // Hashed byte content of an empty superblock. Note that container sizes
    // are not considered in the hash:
    std::vector<unsigned char> input {
        0x00, // Zero-mag CPID count
    };

    NN::QuorumHash hash = NN::QuorumHash::Hash(superblock);
    uint256 expected = Hash(input.begin(), input.end());

    BOOST_CHECK(hash == expected);
    BOOST_CHECK(hash != uint256(0));
    BOOST_CHECK(NN::QuorumHash() != expected);
}

BOOST_AUTO_TEST_CASE(it_compares_a_string_for_equality)
{
    NN::Superblock superblock;
    NN::QuorumHash hash = NN::QuorumHash::Hash(superblock);

    BOOST_CHECK(hash == "9a538906e6466ebd2617d321f71bc94e56056ce213d366773699e28158e00614");
    BOOST_CHECK(hash != "invalid");
    BOOST_CHECK(hash != "");

    NN::Superblock legacy_superblock(1);
    hash = NN::QuorumHash::Hash(legacy_superblock);

    BOOST_CHECK(hash == Legacy::GetQuorumHash("<MAGNITUDES></MAGNITUDES>"));
    BOOST_CHECK(hash != "invalid");
    BOOST_CHECK(hash != "");

    hash = NN::QuorumHash(); // Invalid

    BOOST_CHECK(hash == "");
    BOOST_CHECK(hash != "9a538906e6466ebd2617d321f71bc94e56056ce213d366773699e28158e00614");
    BOOST_CHECK(hash != Legacy::GetQuorumHash("<MAGNITUDES></MAGNITUDES>"));
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    NN::QuorumHash hash;

    BOOST_CHECK(hash.ToString() == "");

    hash = NN::QuorumHash(uint256(0));

    BOOST_CHECK(hash.ToString()
        == "0000000000000000000000000000000000000000000000000000000000000000");

    hash = NN::QuorumHash(NN::QuorumHash::Md5Sum { });

    BOOST_CHECK(hash.ToString() == "00000000000000000000000000000000");
}

BOOST_AUTO_TEST_CASE(it_is_hashable_to_key_a_lookup_map)
{
    std::hash<NN::QuorumHash> hasher;

    NN::QuorumHash hash_invalid;

    BOOST_CHECK(hasher(hash_invalid) == 0);

    NN::QuorumHash hash_sha256(uint256(std::vector<unsigned char> {
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    }));


    // 0x01 + 0x02 + 0x03 + 0x04 (SHA256 quarters, big endian)
    BOOST_CHECK(hasher(hash_sha256) == 10);

    NN::QuorumHash hash_md5(std::array<unsigned char, 16> {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    });

    // 0x0706050403020100 + 0x1514131211100908 (MD5 halves, little endian)
    BOOST_CHECK(hasher(hash_md5) == 2024957465561532936);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_invalid)
{
    const NN::QuorumHash hash;

    BOOST_CHECK(GetSerializeSize(hash, SER_NETWORK, 1) == 1);

    CDataStream stream(SER_NETWORK, 1);
    stream << hash;

    BOOST_CHECK(stream.size() == 1);
    BOOST_CHECK(stream[0] == 0x00); // QuorumHash::Kind::INVALID
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_sha256)
{
    const std::vector<unsigned char> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    const NN::QuorumHash hash(expected);

    BOOST_CHECK(GetSerializeSize(hash, SER_NETWORK, 1) == 33);

    CDataStream stream(SER_NETWORK, 1);
    stream << hash;
    const std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output[0] == 0x01); // QuorumHash::Kind::SHA256

    BOOST_CHECK_EQUAL_COLLECTIONS(
        ++output.begin(), // we already checked the first byte
        output.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_md5)
{
    const std::vector<unsigned char> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    const NN::QuorumHash hash(expected);

    BOOST_CHECK(GetSerializeSize(hash, SER_NETWORK, 1) == 17);

    CDataStream stream(SER_NETWORK, 1);
    stream << hash;
    const std::vector<unsigned char> output(stream.begin(), stream.end());

    BOOST_CHECK(output[0] == 0x02); // QuorumHash::Kind::MD5

    BOOST_CHECK_EQUAL_COLLECTIONS(
        ++output.begin(), // we already checked the first byte
        output.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_invalid)
{
    // Initialize quorum hash with a valid value to test invalid:
    NN::QuorumHash hash(NN::QuorumHash::Md5Sum { }); // Initialize to zeros

    CDataStream stream(SER_NETWORK, 1);
    stream << (unsigned char)0x00; // QuorumHash::Kind::INVALID
    stream >> hash;

    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::INVALID);
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_sha256)
{
    NN::QuorumHash hash;

    const std::array<unsigned char, 32> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    CDataStream stream(SER_NETWORK, 1);
    stream << (unsigned char)0x01; // QuorumHash::Kind::SHA256
    stream.write(CharCast(expected.data()), expected.size());
    stream >> hash;

    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::SHA256);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        hash.Raw(),
        hash.Raw() + 32,
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_md5)
{
    NN::QuorumHash hash;

    const std::array<unsigned char, 16> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    CDataStream stream(SER_NETWORK, 1);
    stream << (unsigned char)0x02; // QuorumHash::Kind::MD5
    stream.write(CharCast(expected.data()), expected.size());
    stream >> hash;

    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::MD5);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        hash.Raw(),
        hash.Raw() + 16,
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_SUITE_END()
