#include "compat/endian.h"
#include "neuralnet/superblock.h"
#include "util.h"

#include <boost/variant/apply_visitor.hpp>
#include <openssl/md5.h>

using namespace NN;

std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);
std::string ExtractValue(std::string data, std::string delimiter, int pos);

namespace {
//!
//! \brief Parses and unpacks superblock data from legacy superblock contracts.
//!
//! Legacy superblock string contracts contain the following XML-like elements
//! with records delimited by semicolons and fields delimited by commas:
//!
//!   <MAGNITUDES>EXTERNAL_CPID,MAGNITUDE;...</MAGNITUDES>
//!   <AVERAGES>PROJECT_NAME,AVERAGE_RAC,RAC;...</AVERAGES>
//!   <QUOTES>CURRENCY_CODE,PRICE;...</QUOTES>
//!
//! The application no longer supports the vestigial price quotes so these are
//! not parsed by this class.
//!
//! Newer, binary-packed legacy contracts replace <MAGNITUDES>...</MAGNITUDES>
//! elements with these sections to conserve space:
//!
//!   <ZERO>COUNT</ZERO>
//!   <BINARY>PACKED_DATA...</BINARY>
//!
//! ...where PACKED_DATA are the binary representations of alternating 16-byte
//! external CPID values and 2-byte (16-bit) magnitudes. The COUNT placeholder
//! tallies the number of zero-magnitude CPIDs omitted from the contract.
//!
class LegacySuperblockParser
{
public:
    std::string m_magnitudes;        //!< Mapping of CPIDs to magnitudes.
    std::string m_binary_magnitudes; //!< Packed mapping of CPIDs to magnitudes.
    std::string m_averages;          //!< Project RACs and average RACs.
    int32_t m_zero_mags;             //!< Count of CPIDs with zero magnitude.

    //!
    //! \brief Initialize a parser from a legacy superblock contract.
    //!
    //! \param packed Legacy superblock contract to extract data from. The CPID
    //! magnitude records may exist as text or in a packed binary format.
    //!
    LegacySuperblockParser(const std::string& packed)
        : m_magnitudes(ExtractXML(packed, "<MAGNITUDES>", "</MAGNITUDES>"))
        , m_binary_magnitudes(ExtractXML(packed, "<BINARY>", "</BINARY>"))
        , m_averages(ExtractXML(packed, "<AVERAGES>", "</AVERAGES>"))
        , m_zero_mags(0)
    {
        try {
            m_zero_mags = std::stoi(ExtractXML(packed, "<ZERO>", "</ZERO>"));
        } catch (...) {
            LogPrintf("LegacySuperblock: Failed to parse zero mag CPIDs");
        }
    }

    //!
    //! \brief Parse project recent average credit and average RAC from the
    //! legacy superblock data.
    //!
    //! \return Superblock project statistics to set on a superblock instance.
    //!
    Superblock::ProjectIndex ExtractProjects() const
    {
        Superblock::ProjectIndex projects;
        size_t start = 0;
        size_t end = m_averages.find(";");

        while (end != std::string::npos) {
            std::string project_record = m_averages.substr(start, end - start);
            std::vector<std::string> parts = split(project_record, ",");

            start = end + 1;
            end = m_averages.find(";", start);

            if (parts.size() < 2
                || parts[0].empty()
                || parts[0] == "NeuralNetwork") // Ignore network stats
            {
                continue;
            }

            try {
                projects.Add(std::move(parts[0]), Superblock::ProjectStats(
                    std::stoi(parts[1]),                          // average RAC
                    parts.size() > 2 ? std::stoi(parts[2]) : 0)); // RAC

            } catch (...) {
                LogPrintf("ExtractProjects(): Failed to parse project RAC.");
            }
        }

        return projects;
    }

    //!
    //! \brief Parse CPID magnitudes from the legacy superblock data.
    //!
    //! \return Superblock CPID map to set on a superblock instance.
    //!
    Superblock::CpidIndex ExtractMagnitudes() const
    {
        if (!m_binary_magnitudes.empty()) {
            return ExtractBinaryMagnitudes();
        }

        return ExtractTextMagnitudes();
    }

private:
    //!
    //! \brief Unpack CPID magnitudes stored in binary format.
    //!
    //! \return Superblock CPID map to set on a superblock instance.
    //!
    Superblock::CpidIndex ExtractBinaryMagnitudes() const
    {
        Superblock::CpidIndex magnitudes(m_zero_mags);

        const char* const byte_ptr = m_binary_magnitudes.data();
        const size_t binary_size = m_binary_magnitudes.size();

        for (size_t x = 0; x < binary_size && binary_size - x >= 18; x += 18) {
            magnitudes.Add(
                *reinterpret_cast<const Cpid*>(byte_ptr + x),
                be16toh(*reinterpret_cast<const int16_t*>(byte_ptr + x + 16)));
        }

        return magnitudes;
    }

    //!
    //! \brief Parse CPID magnitudes stored as text.
    //!
    //! \return Superblock CPID map to set on a superblock instance.
    //!
    Superblock::CpidIndex ExtractTextMagnitudes() const
    {
        Superblock::CpidIndex magnitudes(m_zero_mags);

        size_t start = 0;
        size_t end = m_magnitudes.find(";");

        while (end != std::string::npos) {
            std::string cpid_record = m_magnitudes.substr(start, end - start);
            std::vector<std::string> parts = split(cpid_record, ",");

            start = end + 1;
            end = m_magnitudes.find(";", start);

            if (parts.size() != 2 || parts[0].empty()) {
                continue;
            }

            try {
                magnitudes.Add(MiningId::Parse(parts[0]), std::stoi(parts[1]));
            } catch(...) {
                LogPrintf("ExtractTextMagnitude(): Failed to parse magnitude.");
            }
        }

        return magnitudes;
    }
}; // LegacySuperblockParser

//!
//! \brief Gets the string representation of a quorum hash object.
//!
struct QuorumHashToStringVisitor : boost::static_visitor<std::string>
{
    //!
    //! \brief Get the string representation of an invalid or empty quorum hash.
    //!
    //! \param invalid The object to create a string for.
    //!
    //! \return An empty string.
    //!
    std::string operator()(const QuorumHash::Invalid invalid) const
    {
        return std::string();
    }

    //!
    //! \brief Get the string representation of a SHA256 quorum hash.
    //!
    //! \param hash The object to create a string for.
    //!
    //! \return 64-character hex-encoded representation of bytes in the hash.
    //!
    std::string operator()(const uint256& hash) const
    {
        return hash.ToString();
    }

    //!
    //! \brief Get the string representation of a legacy MD5 quorum hash.
    //!
    //! \param legacy_hash The object to create a string for.
    //!
    //! \return 32-character hex-encoded representation of bytes in the hash.
    //!
    std::string operator()(const QuorumHash::Md5Sum& legacy_hash) const
    {
        return HexStr(legacy_hash.begin(), legacy_hash.end());
    }
};

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
} // anonymous namespace

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Class: Superblock
// -----------------------------------------------------------------------------

Superblock::Superblock()
    : m_version(Superblock::CURRENT_VERSION)
    , m_height(0)
    , m_timestamp(0)
{
}

Superblock::Superblock(uint32_t version)
    : m_version(version)
    , m_height(0)
    , m_timestamp(0)
{
}

Superblock Superblock::FromStats(const ScraperStats& stats)
{
    // The loop below depends on the relative value of these enum types:
    static_assert(statsobjecttype::NetworkWide < statsobjecttype::byCPID);
    static_assert(statsobjecttype::byCPID < statsobjecttype::byCPIDbyProject);
    static_assert(statsobjecttype::byProject < statsobjecttype::byCPIDbyProject);

    Superblock superblock;

    for (const auto& entry : stats) {
        // A CPID, project name, or project name/CPID pair that identifies the
        // current statistics record:
        const std::string& object_id = entry.first.objectID;

        switch (entry.first.objecttype) {
            case statsobjecttype::NetworkWide:
                // This map starts with a single, network-wide statistics entry.
                // Skip it because superblock objects will recalculate the stats
                // as needed after deserialization.
                //
                continue;

            case statsobjecttype::byCPID:
                superblock.m_cpids.Add(
                    Cpid::Parse(object_id),
                    std::round(entry.second.statsvalue.dMag));

                break;

            case statsobjecttype::byProject:
                superblock.m_projects.Add(object_id, ProjectStats(
                    std::round(entry.second.statsvalue.dTC),
                    std::round(entry.second.statsvalue.dAvgRAC),
                    std::round(entry.second.statsvalue.dRAC)));

                break;

            default:
                // The scraper statistics map orders the entries by "objecttype"
                // starting with "byCPID" and "byProject". After importing these
                // sections into the superblock, we can exit this loop.
                //
                return superblock;
        }
    }

    return superblock;
}

Superblock Superblock::UnpackLegacy(const std::string& packed)
{
    // Legacy-packed superblocks always initialize to version 1:
    Superblock superblock(1);
    LegacySuperblockParser legacy(packed);

    superblock.m_cpids = legacy.ExtractMagnitudes();
    superblock.m_projects = legacy.ExtractProjects();

    return superblock;
}

std::string Superblock::PackLegacy() const
{
    std::stringstream out;

    out << "<ZERO>" << m_cpids.Zeros() << "</ZERO>"
        << "<BINARY>";

    for (const auto& cpid_pair : m_cpids) {
        uint16_t mag = htobe16(cpid_pair.second);

        out.write(reinterpret_cast<const char*>(cpid_pair.first.Raw().data()), 16);
        out.write(reinterpret_cast<const char*>(&mag), sizeof(uint16_t));
    }

    out << "</BINARY>"
        << "<AVERAGES>";

    for (const auto& project_pair : m_projects) {
        out << project_pair.first << ","
            << project_pair.second.m_average_rac << ","
            << project_pair.second.m_rac << ";";
    }

    out << "</AVERAGES>"
        << "<QUOTES></QUOTES>";

    return out.str();
}

int64_t Superblock::Age() const
{
    return GetAdjustedTime() - m_timestamp;
}

// -----------------------------------------------------------------------------
// Class: Superblock::CpidIndex
// -----------------------------------------------------------------------------

Superblock::CpidIndex::CpidIndex()
    : m_zero_magnitude_count(0)
    , m_total_magnitude(0)
    , m_legacy(false)
{
}

Superblock::CpidIndex::CpidIndex(uint32_t zero_magnitude_count)
    : m_zero_magnitude_count(zero_magnitude_count)
    , m_total_magnitude(0)
    , m_legacy(true)
{
}

Superblock::CpidIndex::const_iterator Superblock::CpidIndex::begin() const
{
    return m_magnitudes.begin();
}

Superblock::CpidIndex::const_iterator Superblock::CpidIndex::end() const
{
    return m_magnitudes.end();
}

Superblock::CpidIndex::size_type Superblock::CpidIndex::size() const
{
    return m_magnitudes.size();
}

bool Superblock::CpidIndex::empty() const
{
    return m_magnitudes.empty();
}

uint32_t Superblock::CpidIndex::Zeros() const
{
    return m_zero_magnitude_count;
}

size_t Superblock::CpidIndex::TotalCount() const
{
    return m_magnitudes.size() + m_zero_magnitude_count;
}

uint64_t Superblock::CpidIndex::TotalMagnitude() const
{
    return m_total_magnitude;
}

double Superblock::CpidIndex::AverageMagnitude() const
{
    if (m_magnitudes.empty()) {
        return 0;
    }

    return static_cast<double>(m_total_magnitude) / m_magnitudes.size();
}

uint16_t Superblock::CpidIndex::MagnitudeOf(const Cpid& cpid) const
{
    const auto iter = m_magnitudes.find(cpid);

    if (iter == m_magnitudes.end()) {
        return 0;
    }

    return iter->second;
}

size_t Superblock::CpidIndex::OffsetOf(const Cpid& cpid) const
{
    const auto iter = m_magnitudes.find(cpid);

    if (iter == m_magnitudes.end()) {
        return m_magnitudes.size();
    }

    // Not very efficient--we can optimize this if needed:
    return std::distance(m_magnitudes.begin(), iter);
}

Superblock::CpidIndex::const_iterator
Superblock::CpidIndex::At(const size_t offset) const
{
    // Not very efficient--we can optimize this if needed:
    return std::next(m_magnitudes.begin(), offset);
}

void Superblock::CpidIndex::Add(const Cpid cpid, const uint16_t magnitude)
{
    if (magnitude > 0 || m_legacy) {
        // Only increment the total magnitude if the CPID does not already
        // exist in the index:
        if (m_magnitudes.emplace(cpid, magnitude).second == true) {
            m_total_magnitude += magnitude;
        }
    } else {
        m_zero_magnitude_count++;
    }
}

void Superblock::CpidIndex::Add(const MiningId id, const uint16_t magnitude)
{
    if (const CpidOption cpid = id.TryCpid()) {
        Add(*cpid, magnitude);
    } else if (!m_legacy) {
        m_zero_magnitude_count++;
    }
}

// -----------------------------------------------------------------------------
// Class: Superblock::ProjectStats
// -----------------------------------------------------------------------------

Superblock::ProjectStats::ProjectStats()
    : m_total_credit(0)
    , m_average_rac(0)
    , m_rac(0)
{
}

Superblock::ProjectStats::ProjectStats(
    uint64_t total_credit,
    uint64_t average_rac,
    uint64_t rac)
    : m_total_credit(total_credit)
    , m_average_rac(average_rac)
    , m_rac(rac)
{
}

Superblock::ProjectStats::ProjectStats(uint64_t average_rac, uint64_t rac)
    : m_total_credit(0)
    , m_average_rac(average_rac)
    , m_rac(rac)
{
}

// -----------------------------------------------------------------------------
// Class: Superblock::ProjectIndex
// -----------------------------------------------------------------------------

Superblock::ProjectIndex::ProjectIndex() : m_total_rac(0)
{
}

Superblock::ProjectIndex::const_iterator Superblock::ProjectIndex::begin() const
{
    return m_projects.begin();
}

Superblock::ProjectIndex::const_iterator Superblock::ProjectIndex::end() const
{
    return m_projects.end();
}

Superblock::ProjectIndex::size_type Superblock::ProjectIndex::size() const
{
    return m_projects.size();
}

bool Superblock::ProjectIndex::empty() const
{
    return m_projects.empty();
}

uint64_t Superblock::ProjectIndex::TotalRac() const
{
    return m_total_rac;
}

double Superblock::ProjectIndex::AverageRac() const
{
    if (m_projects.empty()) {
        return 0;
    }

    return static_cast<double>(m_total_rac) / m_projects.size();
}

Superblock::ProjectStatsOption
Superblock::ProjectIndex::Try(const std::string& name) const
{
    const auto iter = m_projects.find(name);

    if (iter == m_projects.end()) {
        return boost::none;
    }

    return iter->second;
}

void Superblock::ProjectIndex::Add(std::string name, const ProjectStats& stats)
{
    if (name.empty()) {
        return;
    }

    // Only increment the total RAC if the project does not already exist in
    // the index:
    if (m_projects.emplace(std::move(name), stats).second == true) {
        m_total_rac += stats.m_rac;
    }
}

// -----------------------------------------------------------------------------
// Class: QuorumHash
// -----------------------------------------------------------------------------

QuorumHash::QuorumHash() : m_hash(Invalid())
{
}

QuorumHash::QuorumHash(uint256 hash) : m_hash(hash)
{
}

QuorumHash::QuorumHash(Md5Sum legacy_hash) : m_hash(legacy_hash)
{
}

QuorumHash QuorumHash::Hash(const Superblock& superblock)
{
    if (superblock.m_version > 1) {
        return QuorumHash(SerializeHash(superblock));
    }

    std::string input;
    input.reserve(superblock.m_cpids.size() * (32 + 1 + 5 + 5));

    for (const auto& cpid_pair : superblock.m_cpids) {
        double dMagLength = RoundToString(cpid_pair.second, 0).length();
        double dExponent = pow(dMagLength, 5);

        input += cpid_pair.first.ToString();
        input += RoundToString(cpid_pair.second / (dExponent + .01), 0);
        input += RoundToString(dMagLength * dExponent, 0);
        input += "<COL>";
    }

    Md5Sum output;
    MD5((const unsigned char*)input.data(), input.size(), output.data());

    return QuorumHash(output);
}

bool QuorumHash::operator==(const QuorumHash& other) const
{
    if (m_hash.which() != other.m_hash.which()) {
        return false;
    }

    switch (Which()) {
        case Kind::INVALID:
            return true;

        case Kind::SHA256:
            return boost::get<uint256>(m_hash)
                == boost::get<uint256>(other.m_hash);

        case Kind::MD5:
            return boost::get<Md5Sum>(m_hash)
                == boost::get<Md5Sum>(other.m_hash);
    }

    return false;
}

bool QuorumHash::operator!=(const QuorumHash& other) const
{
    return !(*this == other);
}

bool QuorumHash::operator==(const uint256& other) const
{
    return Which() == Kind::SHA256
        && boost::get<uint256>(m_hash) == other;
}

bool QuorumHash::operator!=(const uint256& other) const
{
    return !(*this == other);
}

bool QuorumHash::operator==(const std::string& other) const
{
    switch (Which()) {
        case Kind::INVALID:
            return other.empty();

        case Kind::SHA256:
            return other.size() == 64
                && boost::get<uint256>(m_hash) == uint256(other);

        case Kind::MD5:
            return other.size() == 32
                && std::equal(
                    boost::get<Md5Sum>(m_hash).begin(),
                    boost::get<Md5Sum>(m_hash).end(),
                    ParseHex(other).begin());
    }

    return false;
}

bool QuorumHash::operator!=(const std::string&other) const
{
    return !(*this == other);
}

QuorumHash::Kind QuorumHash::Which() const
{
    return static_cast<Kind>(m_hash.which());
}

bool QuorumHash::Valid() const
{
    return Which() != Kind::INVALID;
}

std::string QuorumHash::ToString() const
{
    return boost::apply_visitor(QuorumHashToStringVisitor(), m_hash);
}
