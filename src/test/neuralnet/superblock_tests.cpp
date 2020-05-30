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

    static std::string ExtractValue(std::string data, std::string delimiter, int pos)
    {
        std::vector<std::string> vKeys = split(data.c_str(),delimiter);
        std::string keyvalue = "";
        if (vKeys.size() > (unsigned int)pos)
        {
            keyvalue = vKeys[pos];
        }

        return keyvalue;
    }

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

//!
//! \brief Common values shared by tests that produce superblocks from scraper
//! statistics and convergences.
//!
struct ScraperStatsMeta
{
    // Make clang happy
    ScraperStatsMeta()
    {
    }

    std::string cpid1_str = "00010203040506070809101112131415";
    NN::Cpid cpid1 = NN::Cpid::Parse(cpid1_str);

    std::string cpid2_str = "15141312111009080706050403020100";
    NN::Cpid cpid2 = NN::Cpid::Parse(cpid2_str);

    std::string cpid3_str = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    NN::Cpid cpid3 = NN::Cpid::Parse(cpid3_str);

    std::string project1 = "project_1";
    std::string project2 = "project_2";

    // Project 1, CPID 1
    double p1c1_tc = 1000;
    double p1c1_rac = 101;
    double p1c1_avg_rac = 1003;
    double p1c1_mag = 1004;

    // Project 2, CPID 1
    double p2c1_tc = 2000;
    double p2c1_rac = 102;
    double p2c1_avg_rac = 2003;
    double p2c1_mag = 2004;

    // Project 1, CPID 2
    double p1c2_tc = 3000;
    double p1c2_rac = 103;
    double p1c2_avg_rac = 3003;
    double p1c2_mag = 1.3;

    // Project 2, CPID 2
    double p2c2_tc = 4000;
    double p2c2_rac = 402;
    double p2c2_avg_rac = 4003;
    double p2c2_mag = 1.4;

    // Project 1, CPID 3
    double p1c3_tc = 5000;
    double p1c3_rac = 105;
    double p1c3_avg_rac = 5003;
    double p1c3_mag = 0.3;

    // Project 2, CPID 3
    double p2c3_tc = 6000;
    double p2c3_rac = 602;
    double p2c3_avg_rac = 6003;
    double p2c3_mag = 0.4;

    double c1_tc = p1c1_tc + p2c1_tc;
    double c1_rac = p1c1_rac + p2c1_rac;
    double c1_mag = p1c1_mag + p2c1_mag;
    NN::Magnitude c1_mag_obj = NN::Magnitude::RoundFrom(c1_mag);

    double c2_tc = p1c2_tc + p2c2_tc;
    double c2_rac = p1c2_rac + p2c2_rac;
    double c2_mag = p1c2_mag + p2c2_mag;
    NN::Magnitude c2_mag_obj = NN::Magnitude::RoundFrom(c2_mag);

    double c3_tc = p1c3_tc + p2c3_tc;
    double c3_rac = p1c3_rac + p2c3_rac;
    double c3_mag = p1c3_mag + p2c3_mag;
    NN::Magnitude c3_mag_obj = NN::Magnitude::RoundFrom(c3_mag);

    double p1_tc = p1c1_tc + p1c2_tc + p1c3_tc;
    double p1_rac = p1c1_rac + p1c2_rac + p1c3_rac;
    double p1_avg_rac = p1_rac / 3;
    double p1_avg_rac_rounded = std::nearbyint(p1_avg_rac);
    double p1_mag = p1c1_mag + p1c2_mag + p1c3_mag;

    double p2_tc = p2c1_tc + p2c2_tc + p2c3_tc;
    double p2_rac = p2c1_rac + p2c2_rac + p2c3_rac;
    double p2_avg_rac = p2_rac / 3;
    double p2_avg_rac_rounded = std::nearbyint(p2_avg_rac);
    double p2_mag = p2c1_mag + p2c2_mag + p2c3_mag;

    uint64_t cpid_count = 3;
    double cpid_total_mag = (c1_mag_obj.Scaled() + c2_mag_obj.Scaled() + c3_mag_obj.Scaled()) / 100.0;
    double cpid_average_mag = cpid_total_mag / cpid_count;

    uint64_t project_count = 2;
    double project_total_rac = p1_rac + p2_rac;
    double project_average_rac = project_total_rac / project_count;

    uint160 beacon_id_1 = uint160(std::vector<uint8_t>(sizeof(uint160), 0x01));
    uint160 beacon_id_2 = uint160(std::vector<uint8_t>(sizeof(uint160), 0x02));
};

//!
//! \brief Build a mock scraper statistics data object.
//!
//! \param meta Contains the values to initialize the scraper stats object with.
//!
const ScraperStatsAndVerifiedBeacons GetTestScraperStats(const ScraperStatsMeta& meta)
{
    ScraperStatsAndVerifiedBeacons stats_and_verified_beacons;

    ScraperObjectStats p1c1;
    p1c1.statskey.objecttype = statsobjecttype::byCPIDbyProject;
    p1c1.statskey.objectID = meta.project1 + "," + meta.cpid1_str;
    p1c1.statsvalue.dTC = meta.p1c1_tc;
    p1c1.statsvalue.dRAC = meta.p1c1_rac;
    p1c1.statsvalue.dAvgRAC = meta.p1c1_avg_rac;
    p1c1.statsvalue.dMag = meta.p1c1_mag;
    stats_and_verified_beacons.mScraperStats.emplace(p1c1.statskey, p1c1);

    ScraperObjectStats p1c2;
    p1c2.statskey.objecttype = statsobjecttype::byCPIDbyProject;
    p1c2.statskey.objectID = meta.project1 + "," + meta.cpid2_str;
    p1c2.statsvalue.dTC = meta.p1c2_tc;
    p1c2.statsvalue.dRAC = meta.p1c2_rac;
    p1c2.statsvalue.dAvgRAC = meta.p1c2_avg_rac;
    p1c2.statsvalue.dMag = meta.p1c2_mag;
    stats_and_verified_beacons.mScraperStats.emplace(p1c2.statskey, p1c2);

    ScraperObjectStats p2c1;
    p2c1.statskey.objecttype = statsobjecttype::byCPIDbyProject;
    p2c1.statskey.objectID = meta.project2 + "," + meta.cpid1_str;
    p2c1.statsvalue.dTC = meta.p2c1_tc;
    p2c1.statsvalue.dRAC = meta.p2c1_rac;
    p2c1.statsvalue.dAvgRAC = meta.p2c1_avg_rac;
    p2c1.statsvalue.dMag = meta.p2c1_mag;
    stats_and_verified_beacons.mScraperStats.emplace(p2c1.statskey, p2c1);

    ScraperObjectStats p2c2;
    p2c2.statskey.objecttype = statsobjecttype::byCPIDbyProject;
    p2c2.statskey.objectID = meta.project2 + "," + meta.cpid2_str;
    p2c2.statsvalue.dTC = meta.p2c2_tc;
    p2c2.statsvalue.dRAC = meta.p2c2_rac;
    p2c2.statsvalue.dAvgRAC = meta.p2c2_avg_rac;
    p2c2.statsvalue.dMag = meta.p2c2_mag;
    stats_and_verified_beacons.mScraperStats.emplace(p2c2.statskey, p2c2);

    ScraperObjectStats p1c3;
    p1c3.statskey.objecttype = statsobjecttype::byCPIDbyProject;
    p1c3.statskey.objectID = meta.project1 + "," + meta.cpid3_str;
    p1c3.statsvalue.dTC = meta.p1c3_tc;
    p1c3.statsvalue.dRAC = meta.p1c3_rac;
    p1c3.statsvalue.dAvgRAC = meta.p1c3_avg_rac;
    p1c3.statsvalue.dMag = meta.p1c3_mag;
    stats_and_verified_beacons.mScraperStats.emplace(p1c2.statskey, p1c3);

    ScraperObjectStats p2c3;
    p2c3.statskey.objecttype = statsobjecttype::byCPIDbyProject;
    p2c3.statskey.objectID = meta.project2 + "," + meta.cpid3_str;
    p2c3.statsvalue.dTC = meta.p2c3_tc;
    p2c3.statsvalue.dRAC = meta.p2c3_rac;
    p2c3.statsvalue.dAvgRAC = meta.p2c3_avg_rac;
    p2c3.statsvalue.dMag = meta.p2c3_mag;
    stats_and_verified_beacons.mScraperStats.emplace(p2c2.statskey, p2c3);

    ScraperObjectStats c1;
    c1.statskey.objecttype = statsobjecttype::byCPID;
    c1.statskey.objectID = meta.cpid1_str;
    c1.statsvalue.dTC = meta.c1_tc;
    c1.statsvalue.dRAC = meta.c1_rac;
    c1.statsvalue.dAvgRAC = meta.c1_rac;
    c1.statsvalue.dMag = meta.c1_mag;
    stats_and_verified_beacons.mScraperStats.emplace(c1.statskey, c1);

    ScraperObjectStats c2;
    c2.statskey.objecttype = statsobjecttype::byCPID;
    c2.statskey.objectID = meta.cpid2_str;
    c2.statsvalue.dTC = meta.c2_tc;
    c2.statsvalue.dRAC = meta.c2_rac;
    c2.statsvalue.dAvgRAC = meta.c2_rac;
    c2.statsvalue.dMag = meta.c2_mag;
    stats_and_verified_beacons.mScraperStats.emplace(c2.statskey, c2);

    ScraperObjectStats c3;
    c3.statskey.objecttype = statsobjecttype::byCPID;
    c3.statskey.objectID = meta.cpid3_str;
    c3.statsvalue.dTC = meta.c3_tc;
    c3.statsvalue.dRAC = meta.c3_rac;
    c3.statsvalue.dAvgRAC = meta.c3_rac;
    c3.statsvalue.dMag = meta.c3_mag;
    stats_and_verified_beacons.mScraperStats.emplace(c3.statskey, c3);

    ScraperObjectStats p1;
    p1.statskey.objecttype = statsobjecttype::byProject;
    p1.statskey.objectID = meta.project1;
    p1.statsvalue.dTC = meta.p1_tc;
    p1.statsvalue.dRAC = meta.p1_rac;
    p1.statsvalue.dAvgRAC = meta.p1_avg_rac;
    p1.statsvalue.dMag = meta.p1_mag;
    stats_and_verified_beacons.mScraperStats.emplace(p1.statskey, p1);

    ScraperObjectStats p2;
    p2.statskey.objecttype = statsobjecttype::byProject;
    p2.statskey.objectID = meta.project2;
    p2.statsvalue.dTC = meta.p2_tc;
    p2.statsvalue.dRAC = meta.p2_rac;
    p2.statsvalue.dAvgRAC = meta.p2_avg_rac;
    p2.statsvalue.dMag = meta.p2_mag;
    stats_and_verified_beacons.mScraperStats.emplace(p2.statskey, p2);

    stats_and_verified_beacons.mVerifiedMap.emplace(
        meta.beacon_id_1.ToString(),
        ScraperPendingBeaconEntry());
    stats_and_verified_beacons.mVerifiedMap.emplace(
        meta.beacon_id_2.ToString(),
        ScraperPendingBeaconEntry());

    return stats_and_verified_beacons;
}

//!
//! \brief Build a mock scraper convergence object.
//!
//! \param meta Contains the values to initialize the scraper stats object with.
//! \param by_parts If \c true, build it as a by-project fallback convergence.
//!
ConvergedScraperStats GetTestConvergence(
    const ScraperStatsMeta& meta,
    const bool by_parts = false)
{
    ConvergedScraperStats convergence;

    convergence.mScraperConvergedStats = GetTestScraperStats(meta).mScraperStats;

    convergence.Convergence.bByParts = by_parts;
    convergence.Convergence.nContentHash
        = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    convergence.Convergence.nUnderlyingManifestContentHash
        = uint256S("2222222222222222222222222222222222222222222222222222222222222222");

    // Add a verified beacons project part. Technically, this is the second
    // part for a manifest (offset 1). We skipped adding the beacon list part.
    //
    CDataStream verified_beacons_part(SER_NETWORK, PROTOCOL_VERSION);
    verified_beacons_part
        << ScraperPendingBeaconMap {
             { meta.beacon_id_1.ToString(), ScraperPendingBeaconEntry() },
             { meta.beacon_id_2.ToString(), ScraperPendingBeaconEntry() },
        };

    convergence.Convergence.ConvergedManifestPartsMap.emplace(
        "VerifiedBeacons",
        CSerializeData(verified_beacons_part.begin() , verified_beacons_part.end()));

    // Add some project parts with the same names as the projects in the stats.
    // The part data doesn't matter, so we just add empty containers.
    //
    convergence.Convergence.ConvergedManifestPartsMap.emplace("project_1", CSerializeData());
    convergence.Convergence.ConvergedManifestPartsMap.emplace("project_2", CSerializeData());

    return convergence;
}
} // anonymous namespace

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
    BOOST_CHECK(superblock.m_cpids.TotalMagnitude() == 0.0);
    BOOST_CHECK(superblock.m_cpids.AverageMagnitude() == 0.0);

    BOOST_CHECK(superblock.m_projects.empty() == true);
    BOOST_CHECK(superblock.m_projects.TotalRac() == 0);
    BOOST_CHECK(superblock.m_projects.AverageRac() == 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_to_the_specified_version)
{
    NN::Superblock superblock(1);

    BOOST_CHECK(superblock.m_version == 1);
    BOOST_CHECK(superblock.m_convergence_hint == 0);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0);

    BOOST_CHECK(superblock.m_cpids.empty() == true);
    BOOST_CHECK(superblock.m_cpids.TotalMagnitude() == 0.0);
    BOOST_CHECK(superblock.m_cpids.AverageMagnitude() == 0.0);

    BOOST_CHECK(superblock.m_projects.empty() == true);
    BOOST_CHECK(superblock.m_projects.TotalRac() == 0);
    BOOST_CHECK(superblock.m_projects.AverageRac() == 0);
}

BOOST_AUTO_TEST_CASE(it_initializes_from_a_provided_set_of_scraper_statistics)
{
    const ScraperStatsMeta meta;
    NN::Superblock superblock = NN::Superblock::FromStats(GetTestScraperStats(meta));

    BOOST_CHECK(superblock.m_version == NN::Superblock::CURRENT_VERSION);
    BOOST_CHECK(superblock.m_convergence_hint == 0);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0);

    auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == meta.cpid_count);
    BOOST_CHECK(cpids.TotalMagnitude() == meta.cpid_total_mag);
    BOOST_CHECK(cpids.AverageMagnitude() == meta.cpid_average_mag);

    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid1) == meta.c1_mag_obj);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid2) == meta.c2_mag_obj);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid3) == meta.c3_mag_obj);

    auto& projects = superblock.m_projects;
    BOOST_CHECK(projects.size() == meta.project_count);
    BOOST_CHECK(projects.TotalRac() == meta.project_total_rac);
    BOOST_CHECK(projects.AverageRac() == meta.project_average_rac);

    if (const auto project_1 = projects.Try(meta.project1)) {
        BOOST_CHECK(project_1->m_total_credit == meta.p1_tc);
        BOOST_CHECK(project_1->m_average_rac == meta.p1_avg_rac_rounded);
        BOOST_CHECK(project_1->m_rac == meta.p1_rac);
        BOOST_CHECK(project_1->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 1 not found in superblock.");
    }

    if (const auto project_2 = projects.Try(meta.project2)) {
        BOOST_CHECK(project_2->m_total_credit == meta.p2_tc);
        BOOST_CHECK(project_2->m_average_rac == meta.p2_avg_rac_rounded);
        BOOST_CHECK(project_2->m_rac == meta.p2_rac);
        BOOST_CHECK(project_2->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 2 not found in superblock.");
    }
}

BOOST_AUTO_TEST_CASE(it_initializes_from_a_provided_scraper_convergnce)
{
    const ScraperStatsMeta meta;
    NN::Superblock superblock = NN::Superblock::FromConvergence(GetTestConvergence(meta));

    BOOST_CHECK(superblock.m_version == NN::Superblock::CURRENT_VERSION);

    // This initialization mode must set the convergence hint derived from
    // the content hash of the convergence:
    BOOST_CHECK(superblock.m_convergence_hint == 0x11111111);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0x22222222);

    auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == meta.cpid_count);
    BOOST_CHECK(cpids.TotalMagnitude() == meta.cpid_total_mag);
    BOOST_CHECK(cpids.AverageMagnitude() == meta.cpid_average_mag);

    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid1) == meta.c1_mag_obj);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid2) == meta.c2_mag_obj);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid3) == meta.c3_mag_obj);

    auto& projects = superblock.m_projects;
    BOOST_CHECK(projects.m_converged_by_project == false);
    BOOST_CHECK(projects.size() == meta.project_count);
    BOOST_CHECK(projects.TotalRac() == meta.project_total_rac);
    BOOST_CHECK(projects.AverageRac() == meta.project_average_rac);

    if (const auto project_1 = projects.Try(meta.project1)) {
        BOOST_CHECK(project_1->m_total_credit == meta.p1_tc);
        BOOST_CHECK(project_1->m_average_rac == meta.p1_avg_rac_rounded);
        BOOST_CHECK(project_1->m_rac == meta.p1_rac);
        BOOST_CHECK(project_1->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 1 not found in superblock.");
    }

    if (const auto project_2 = projects.Try(meta.project2)) {
        BOOST_CHECK(project_2->m_total_credit == meta.p2_tc);
        BOOST_CHECK(project_2->m_average_rac == meta.p2_avg_rac_rounded);
        BOOST_CHECK(project_2->m_rac == meta.p2_rac);
        BOOST_CHECK(project_2->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 2 not found in superblock.");
    }
}

BOOST_AUTO_TEST_CASE(it_initializes_from_a_fallback_by_project_scraper_convergnce)
{
    const ScraperStatsMeta meta;
    NN::Superblock superblock = NN::Superblock::FromConvergence(
        GetTestConvergence(meta, true)); // Set fallback by project flag

    BOOST_CHECK(superblock.m_version == NN::Superblock::CURRENT_VERSION);
    BOOST_CHECK(superblock.m_convergence_hint == 0x11111111);
    // Manifest content hint not set for fallback convergence:
    BOOST_CHECK(superblock.m_manifest_content_hint == 0x00000000);

    auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == meta.cpid_count);
    BOOST_CHECK(cpids.TotalMagnitude() == meta.cpid_total_mag);
    BOOST_CHECK(cpids.AverageMagnitude() == meta.cpid_average_mag);

    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid1) == meta.c1_mag_obj);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid2) == meta.c2_mag_obj);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid3) == meta.c3_mag_obj);

    auto& projects = superblock.m_projects;

    // By project flag must be true in a fallback-to-project convergence:
    BOOST_CHECK(projects.m_converged_by_project == true);
    BOOST_CHECK(projects.size() == meta.project_count);
    BOOST_CHECK(projects.TotalRac() == meta.project_total_rac);
    BOOST_CHECK(projects.AverageRac() == meta.project_average_rac);

    if (const auto project_1 = projects.Try(meta.project1)) {
        BOOST_CHECK(project_1->m_total_credit == meta.p1_tc);
        BOOST_CHECK(project_1->m_average_rac == meta.p1_avg_rac_rounded);
        BOOST_CHECK(project_1->m_rac == meta.p1_rac);

        // The convergence hint must be set in fallback-to-project convergence.
        // This is derived from the hash of an empty part data:
        BOOST_CHECK(project_1->m_convergence_hint == 0xd3591376);
    } else {
        BOOST_FAIL("Project 1 not found in superblock.");
    }

    if (const auto project_2 = projects.Try(meta.project2)) {
        BOOST_CHECK(project_2->m_total_credit == meta.p2_tc);
        BOOST_CHECK(project_2->m_average_rac == meta.p2_avg_rac_rounded);
        BOOST_CHECK(project_2->m_rac == meta.p2_rac);

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
    BOOST_CHECK(superblock.m_cpids.At(0)->Cpid().ToString() == cpid1);
    BOOST_CHECK(superblock.m_cpids.At(0)->Magnitude() == 0);
    BOOST_CHECK(superblock.m_cpids.At(1)->Cpid().ToString() == cpid2);
    BOOST_CHECK(superblock.m_cpids.At(1)->Magnitude() == 100);
    BOOST_CHECK(superblock.m_cpids.At(2)->Cpid().ToString() == cpid3);
    BOOST_CHECK(superblock.m_cpids.At(2)->Magnitude() == 200);

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
    BOOST_CHECK(superblock.m_cpids.At(0)->Cpid().ToString() == cpid1);
    BOOST_CHECK(superblock.m_cpids.At(0)->Magnitude() == 0);
    BOOST_CHECK(superblock.m_cpids.At(1)->Cpid().ToString() == cpid2);
    BOOST_CHECK(superblock.m_cpids.At(1)->Magnitude() == 100);
    BOOST_CHECK(superblock.m_cpids.At(2)->Cpid().ToString() == cpid3);
    BOOST_CHECK(superblock.m_cpids.At(2)->Magnitude() == 200);

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
    BOOST_CHECK(cpids.At(0)->Cpid().ToString() == "002a9d6f3832d0b0028606d907e09d97");
    BOOST_CHECK(cpids.At(0)->Magnitude() == 1);
    BOOST_CHECK(cpids.At(1)->Cpid().ToString() == "002d383a19b63d698a3201793e7e3750");
    BOOST_CHECK(cpids.At(1)->Magnitude() == 24);

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

    valid.m_cpids.Add(NN::Cpid(), NN::Magnitude::RoundFrom(123));
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
    superblock.m_cpids.Add(NN::Cpid(), NN::Magnitude::RoundFrom(123));

    // The cached hash should not change:
    BOOST_CHECK(superblock.GetHash() == original_hash);
}

BOOST_AUTO_TEST_CASE(it_regenerates_its_cached_quorum_hash)
{
    NN::Superblock superblock;

    // Cache the hash:
    superblock.GetHash();

    // Change the resulting hash:
    superblock.m_cpids.Add(NN::Cpid(), NN::Magnitude::RoundFrom(123));

    // Regenrate the hash:
    BOOST_CHECK(superblock.GetHash(true) == NN::QuorumHash::Hash(superblock));
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const ScraperStatsMeta meta;
    CDataStream expected(SER_NETWORK, PROTOCOL_VERSION);

    expected
        << uint32_t{2}                                  // Version
        << uint32_t{0x11111111}                         // Convergence hint
        << uint32_t{0x22222222}                         // Manifest content hint
        << COMPACTSIZE(uint64_t{1})                     // Small magnitudes
        << meta.cpid3
        << static_cast<uint8_t>(meta.c3_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Medium magnitudes
        << meta.cpid2
        << static_cast<uint8_t>(meta.c2_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Large magnitudes
        << meta.cpid1
        << COMPACTSIZE(uint64_t{meta.c1_mag_obj.Compact()})
        << VARINT(uint32_t{0})                          // Zero count
        << uint8_t{0}                                   // By-project flag
        << COMPACTSIZE(meta.project_count)              // Project stats
        << meta.project1
        << VARINT((uint64_t)std::nearbyint(meta.p1_tc))
        << VARINT((uint64_t)std::nearbyint(meta.p1_avg_rac))
        << VARINT((uint64_t)std::nearbyint(meta.p1_rac))
        << meta.project2
        << VARINT((uint64_t)std::nearbyint(meta.p2_tc))
        << VARINT((uint64_t)std::nearbyint(meta.p2_avg_rac))
        << VARINT((uint64_t)std::nearbyint(meta.p2_rac))
        << std::vector<uint160> { meta.beacon_id_1, meta.beacon_id_2 };

    NN::Superblock superblock = NN::Superblock::FromConvergence(GetTestConvergence(meta));

    BOOST_CHECK(GetSerializeSize(superblock, SER_NETWORK, 1) == expected.size());

    CDataStream stream(SER_NETWORK, 1);
    stream << superblock;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    const ScraperStatsMeta meta;
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream
        << uint32_t{2}                                  // Version
        << uint32_t{0x11111111}                         // Convergence hint
        << uint32_t{0x22222222}                         // Manifest content hint
        << COMPACTSIZE(uint64_t{1})                     // Small magnitudes
        << meta.cpid3
        << static_cast<uint8_t>(meta.c3_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Medium magnitudes
        << meta.cpid2
        << static_cast<uint8_t>(meta.c2_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Large magnitudes
        << meta.cpid1
        << COMPACTSIZE(uint64_t{meta.c1_mag_obj.Compact()})
        << VARINT(uint32_t{0})                          // Zero count
        << uint8_t{0}                                   // By-project flag
        << COMPACTSIZE(meta.project_count)              // Project stats
        << meta.project1
        << VARINT((uint64_t)std::nearbyint(meta.p1_tc))
        << VARINT((uint64_t)std::nearbyint(meta.p1_avg_rac))
        << VARINT((uint64_t)std::nearbyint(meta.p1_rac))
        << meta.project2
        << VARINT((uint64_t)std::nearbyint(meta.p2_tc))
        << VARINT((uint64_t)std::nearbyint(meta.p2_avg_rac))
        << VARINT((uint64_t)std::nearbyint(meta.p2_rac))
        << std::vector<uint160> { meta.beacon_id_1, meta.beacon_id_2 };

    NN::Superblock superblock;
    stream >> superblock;

    BOOST_CHECK(superblock.m_version == 2);
    BOOST_CHECK(superblock.m_convergence_hint == 0x11111111);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0x22222222);

    const auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == meta.cpid_count);
    BOOST_CHECK(cpids.Zeros() == 0);
    BOOST_CHECK(cpids.TotalMagnitude() == meta.cpid_total_mag);
    BOOST_CHECK(cpids.AverageMagnitude() == meta.cpid_average_mag);

    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid1) == meta.c1_mag_obj);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid2) == meta.c2_mag_obj);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid3) == meta.c3_mag_obj);

    const auto& projects = superblock.m_projects;
    BOOST_CHECK(projects.m_converged_by_project == false);
    BOOST_CHECK(projects.size() == meta.project_count);
    BOOST_CHECK(projects.TotalRac() == meta.project_total_rac);
    BOOST_CHECK(projects.AverageRac() == meta.project_average_rac);

    if (const auto project1 = projects.Try(meta.project1)) {
        BOOST_CHECK(project1->m_total_credit == meta.p1_tc);
        BOOST_CHECK(project1->m_average_rac == meta.p1_avg_rac_rounded);
        BOOST_CHECK(project1->m_rac == meta.p1_rac);
        BOOST_CHECK(project1->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 1 not found in index.");
    }

    if (const auto project2 = projects.Try(meta.project2)) {
        BOOST_CHECK(project2->m_total_credit == meta.p2_tc);
        BOOST_CHECK(project2->m_average_rac == meta.p2_avg_rac_rounded);
        BOOST_CHECK(project2->m_rac == meta.p2_rac);
        BOOST_CHECK(project2->m_convergence_hint == 0);
    } else {
        BOOST_FAIL("Project 2 not found in index.");
    }

    const auto& beacon_ids = superblock.m_verified_beacons;
    BOOST_CHECK_EQUAL(beacon_ids.m_verified.size(), 2);
    BOOST_CHECK(beacon_ids.m_verified[0] == meta.beacon_id_1);
    BOOST_CHECK(beacon_ids.m_verified[1] == meta.beacon_id_2);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream_for_fallback_convergences)
{
    const ScraperStatsMeta meta;
    CDataStream expected(SER_NETWORK, PROTOCOL_VERSION);

    // Superblocks generated from fallback-by-project convergences include
    // convergence hints with a by-project flag set to 1:
    //
    expected
        << uint32_t{2}                                  // Version
        << uint32_t{0x11111111}                         // Convergence hint
        << uint32_t{0x00000000}                         // Manifest content hint
        << COMPACTSIZE(uint64_t{1})                     // Small magnitudes
        << meta.cpid3
        << static_cast<uint8_t>(meta.c3_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Medium magnitudes
        << meta.cpid2
        << static_cast<uint8_t>(meta.c2_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Large magnitudes
        << meta.cpid1
        << COMPACTSIZE(uint64_t{meta.c1_mag_obj.Compact()})
        << VARINT(uint32_t{0})                          // Zero count
        << uint8_t{1}                                   // By-project flag
        << COMPACTSIZE(meta.project_count)              // Project stats
        << meta.project1
        << VARINT((uint64_t)std::nearbyint(meta.p1_tc))
        << VARINT((uint64_t)std::nearbyint(meta.p1_avg_rac))
        << VARINT((uint64_t)std::nearbyint(meta.p1_rac))
        << uint32_t{0xd3591376}                         // Convergence hint
        << meta.project2
        << VARINT((uint64_t)std::nearbyint(meta.p2_tc))
        << VARINT((uint64_t)std::nearbyint(meta.p2_avg_rac))
        << VARINT((uint64_t)std::nearbyint(meta.p2_rac))
        << uint32_t{0xd3591376}                         // Convergence hint
        << std::vector<uint160> { meta.beacon_id_1, meta.beacon_id_2 };

    NN::Superblock superblock = NN::Superblock::FromConvergence(
        GetTestConvergence(meta, true)); // Set fallback by project flag

    BOOST_CHECK(GetSerializeSize(superblock, SER_NETWORK, 1) == expected.size());

    CDataStream stream(SER_NETWORK, 1);
    stream << superblock;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream_for_fallback_convergence)
{
    const ScraperStatsMeta meta;
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    // Superblocks generated from fallback-by-project convergences include
    // convergence hints with a by-project flag set to 1:
    //
    stream
        << uint32_t{2}                                  // Version
        << uint32_t{0x11111111}                         // Convergence hint
        << uint32_t{0x00000000}                         // Manifest content hint
        << COMPACTSIZE(uint64_t{1})                     // Small magnitudes
        << meta.cpid3
        << static_cast<uint8_t>(meta.c3_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Medium magnitudes
        << meta.cpid2
        << static_cast<uint8_t>(meta.c2_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Large magnitudes
        << meta.cpid1
        << COMPACTSIZE(uint64_t{meta.c1_mag_obj.Compact()})
        << VARINT(uint32_t{0})                          // Zero count
        << uint8_t{1}                                   // By-project flag
        << COMPACTSIZE(meta.project_count)              // Project stats
        << meta.project1
        << VARINT((uint64_t)std::nearbyint(meta.p1_tc))
        << VARINT((uint64_t)std::nearbyint(meta.p1_avg_rac))
        << VARINT((uint64_t)std::nearbyint(meta.p1_rac))
        << uint32_t{0xd3591376}                         // Convergence hint
        << meta.project2
        << VARINT((uint64_t)std::nearbyint(meta.p2_tc))
        << VARINT((uint64_t)std::nearbyint(meta.p2_avg_rac))
        << VARINT((uint64_t)std::nearbyint(meta.p2_rac))
        << uint32_t{0xd3591376}                         // Convergence hint
        << std::vector<uint160> { meta.beacon_id_1, meta.beacon_id_2 };

    NN::Superblock superblock;
    stream >> superblock;

    BOOST_CHECK(superblock.m_version == 2);
    BOOST_CHECK(superblock.m_convergence_hint == 0x11111111);
    BOOST_CHECK(superblock.m_manifest_content_hint == 0x00000000);

    const auto& cpids = superblock.m_cpids;
    BOOST_CHECK(cpids.size() == meta.cpid_count);
    BOOST_CHECK(cpids.Zeros() == 0);
    BOOST_CHECK_EQUAL(cpids.TotalMagnitude(), meta.cpid_total_mag);
    BOOST_CHECK_EQUAL(cpids.AverageMagnitude(), meta.cpid_average_mag);

    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid1) == meta.c1_mag_obj);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid2) == meta.c2_mag_obj);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid3) == meta.c3_mag_obj);

    const auto& projects = superblock.m_projects;
    BOOST_CHECK(projects.m_converged_by_project == true);
    BOOST_CHECK(projects.size() == meta.project_count);
    BOOST_CHECK(projects.TotalRac() == meta.project_total_rac);
    BOOST_CHECK(projects.AverageRac() == meta.project_average_rac);

    if (const auto project1 = projects.Try(meta.project1)) {
        BOOST_CHECK(project1->m_total_credit == meta.p1_tc);
        BOOST_CHECK(project1->m_average_rac == meta.p1_avg_rac_rounded);
        BOOST_CHECK(project1->m_rac == meta.p1_rac);
        BOOST_CHECK(project1->m_convergence_hint == 0xd3591376);
    } else {
        BOOST_FAIL("Project 1 not found in index.");
    }

    if (const auto project2 = projects.Try(meta.project2)) {
        BOOST_CHECK(project2->m_total_credit == meta.p2_tc);
        BOOST_CHECK(project2->m_average_rac == meta.p2_avg_rac_rounded);
        BOOST_CHECK(project2->m_rac == meta.p2_rac);
        BOOST_CHECK(project2->m_convergence_hint == 0xd3591376);
    } else {
        BOOST_FAIL("Project 2 not found in index.");
    }

    const auto& beacon_ids = superblock.m_verified_beacons;
    BOOST_CHECK_EQUAL(beacon_ids.m_verified.size(), 2);
    BOOST_CHECK(beacon_ids.m_verified[0] == meta.beacon_id_1);
    BOOST_CHECK(beacon_ids.m_verified[1] == meta.beacon_id_2);
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

    cpids.Add(NN::Cpid(), NN::Magnitude::RoundFrom(123));

    BOOST_CHECK(cpids.size() == 1);
}

BOOST_AUTO_TEST_CASE(it_adds_a_rounded_cpid_magnitude_to_the_index)
{
    NN::Superblock::CpidIndex cpids;

    NN::Cpid cpid1 = NN::Cpid::Parse("00010203040506070809101112131415");
    NN::Cpid cpid2 = NN::Cpid::Parse("15141312111009080706050403020100");

    cpids.RoundAndAdd(cpid1, 123.5);
    cpids.RoundAndAdd(cpid2, 123.4);

    BOOST_CHECK(cpids.MagnitudeOf(cpid1) == 124);
    BOOST_CHECK(cpids.MagnitudeOf(cpid2) == 123);
}

BOOST_AUTO_TEST_CASE(it_adds_a_legacy_rounded_cpid_magnitude_to_the_index)
{
    // Initializing the CPID index with a count of zero-magnitude CPIDs
    // switches it to legacy mode:
    //
    NN::Superblock::CpidIndex cpids(0);

    NN::Cpid cpid1 = NN::Cpid::Parse("11111111111111111111111111111111");
    NN::Cpid cpid2 = NN::Cpid::Parse("22222222222222222222222222222222");
    NN::Cpid cpid3 = NN::Cpid::Parse("33333333333333333333333333333333");
    NN::Cpid cpid4 = NN::Cpid::Parse("44444444444444444444444444444444");

    // The ScraperGetNeuralContract() function that these classes replace
    // rounded magnitude values using a half-away-from-zero rounding mode
    // to determine whether floating-point magnitudes round-down to zero,
    // but it added the magnitude values to the superblock with half-even
    // rounding. This caused legacy superblock contracts to contain CPIDs
    // with zero magnitude when the rounding results differed.
    //
    // To create legacy superblocks from scraper statistics with matching
    // hashes, we filter magnitudes using the same rounding rules:
    //
    //  - A magnitude of 0.5 is rounded-down to zero.
    //  - A magnitude less than 0.5 is omitted from the superblock.
    //
    cpids.RoundAndAdd(cpid1, 1.1);
    cpids.RoundAndAdd(cpid2, 1.5);
    cpids.RoundAndAdd(cpid3, 0.5);
    cpids.RoundAndAdd(cpid4, 0.4);

    BOOST_CHECK(cpids.size() == 3); // cpid4 is omitted

    BOOST_CHECK(cpids.MagnitudeOf(cpid1) == 1);
    BOOST_CHECK(cpids.MagnitudeOf(cpid2) == 2);
    BOOST_CHECK(cpids.MagnitudeOf(cpid3) == 0);
    BOOST_CHECK(cpids.MagnitudeOf(cpid4) == 0);
}

BOOST_AUTO_TEST_CASE(it_fetches_a_cpid_pair_by_offset)
{
    NN::Superblock::CpidIndex cpids;

    NN::Cpid cpid1 = NN::Cpid::Parse("00010203040506070809101112131415");
    NN::Cpid cpid2 = NN::Cpid::Parse("15141312111009080706050403020100");

    cpids.Add(cpid1, NN::Magnitude::RoundFrom(123));
    cpids.Add(cpid2, NN::Magnitude::RoundFrom(456));

    BOOST_CHECK(cpids.At(0)->Cpid() == cpid1);
    BOOST_CHECK(cpids.At(0)->Magnitude() == 123);
    BOOST_CHECK(cpids.At(1)->Cpid() == cpid2);
    BOOST_CHECK(cpids.At(1)->Magnitude() == 456);

    BOOST_CHECK(cpids.At(cpids.size()) == cpids.end());
}

BOOST_AUTO_TEST_CASE(it_fetches_the_magnitude_of_a_specific_cpid)
{
    NN::Superblock::CpidIndex cpids;
    NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");

    cpids.Add(cpid, NN::Magnitude::RoundFrom(123));

    BOOST_CHECK(cpids.MagnitudeOf(cpid) == 123);
}

BOOST_AUTO_TEST_CASE(it_assumes_zero_magnitude_for_a_nonexistent_cpid)
{
    NN::Superblock::CpidIndex cpids;
    NN::Cpid cpid = NN::Cpid::Parse("00010203040506070809101112131415");

    BOOST_CHECK(cpids.MagnitudeOf(cpid) == 0);
}

BOOST_AUTO_TEST_CASE(it_counts_the_number_of_active_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.size() == 0);

    cpids.Add(NN::Cpid(), NN::Magnitude::RoundFrom(123));

    BOOST_CHECK(cpids.size() == 1);
}

BOOST_AUTO_TEST_CASE(it_determines_whether_it_contains_any_active_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.empty() == true);

    cpids.Add(NN::Cpid(), NN::Magnitude::RoundFrom(123));

    BOOST_CHECK(cpids.empty() == false);
}

BOOST_AUTO_TEST_CASE(it_tallies_the_number_of_zero_magnitude_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.Zeros() == 0);

    // Add only one zero-magnitude CPID:
    cpids.Add(
        NN::Cpid::Parse("00010203040506070809101112131415"),
        NN::Magnitude::Zero());
    cpids.Add(
        NN::Cpid::Parse("15141312111009080706050403020100"),
        NN::Magnitude::RoundFrom(123));

    BOOST_CHECK(cpids.Zeros() == 1);
}

BOOST_AUTO_TEST_CASE(it_skips_tallying_zero_magnitudes_for_legacy_superblocks)
{
    // Legacy superblocks embed the number of zero-magnitude CPIDs in the
    // contract, so we won't count them upon insertion if explicitly set.

    NN::Superblock::CpidIndex cpids(123);

    BOOST_CHECK(cpids.Zeros() == 123);

    cpids.AddLegacy(NN::Cpid::Parse("00010203040506070809101112131415"), 0);

    BOOST_CHECK(cpids.Zeros() == 123);
}

BOOST_AUTO_TEST_CASE(it_sums_the_count_of_both_active_and_zero_magnitude_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.TotalCount() == 0);

    // Add only one zero-magnitude CPID:
    cpids.Add(
        NN::Cpid::Parse("00010203040506070809101112131415"),
        NN::Magnitude::RoundFrom(0));
    cpids.Add(
        NN::Cpid::Parse("15141312111009080706050403020100"),
        NN::Magnitude::RoundFrom(123));

    BOOST_CHECK(cpids.TotalCount() == 2);
}

BOOST_AUTO_TEST_CASE(it_tallies_the_sum_of_the_magnitudes_of_active_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.TotalMagnitude() == 0.0);

    cpids.Add(NN::Cpid(), NN::Magnitude::Zero());
    cpids.Add(
        NN::Cpid::Parse("00010203040506070809101112131415"),
        NN::Magnitude::RoundFrom(123));
    cpids.Add(
        NN::Cpid::Parse("15141312111009080706050403020100"),
        NN::Magnitude::RoundFrom(456));

    BOOST_CHECK(cpids.TotalMagnitude() == 579.0);
}

BOOST_AUTO_TEST_CASE(it_calculates_the_average_magnitude_of_active_cpids)
{
    NN::Superblock::CpidIndex cpids;

    BOOST_CHECK(cpids.AverageMagnitude() == 0.0);

    cpids.Add(NN::Cpid(), NN::Magnitude::Zero());
    cpids.Add(
        NN::Cpid::Parse("00010203040506070809101112131415"),
        NN::Magnitude::RoundFrom(123));
    cpids.Add(
        NN::Cpid::Parse("15141312111009080706050403020100"),
        NN::Magnitude::RoundFrom(456));

    BOOST_CHECK(cpids.AverageMagnitude() == 289.5);
}

BOOST_AUTO_TEST_CASE(it_is_iterable)
{
    const ScraperStatsMeta meta;
    NN::Superblock::CpidIndex cpids;

    cpids.Add(meta.cpid1, meta.c1_mag_obj);
    cpids.Add(meta.cpid2, meta.c2_mag_obj);
    cpids.Add(meta.cpid3, meta.c3_mag_obj);

    size_t counter = 0;

    for (const auto& cpid : cpids) {
        // Iteration over CPID-to-magnitude mappings proceeds in order of the
        // magnitude size segments (small -> medium -> large), so these CPIDs
        // appear in a different order than inserted:
        //
        switch (counter) {
            case 0:
                BOOST_CHECK(cpid.Cpid() == meta.cpid3);
                BOOST_CHECK(cpid.Magnitude() == meta.c3_mag_obj);
                break;
            case 1:
                BOOST_CHECK(cpid.Cpid() == meta.cpid2);
                BOOST_CHECK(cpid.Magnitude() == meta.c2_mag_obj);
                break;
            case 2:
                BOOST_CHECK(cpid.Cpid() == meta.cpid1);
                BOOST_CHECK(cpid.Magnitude() == meta.c1_mag_obj);
                break;
            default:
                BOOST_FAIL("Unexpected number of iterations.");
                break;
        }

        counter++;
    }

    BOOST_CHECK(counter == 3);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const ScraperStatsMeta meta;
    CDataStream expected(SER_NETWORK, PROTOCOL_VERSION);

    expected
        << COMPACTSIZE(uint64_t{1})                     // Small magnitudes
        << meta.cpid3
        << static_cast<uint8_t>(meta.c3_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Medium magnitudes
        << meta.cpid2
        << static_cast<uint8_t>(meta.c2_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Large magnitudes
        << meta.cpid1
        << COMPACTSIZE(uint64_t{meta.c1_mag_obj.Compact()})
        << VARINT(uint32_t{1});                         // Zero magnitude count

    NN::Superblock::CpidIndex cpids;

    cpids.Add(meta.cpid1, NN::Magnitude::RoundFrom(meta.c1_mag));
    cpids.Add(meta.cpid2, NN::Magnitude::RoundFrom(meta.c2_mag));
    cpids.Add(meta.cpid3, NN::Magnitude::RoundFrom(meta.c3_mag));
    cpids.Add(NN::Cpid(), NN::Magnitude::Zero());

    BOOST_CHECK(GetSerializeSize(cpids, SER_NETWORK, 1) == expected.size());

    CDataStream stream(SER_NETWORK, 1);
    stream << cpids;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    const ScraperStatsMeta meta;
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream
        << COMPACTSIZE(uint64_t{1})                     // Small magnitudes
        << meta.cpid3
        << static_cast<uint8_t>(meta.c3_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Medium magnitudes
        << meta.cpid2
        << static_cast<uint8_t>(meta.c2_mag_obj.Compact())
        << COMPACTSIZE(uint64_t{1})                     // Large magnitudes
        << meta.cpid1
        << COMPACTSIZE(uint64_t{meta.c1_mag_obj.Compact()})
        << VARINT(uint32_t{1});                         // Zero magnitude count

    NN::Superblock::CpidIndex cpids;
    stream >> cpids;

    BOOST_CHECK(cpids.size() == 3);
    BOOST_CHECK(cpids.Zeros() == 1);
    BOOST_CHECK(cpids.At(0)->Cpid() == meta.cpid3);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid3) == meta.c3_mag_obj);
    BOOST_CHECK(cpids.At(1)->Cpid() == meta.cpid2);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid2) == meta.c2_mag_obj);
    BOOST_CHECK(cpids.At(2)->Cpid() == meta.cpid1);
    BOOST_CHECK(cpids.MagnitudeOf(meta.cpid1) == meta.c1_mag_obj);
    BOOST_CHECK(cpids.TotalMagnitude() == meta.cpid_total_mag);
    BOOST_CHECK(cpids.AverageMagnitude() == meta.cpid_average_mag);
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
// Superblock::VerifiedBeacons
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Superblock__VerifiedBeacons)

BOOST_AUTO_TEST_CASE(it_initializes_to_an_empty_collection)
{
    const NN::Superblock::VerifiedBeacons beacon_ids;

    BOOST_CHECK(beacon_ids.m_verified.empty() == true);
}

BOOST_AUTO_TEST_CASE(it_replaces_the_collection_from_scraper_statistics)
{
    const ScraperStatsMeta meta;
    const ScraperStatsAndVerifiedBeacons stats_and_verified_beacons = GetTestScraperStats(meta);

    NN::Superblock::VerifiedBeacons beacon_ids;

    beacon_ids.Reset(stats_and_verified_beacons.mVerifiedMap);

    BOOST_CHECK_EQUAL(beacon_ids.m_verified.size(), 2);
    BOOST_CHECK(beacon_ids.m_verified[0] == meta.beacon_id_1);
    BOOST_CHECK(beacon_ids.m_verified[1] == meta.beacon_id_2);
}

BOOST_AUTO_TEST_CASE(it_serializes_to_a_stream)
{
    const ScraperStatsMeta meta;
    CDataStream expected(SER_NETWORK, PROTOCOL_VERSION);

    expected << std::vector<uint160> {
        meta.beacon_id_1,
        meta.beacon_id_2,
    };

    NN::Superblock::VerifiedBeacons beacon_ids;

    beacon_ids.m_verified.emplace_back(meta.beacon_id_1);
    beacon_ids.m_verified.emplace_back(meta.beacon_id_2);

    CDataStream stream(SER_NETWORK, 1);
    stream << beacon_ids;

    BOOST_CHECK_EQUAL_COLLECTIONS(
        stream.begin(),
        stream.end(),
        expected.begin(),
        expected.end());
}

BOOST_AUTO_TEST_CASE(it_deserializes_from_a_stream)
{
    const ScraperStatsMeta meta;
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream << std::vector<uint160> {
        meta.beacon_id_1,
        meta.beacon_id_2,
    };

    NN::Superblock::VerifiedBeacons beacon_ids;
    stream >> beacon_ids;

    BOOST_CHECK_EQUAL(beacon_ids.m_verified.size(), 2);
    BOOST_CHECK(beacon_ids.m_verified[0] == meta.beacon_id_1);
    BOOST_CHECK(beacon_ids.m_verified[1] == meta.beacon_id_2);
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
    NN::QuorumHash hash(uint256{});

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
    const ScraperStatsMeta meta;
    CHashWriter expected_hasher(SER_GETHASH, PROTOCOL_VERSION);

    // Note: convergence hints embedded in a superblock are NOT considered
    // when generating the superblock hash, and the container sizes aren't
    // either:
    //
    expected_hasher
        // To allow for direct hashing of scraper stats data without
        // allocating a superblock, we generate an intermediate hash
        // of the segments of CPID-to-magnitude mappings:
        //
        << (CHashWriter(SER_GETHASH, PROTOCOL_VERSION)
            << (CHashWriter(SER_GETHASH, PROTOCOL_VERSION)
                << meta.cpid3
                << static_cast<uint8_t>(meta.c3_mag_obj.Compact()))
                .GetHash()
            << (CHashWriter(SER_GETHASH, PROTOCOL_VERSION)
                << meta.cpid2
                << static_cast<uint8_t>(meta.c2_mag_obj.Compact()))
                .GetHash()
            << (CHashWriter(SER_GETHASH, PROTOCOL_VERSION)
                << meta.cpid1
                << COMPACTSIZE(uint64_t{meta.c1_mag_obj.Compact()}))
                .GetHash())
            .GetHash()
        << VARINT(uint32_t{0}) // Zero-mag count
        << meta.project1
        << VARINT((uint64_t)std::nearbyint(meta.p1_tc))
        << VARINT((uint64_t)std::nearbyint(meta.p1_avg_rac))
        << VARINT((uint64_t)std::nearbyint(meta.p1_rac))
        << meta.project2
        << VARINT((uint64_t)std::nearbyint(meta.p2_tc))
        << VARINT((uint64_t)std::nearbyint(meta.p2_avg_rac))
        << VARINT((uint64_t)std::nearbyint(meta.p2_rac))
        << std::vector<uint160> { meta.beacon_id_1, meta.beacon_id_2 };

    const uint256 expected = expected_hasher.GetHash();

    const NN::QuorumHash hash = NN::QuorumHash::Hash(
        NN::Superblock::FromStats(GetTestScraperStats(meta)));

    BOOST_CHECK(hash.Valid() == true);
    BOOST_CHECK(hash.Which() == NN::QuorumHash::Kind::SHA256);
    BOOST_CHECK(hash == expected);
    BOOST_CHECK(hash.ToString() == expected.ToString());
}

BOOST_AUTO_TEST_CASE(it_hashes_a_set_of_scraper_statistics_like_a_superblock)
{
    const ScraperStatsMeta meta;
    const ScraperStatsAndVerifiedBeacons stats_and_verified_beacons = GetTestScraperStats(meta);

    NN::Superblock superblock = NN::Superblock::FromStats(stats_and_verified_beacons);
    NN::QuorumHash quorum_hash = NN::QuorumHash::Hash(stats_and_verified_beacons);

    BOOST_CHECK(quorum_hash == superblock.GetHash());
}

BOOST_AUTO_TEST_CASE(it_parses_a_sha256_hash_string)
{
    const std::vector<unsigned char> expected {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };

    NN::QuorumHash hash = NN::QuorumHash::Parse(uint256(expected).ToString());

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

BOOST_AUTO_TEST_CASE(it_parses_an_empty_superblock_hash_to_an_invalid_variant)
{
    // This is the hash of an empty legacy superblock contract. A bug in
    // previous versions caused nodes to vote for empty superblocks when
    // staking a block. It should parse to an invalid quorum hash value:
    //
    NN::QuorumHash hash = NN::QuorumHash::Parse("d41d8cd98f00b204e9800998ecf8427e");

    BOOST_CHECK(hash.Valid() == false);
}

BOOST_AUTO_TEST_CASE(it_hashes_cpid_magnitudes_from_a_legacy_superblock)
{
    // Version 1 superblocks hash with the legacy MD5-based algorithm:
    NN::Superblock superblock(1);

    std::string cpid1 = "00010203040506070809101112131415";
    std::string cpid2 = "15141312111009080706050403020100";

    superblock.m_cpids.AddLegacy(NN::Cpid::Parse(cpid1), 100);
    superblock.m_cpids.AddLegacy(NN::Cpid::Parse(cpid2), 200);

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
    const NN::Superblock superblock;
    CHashWriter expected_hasher(SER_GETHASH, PROTOCOL_VERSION);

    expected_hasher
        << (CHashWriter(SER_GETHASH, PROTOCOL_VERSION)
            << CHashWriter(SER_GETHASH, PROTOCOL_VERSION).GetHash()
            << CHashWriter(SER_GETHASH, PROTOCOL_VERSION).GetHash()
            << CHashWriter(SER_GETHASH, PROTOCOL_VERSION).GetHash())
            .GetHash()
        << VARINT(uint32_t{0})      // Zero-magnitude count
        << std::vector<uint160> {}; // Verified beacons

    // Hashed byte content of an empty superblock. Note that container sizes
    // are not considered in the hash:
    const uint256 expected = expected_hasher.GetHash();

    NN::QuorumHash hash = NN::QuorumHash::Hash(superblock);

    BOOST_CHECK(hash == expected);
    BOOST_CHECK(hash != uint256());
    BOOST_CHECK(NN::QuorumHash() != expected);
}

BOOST_AUTO_TEST_CASE(it_compares_a_string_for_equality)
{
    const NN::Superblock superblock;
    NN::QuorumHash hash = NN::QuorumHash::Hash(superblock);

    CHashWriter expected_hasher(SER_GETHASH, PROTOCOL_VERSION);

    expected_hasher
        << (CHashWriter(SER_GETHASH, PROTOCOL_VERSION)
            << CHashWriter(SER_GETHASH, PROTOCOL_VERSION).GetHash()
            << CHashWriter(SER_GETHASH, PROTOCOL_VERSION).GetHash()
            << CHashWriter(SER_GETHASH, PROTOCOL_VERSION).GetHash())
            .GetHash()
        << VARINT(uint32_t{0})      // Zero-magnitude count
        << std::vector<uint160> {}; // Verified beacons

    // Hashed byte content of an empty superblock. Note that container sizes
    // are not considered in the hash:
    const std::string expected = expected_hasher.GetHash().ToString();

    BOOST_CHECK(hash == expected);
    BOOST_CHECK(hash != "invalid");
    BOOST_CHECK(hash != "");

    NN::Superblock legacy_superblock(1);
    hash = NN::QuorumHash::Hash(legacy_superblock);

    BOOST_CHECK(hash == Legacy::GetQuorumHash("<MAGNITUDES></MAGNITUDES>"));
    BOOST_CHECK(hash != "invalid");
    BOOST_CHECK(hash != "");

    hash = NN::QuorumHash(); // Invalid

    BOOST_CHECK(hash == "");
    BOOST_CHECK(hash != expected);
    BOOST_CHECK(hash != Legacy::GetQuorumHash("<MAGNITUDES></MAGNITUDES>"));
}

BOOST_AUTO_TEST_CASE(it_represents_itself_as_a_string)
{
    NN::QuorumHash hash;

    BOOST_CHECK(hash.ToString() == "");

    hash = NN::QuorumHash(uint256());

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
