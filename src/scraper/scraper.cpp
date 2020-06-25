#include "main.h"
#include "block.h"
#include "scraper.h"
#include "scraper_net.h"
#include "http.h"
#include "ui_interface.h"

#include "neuralnet/beacon.h"
#include "neuralnet/project.h"
#include "neuralnet/quorum.h"
#include "neuralnet/superblock.h"

#include <zlib.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/serialization/binary_object.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/exception/exception.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/gregorian/greg_date.hpp>
#include <random>

// These are initialized empty. GetDataDir() cannot be called here. It is too early.
fs::path pathDataDir = {};
fs::path pathScraper = {};

extern bool fShutdown;
extern CWallet* pwalletMain;
bool fScraperActive = false;
std::vector<std::pair<std::string, std::string>> vuserpass;
std::vector<std::pair<std::string, int64_t>> vprojectteamids;
std::vector<std::string> vauthenicationetags;
int64_t ndownloadsize = 0;
int64_t nuploadsize = 0;

enum class logattribute
{
    // Can't use ERROR here because it is defined already in windows.h.
    ERR,
    INFO,
    WARNING,
    CRITICAL
};

struct ScraperFileManifestEntry
{
    std::string filename; // Filename
    std::string project;
    uint256 hash; // hash of file
    int64_t timestamp;
    bool current;
    bool excludefromcsmanifest;
    std::string filetype;
};

typedef std::map<std::string, ScraperFileManifestEntry> ScraperFileManifestMap;

struct ScraperFileManifest
{
    ScraperFileManifestMap mScraperFileManifest;
    uint256 nFileManifestMapHash;
    uint256 nConsensusBlockHash;
    int64_t timestamp;
};

// Both TeamIDMap and ProjTeamETags are protected by cs_TeamIDMap.
// --------------- project -------------team name -- teamID
typedef std::map<std::string, std::map<std::string, int64_t>> mTeamIDs;
mTeamIDs TeamIDMap;

// ProjTeamETags is not persisted to disk. There would be little to be gained by doing so. The scrapers are restarted very
// rarely, and on restart, this would only save downloading team files for those projects that have one or TeamIDs missing AND
// an ETag had NOT changed since the last pull. Not worth the complexity.
// --------------- project ---- eTag
typedef std::map<std::string, std::string> mProjectTeamETags;
mProjectTeamETags ProjTeamETags;


std::vector<std::string> GetTeamWhiteList();

std::string urlsanity(const std::string& s, const std::string& type);
std::string lowercase(std::string s);
std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);
ScraperFileManifest StructScraperFileManifest = {};

// Global cache for converged scraper stats. Access must be with the lock cs_ConvergedScraperStatsCache taken.
ConvergedScraperStats ConvergedScraperStatsCache = {};

CCriticalSection cs_Scraper;
CCriticalSection cs_StructScraperFileManifest;
CCriticalSection cs_ConvergedScraperStatsCache;
CCriticalSection cs_TeamIDMap;
CCriticalSection cs_VerifiedBeacons;

void _log(logattribute eType, const std::string& sCall, const std::string& sMessage);

template<typename T>
void ApplyCache(const std::string& key, T& result);

void ScraperApplyAppCacheEntries();
void Scraper(bool bSingleShot = false);
void ScraperSingleShot();
bool ScraperHousekeeping();
bool UserpassPopulated();
bool ScraperDirectoryAndConfigSanity();
bool StoreBeaconList(const fs::path& file);
bool StoreTeamIDList(const fs::path& file);
bool LoadBeaconList(const fs::path& file, ScraperBeaconMap& mBeaconMap);
bool LoadBeaconListFromConvergedManifest(const ConvergedManifest& StructConvergedManifest, ScraperBeaconMap& mBeaconMap);
bool LoadTeamIDList(const fs::path& file);
std::vector<std::string> split(const std::string& s, const std::string& delim);
uint256 GetmScraperFileManifestHash();
bool StoreScraperFileManifest(const fs::path& file);
bool LoadScraperFileManifest(const fs::path& file);
bool InsertScraperFileManifestEntry(ScraperFileManifestEntry& entry);
unsigned int DeleteScraperFileManifestEntry(ScraperFileManifestEntry& entry);
bool MarkScraperFileManifestEntryNonCurrent(ScraperFileManifestEntry& entry);
void AlignScraperFileManifestEntries(const fs::path& file, const std::string& filetype, const std::string& sProject, const bool& excludefromcsmanifest);
ScraperStatsAndVerifiedBeacons GetScraperStatsByConsensusBeaconList();
ScraperStatsAndVerifiedBeacons GetScraperStatsFromSingleManifest(CScraperManifest &manifest);
bool LoadProjectFileToStatsByCPID(const std::string& project, const fs::path& file, const double& projectmag, const ScraperBeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool LoadProjectObjectToStatsByCPID(const std::string& project, const CSerializeData& ProjectData, const double& projectmag, const ScraperBeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool ProcessProjectStatsFromStreamByCPID(const std::string& project, boostio::filtering_istream& sUncompressedIn,
                                         const double& projectmag, const ScraperBeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool ProcessNetworkWideFromProjectStats(ScraperBeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool StoreStats(const fs::path& file, const ScraperStats& mScraperStats);
bool ScraperSaveCScraperManifestToFiles(uint256 nManifestHash);
bool ScraperSendFileManifestContents(CBitcoinAddress& Address, CKey& Key);
mmCSManifestsBinnedByScraper BinCScraperManifestsByScraper();
mmCSManifestsBinnedByScraper ScraperCullAndBinCScraperManifests();
unsigned int ScraperDeleteUnauthorizedCScraperManifests();
bool ScraperConstructConvergedManifest(ConvergedManifest& StructConvergedManifest);
bool ScraperConstructConvergedManifestByProject(const NN::WhitelistSnapshot& projectWhitelist,
                                                mmCSManifestsBinnedByScraper& mMapCSManifestsBinnedByScraper, ConvergedManifest& StructConvergedManifest);

bool DownloadProjectHostFiles(const NN::WhitelistSnapshot& projectWhitelist);
bool DownloadProjectTeamFiles(const NN::WhitelistSnapshot& projectWhitelist);
bool ProcessProjectTeamFile(const std::string& project, const fs::path& file, const std::string& etag);
bool DownloadProjectRacFilesByCPID(const NN::WhitelistSnapshot& projectWhitelist);
bool ProcessProjectRacFileByCPID(const std::string& project, const fs::path& file, const std::string& etag,  BeaconConsensus& Consensus);
bool AuthenticationETagUpdate(const std::string& project, const std::string& etag);
void AuthenticationETagClear();

extern void MilliSleep(int64_t n);

namespace {
//!
//! \brief Get the block index for the height to consider beacons eligible for
//! rewards.
//!
//! \return Block index for the height of a block about 1 hour below the chain
//! tip.
//!
const CBlockIndex* GetBeaconConsensusHeight()
{
    static BlockFinder block_finder;

    AssertLockHeld(cs_main);

    // Use 4 times the BLOCK_GRANULARITY which moves the consensus block every hour.
    // TODO: Make the mod a function of SCRAPER_CMANIFEST_RETENTION_TIME in scraper.h.
    return block_finder.FindByHeight(
        (nBestHeight - CONSENSUS_LOOKBACK)
            - (nBestHeight - CONSENSUS_LOOKBACK) % (BLOCK_GRANULARITY * 4));
}

//!
//! \brief Get beacon list with consenus.
//!
//! Assembles a list of only active beacons with a consensus lookback from
//! 6 months ago the current tip minus ~1 hour.
//!
//! \return A list of active beacons.
//!
BeaconConsensus GetConsensusBeaconList()
{
    BeaconConsensus consensus;
    std::vector<std::pair<NN::Cpid, NN::Beacon>> beacons;
    std::vector<std::pair<CKeyID, NN::PendingBeacon>> pending_beacons;
    int64_t max_time;

    {
        LOCK(cs_main);

        const CBlockIndex* pMaxConsensusLadder = GetBeaconConsensusHeight();

        consensus.nBlockHash = pMaxConsensusLadder->GetBlockHash();
        max_time = pMaxConsensusLadder->nTime;

        const auto& beacon_registry = NN::GetBeaconRegistry();
        const auto& beacon_map = beacon_registry.Beacons();
        const auto& pending_beacon_map = beacon_registry.PendingBeacons();

        // Copy the set of beacons out of the registry so we can release the
        // lock on cs_main before stringifying them:
        //
        beacons.reserve(beacon_map.size());
        beacons.assign(beacon_map.begin(), beacon_map.end());
        pending_beacons.reserve(pending_beacon_map.size());
        pending_beacons.assign(pending_beacon_map.begin(), pending_beacon_map.end());
    }

    for (const auto& beacon_pair : beacons)
    {
        const NN::Cpid& cpid = beacon_pair.first;
        const NN::Beacon& beacon = beacon_pair.second;

        if (beacon.Expired(max_time) || beacon.m_timestamp >= max_time)
        {
            continue;
        }

        ScraperBeaconEntry beaconentry;

        beaconentry.timestamp = beacon.m_timestamp;
        beaconentry.value = beacon.ToString();

        consensus.mBeaconMap.emplace(cpid.ToString(), std::move(beaconentry));
    }

    for (const auto& pending_beacon_pair : pending_beacons)
    {
        const CKeyID& key_id = pending_beacon_pair.first;
        const NN::PendingBeacon& pending_beacon = pending_beacon_pair.second;

        if (pending_beacon.m_timestamp >= max_time)
        {
            continue;
        }

        ScraperPendingBeaconEntry beaconentry;

        beaconentry.timestamp = pending_beacon.m_timestamp;
        beaconentry.cpid = pending_beacon.m_cpid.ToString();
        beaconentry.key_id = key_id;

        consensus.mPendingMap.emplace(pending_beacon.GetVerificationCode(), std::move(beaconentry));
    }

    return consensus;
}

// A global map for verified beacons. This map is updated by ProcessProjectRacFileByCPID.
// As ProcessProjectRacFileByCPID is called in the loop for each whitelisted projects,
// a single match across any project will inject a record into this map. If multiple
// projects match, the key will match and the [] method is used, so the latest entry will be
// the only one to survive, which is fine. We only need one.

// This map has to be global because the scraper function is reentrant.
ScraperVerifiedBeacons g_verified_beacons;

// Use of this global should be protected by a lock on cs_VerifiedBeacons
ScraperVerifiedBeacons& GetVerifiedBeacons()
{
    // Return global
    return g_verified_beacons;
}

// A lock must be taken on cs_VerifiedBeacons before calling this function.
bool StoreGlobalVerifiedBeacons()
{
    fs::path file = pathScraper / "VerifiedBeacons.dat";

    CAutoFile verified_beacons_file(fsbridge::fopen(file, "wb"), SER_DISK, CLIENT_VERSION);

    ScraperVerifiedBeacons& verified_beacons = GetVerifiedBeacons();

    try
    {
        verified_beacons_file << verified_beacons;
    }
    catch (const std::ios_base::failure& e)
    {
        _log(logattribute::ERR, "StoreGlobalVerifiedBeacons", "Failed to store verified beacons to disk.");
        return false;
    }

    return true;
}

// A lock must be taken on cs_VerifiedBeacons before calling this function.
bool LoadGlobalVerifiedBeacons()
{
    fs::path file = pathScraper / "VerifiedBeacons.dat";

    CAutoFile verified_beacons_file(fsbridge::fopen(file, "rb"), SER_DISK, CLIENT_VERSION);

    ScraperVerifiedBeacons& verified_beacons = GetVerifiedBeacons();

    try
    {
        verified_beacons_file >> verified_beacons;
    }
    catch (const std::ios_base::failure& e)
    {
        _log(logattribute::WARNING, "LoadGlobalVerifiedBeacons", "Failed to load verified beacons from disk.");
        return false;
    }

    verified_beacons.LoadedFromDisk = true;

    return true;
}


// Check to see if any Verified Beacons in the ScraperVerifiedBeacons map have appeared in the active
// beacon map from GetConsensusBeaconList. If so, delete the correponding entry from the verifed beacon
// map. When the superblock is staked and becomes active, the verified beacons are committed to active
// by the staking node, and then when the SB becomes active, any node calling GetConsensusBeaconList
// will then see those beacons as active. Nodes acting as the scraper then need to remove the verified
// beacon entry from the global verified beacon map.

// Also, a beacon will not be removed from the pending beacon map until it is committed to the superblock.
// Therefore also check to ensure that the verified beacon is still on the pending beacon list. If it is
// not, then remove it, because the scraper may have been restarted, and the verified beacons pulled up
// from disk may be stale if the scraper was down for an extended period of time.

// Finally persist the state of the global to disk so that it can be restored on a scraper restart.
void UpdateVerifiedBeaconsFromConsensus(BeaconConsensus& Consensus)
{
    unsigned int now_active = 0;
    unsigned int stale = 0;

    LOCK(cs_VerifiedBeacons);
    _log(logattribute::INFO, "LOCK", "cs_VerifiedBeacons");

    ScraperVerifiedBeacons& ScraperVerifiedBeacons = GetVerifiedBeacons();

    for (auto entry = ScraperVerifiedBeacons.mVerifiedMap.begin(); entry != ScraperVerifiedBeacons.mVerifiedMap.end(); )
    {
        if (Consensus.mBeaconMap.find(entry->second.cpid) != Consensus.mBeaconMap.end())
        {
            entry = ScraperVerifiedBeacons.mVerifiedMap.erase(entry);

            ScraperVerifiedBeacons.timestamp = GetAdjustedTime();

            ++now_active;
        }
        else if (Consensus.mPendingMap.find(entry->first) == Consensus.mPendingMap.end())
        {
            entry = ScraperVerifiedBeacons.mVerifiedMap.erase(entry);

            ScraperVerifiedBeacons.timestamp = GetAdjustedTime();

            ++stale;
        }
        else
        {
            ++entry;
        }
    }

    if (now_active || stale)
    {
        _log(logattribute::INFO, "UpdateVerifiedBeaconsFromConsensus", std::to_string(now_active)
             + " verified beacons now active and removed from verified beacon map. " +
             std::to_string(stale) + " stale verified beacons removed from verfied beacon map.");
    }

    // Store updated verified beacons to disk.
    if (!StoreGlobalVerifiedBeacons())
    {
        _log(logattribute::ERR, "UpdateVerifiedBeaconsFromConsensus", "Verified beacons save to disk failed.");
    }

    _log(logattribute::INFO, "ENDLOCK", "cs_VerifiedBeacons");
}
} // anonymous namespace

/**********************
* Scraper Logger      *
**********************/


class ScraperLogger
{

private:

    static CCriticalSection cs_log;

    static boost::gregorian::date PrevArchiveCheckDate;

    fsbridge::ofstream logfile;

public:

    ScraperLogger()
    {
        LOCK(cs_log);

        fs::path plogfile = pathDataDir / "scraper.log";
        logfile.open(plogfile, std::ios_base::out | std::ios_base::app);

        if (!logfile.is_open())
            LogPrintf("ERROR: Scraper: Logger: Failed to open logging file\n");
    }

    ~ScraperLogger()
    {
        LOCK(cs_log);

        if (logfile.is_open())
        {
            logfile.flush();
            logfile.close();
        }
    }

    void output(const std::string& tofile)
    {
        LOCK(cs_log);

        if (logfile.is_open())
            logfile << tofile << std::endl;

        return;
    }

    void closelogfile()
    {
        LOCK(cs_log);

        if (logfile.is_open())
        {
            logfile.flush();
            logfile.close();
        }
    }

    bool archive(bool fImmediate, fs::path pfile_out)
    {
        bool fArchiveDaily = GetBoolArg("-logarchivedaily", true);

        int64_t nTime = GetAdjustedTime();
        boost::gregorian::date ArchiveCheckDate = boost::posix_time::from_time_t(nTime).date();
        fs::path plogfile;
        fs::path pfile_temp;

        std::stringstream ssArchiveCheckDate, ssPrevArchiveCheckDate;

        ssArchiveCheckDate << ArchiveCheckDate;
        ssPrevArchiveCheckDate << PrevArchiveCheckDate;

        // Goes in main log only and not subject to category.
        LogPrint(BCLog::LogFlags::VERBOSE, "INFO: ScraperLogger: ArchiveCheckDate %s, PrevArchiveCheckDate %s", ssArchiveCheckDate.str(), ssPrevArchiveCheckDate.str());

        fs::path LogArchiveDir = pathDataDir / "logarchive";

        // Check to see if the log archive directory exists and is a directory. If not create it.
        if (fs::exists(LogArchiveDir))
        {
            // If it is a normal file, this is not right. Remove the file and replace with the log archive directory.
            if (fs::is_regular_file(LogArchiveDir))
            {
                fs::remove(LogArchiveDir);
                fs::create_directory(LogArchiveDir);
            }
        }
        else
        {
            fs::create_directory(LogArchiveDir);
        }

        if (fImmediate || (fArchiveDaily && ArchiveCheckDate > PrevArchiveCheckDate))
        {
            {
                LOCK(cs_log);

                if (logfile.is_open())
                {
                    logfile.flush();
                    logfile.close();
                }

                plogfile = pathDataDir / "scraper.log";
                pfile_temp = pathDataDir / ("scraper-" + DateTimeStrFormat("%Y%m%d%H%M%S", nTime) + ".log");
                pfile_out = LogArchiveDir / ("scraper-" + DateTimeStrFormat("%Y%m%d%H%M%S", nTime) + ".log.gz");

                try
                {
                    fs::rename(plogfile, pfile_temp);
                }
                catch(...)
                {
                    LogPrintf("ERROR: ScraperLogger: Failed to rename logging file\n");
                    return false;
                }

                // Re-open log file.
                fs::path plogfile = pathDataDir / "scraper.log";
                logfile.open(plogfile, std::ios_base::out | std::ios_base::app);

                if (!logfile.is_open())
                    LogPrintf("ERROR: ScraperLogger: Failed to open logging file\n");

                PrevArchiveCheckDate = ArchiveCheckDate;
            }

            fsbridge::ifstream infile(pfile_temp, std::ios_base::in | std::ios_base::binary);

            if (!infile)
            {
                LogPrintf("ERROR: ScraperLogger: Failed to open archive log file for compression %s.", pfile_temp.string());
                return false;
            }

            fsbridge::ofstream outgzfile(pfile_out, std::ios_base::out | std::ios_base::binary);

            if (!outgzfile)
            {
                LogPrintf("ERROR: Scraperogger: Failed to open archive gzip file %s.", pfile_out.string());
                return false;
            }

            boostio::filtering_ostream out;
            out.push(boostio::gzip_compressor());
            out.push(outgzfile);

            boost::iostreams::copy(infile, out);

            infile.close();
            outgzfile.flush();
            outgzfile.close();

            fs::remove(pfile_temp);

            bool fDeleteOldLogArchives = GetBoolArg("-deleteoldlogarchives", true);

            if (fDeleteOldLogArchives)
            {
                unsigned int nRetention = (unsigned int)GetArg("-logarchiveretainnumfiles", 30);
                LogPrintf ("INFO: ScraperLogger: nRetention %i.", nRetention);

                std::set<fs::directory_entry, std::greater <fs::directory_entry>> SortedDirEntries;

                // Iterate through the log archive directory and delete the oldest files beyond the retention rule
                // The names are in format scraper-YYYYMMDDHHMMSS for the scraper logs, so filter by containing scraper
                // The greater than sort in the set should then return descending order by datetime.
                for (fs::directory_entry& DirEntry : fs::directory_iterator(LogArchiveDir))
                {
                    std::string sFilename = DirEntry.path().filename().string();
                    size_t FoundPos = sFilename.find("scraper");

                    if (FoundPos != std::string::npos) SortedDirEntries.insert(DirEntry);
                }

                // Now iterate through set of filtered filenames. Delete all files greater than retention count.
                unsigned int i = 0;
                for (auto const& iter : SortedDirEntries)
                {
                    if (i >= nRetention)
                    {
                        fs::remove(iter.path());

                        LogPrintf("INFO: ScraperLogger: Removed old archive gzip file %s.", iter.path().filename().string());
                    }

                    ++i;
                }
            }

            return true;
        }
        else
        {
            return false;
        }
    }
};

ScraperLogger& ScraperLogInstance()
{
    // This is similar to Bitcoin's newer approach.
    static ScraperLogger* scraperlogger{new ScraperLogger()};
    return *scraperlogger;
}

CCriticalSection ScraperLogger::cs_log;
boost::gregorian::date ScraperLogger::PrevArchiveCheckDate = boost::posix_time::from_time_t(GetAdjustedTime()).date();

void _log(logattribute eType, const std::string& sCall, const std::string& sMessage)
{
    std::string sType;
    std::string sOut;

    try
    {
        switch (eType)
        {
        case logattribute::INFO:        sType = "INFO";        break;
        case logattribute::WARNING:     sType = "WARNING";     break;
        case logattribute::ERR:         sType = "ERROR";       break;
        case logattribute::CRITICAL:    sType = "CRITICAL";    break;
        }
    }

    catch (std::exception& ex)
    {
        printf("Logger : exception occurred in _log function (%s)\n", ex.what());

        return;
    }

    sOut = tfm::format("%s [%s] <%s> : %s", DateTimeStrFormat("%x %H:%M:%S", GetAdjustedTime()), sType, sCall, sMessage);

    // This snoops the SCRAPER category setting from the main logger, and uses it as a conditional for the scraper log output.
    // Logs will be posted to the scraper log when the category is set OR it is not an INFO level... (i.e. critical, warning,
    // or error). This has the effect of "turning on" the informational messages to the scraper log when the category is set,
    // and is the equivalent of the old "fDebug3" use for the scraper. Note that no lock is required to call the
    // WillLogCategory, because the underlying type is atomic.
    if (LogInstance().WillLogCategory(BCLog::LogFlags::SCRAPER) || eType != logattribute::INFO)
    {
        ScraperLogger& log = ScraperLogInstance();

        log.output(sOut);
    }

    // Send to UI for log window.
    uiInterface.NotifyScraperEvent(scrapereventtypes::Log, CT_NEW, sOut);

    // Critical, warning, and errors get sent to main debug log regardless of whether category is turned on. Info does not.
    if (eType != logattribute::INFO) LogPrintf(std::string(sType + ": Scraper: <" + sCall + ">: %s").c_str(), sMessage);

    return;
}




/**********************
* String Builder EXP  *
**********************/

class stringbuilder
{
protected:

    std::stringstream builtstring;

public:

    void append(const std::string &value)
    {
        builtstring << value;
    }

    void append(double value)
    {
        builtstring << value;
    }

    void append(int64_t value)
    {
        builtstring << value;
    }

    void fixeddoubleappend(double value, unsigned int precision)
    {
        builtstring << std::fixed << std::setprecision(precision) << value;
    }

    void nlappend(const std::string& value)
    {
        builtstring << value << "\n";
    }

    std::string value()
    {
        // Prevent a memory leak
        const std::string& out = builtstring.str();

        return out;
    }

    size_t size()
    {
        const std::string& out = builtstring.str();
        return out.size();
    }

    void clear()
    {
        builtstring.clear();
        builtstring.str(std::string());
    }
};


/*********************
* Whitelist Data     *
*********************/
int64_t SuperblockAge()
{
    LOCK(cs_main);

    return NN::Quorum::CurrentSuperblock().Age();
}

std::vector<std::string> GetTeamWhiteList()
{
    std::string delimiter;

    // Due to a delimiter changeout from "|" to "<>" we must check to see if "<>" is in use
    // in the protocol string.
    if (TEAM_WHITELIST.find("<>") != std::string::npos)
    {
        delimiter = "<>";
    }
    else
    {
        delimiter = "|";
    }

    return split(TEAM_WHITELIST, delimiter);
}

/*********************
* Userpass Data      *
*********************/

class userpass
{
private:

    fsbridge::ifstream userpassfile;

public:

    userpass()
    {
        vuserpass.clear();

        fs::path plistfile = GetDataDir() / "userpass.dat";

        userpassfile.open(plistfile, std::ios_base::in);

        if (!userpassfile.is_open())
            _log(logattribute::CRITICAL, "userpass_data", "Failed to open userpass file");
    }

    ~userpass()
    {
        if (userpassfile.is_open())
            userpassfile.close();
    }

    bool import()
    {
        vuserpass.clear();
        std::string inputdata;

        try
        {
            while (std::getline(userpassfile, inputdata))
            {
                std::vector<std::string>vlist = split(inputdata, ";");

                vuserpass.push_back(std::make_pair(vlist[0], vlist[1]));
            }

            _log(logattribute::INFO, "userpass_data_import", "Userpass contains " + std::to_string(vuserpass.size()) + " projects");

            return true;
        }

        catch (std::exception& ex)
        {
            _log(logattribute::CRITICAL, "userpass_data_import", "Failed to userpass import due to exception (" + std::string(ex.what()) + ")");

            return false;
        }
    }
};

/*********************
* Auth Data          *
*********************/

class authdata
{
private:

    fsbridge::ofstream oauthdata;
    stringbuilder outdata;

public:

    authdata(const std::string& project)
    {
        std::string outfile = project + "_auth.dat";
        fs::path poutfile = GetDataDir() / outfile;

        oauthdata.open(poutfile, std::ios_base::out | std::ios_base::app);

        if (!oauthdata.is_open())
            _log(logattribute::CRITICAL, "auth_data", "Failed to open auth data file");
    }

    ~authdata()
    {
        if (oauthdata.is_open())
            oauthdata.close();
    }

    void setoutputdata(const std::string& type, const std::string& name, const std::string& etag)
    {
        outdata.clear();
        outdata.append("<auth><etag>");
        outdata.append(etag);
        outdata.append("</etag>");
        outdata.append("<type>");
        outdata.append(type);
        outdata.nlappend("</type></auth>");
    }

    bool xport()
    {
        try
        {
            if (outdata.size() == 0)
            {
                _log(logattribute::CRITICAL, "user_data_export", "No authentication etags to be exported!");

                return false;
            }

            oauthdata.write(outdata.value().c_str(), outdata.size());

            _log(logattribute::INFO, "auth_data_export", "Exported");

            return true;
        }

        catch (std::exception& ex)
        {
            _log(logattribute::CRITICAL, "auth_data_export", "Failed to export auth data due to exception (" + std::string(ex.what()) + ")");

            return false;
        }
    }
};


template<typename T>
void ApplyCache(const std::string& key, T& result)
{
    // Local reference to avoid double lookup.
    const auto& entry = ReadCache(Section::PROTOCOL, key);

    // If the entry has an empty string (no value) then leave the original undisturbed.
    if (entry.value.empty())
        return;

    try
    {
        if (std::is_same<T, double>::value)
            result = stod(entry.value);
        else if (std::is_same<T, int64_t>::value)
            result = atoi64(entry.value);
        else if (std::is_same<T, unsigned int>::value)
            // Throw away (zero out) negative integer
            // This approach limits the range to the non-negative signed int, but that is good enough.
            result = (unsigned int)std::max(0, stoi(entry.value));
        else if (std::is_same<T, bool>::value)
        {
            if (entry.value == "false" || entry.value == "0")
                result = boost::lexical_cast<T>(false);
            else if (entry.value == "true" || entry.value == "1")
                result = boost::lexical_cast<T>(true);
            else
                throw std::invalid_argument("Argument not true or false");
        }
        else
            result = boost::lexical_cast<T>(entry.value);
    }
    catch (const std::exception&)
    {
        _log(logattribute::ERR, "ScraperApplyAppCacheEntries", tfm::format("Ignoring bad AppCache entry for %s.", key));
    }
}


void ScraperApplyAppCacheEntries()
{
    // If there are AppCache entries for the defaults in scraper.h override them. For the first two, this will also
    // override any GetArgs supplied from the command line, which is appropriate as network policy should take precedence.

    ApplyCache("scrapersleep", nScraperSleep);
    ApplyCache("activebeforesb", nActiveBeforeSB);

    ApplyCache("SCRAPER_RETAIN_NONCURRENT_FILES", SCRAPER_RETAIN_NONCURRENT_FILES);
    ApplyCache("SCRAPER_FILE_RETENTION_TIME", SCRAPER_FILE_RETENTION_TIME);
    ApplyCache("EXPLORER_EXTENDED_FILE_RETENTION_TIME", EXPLORER_EXTENDED_FILE_RETENTION_TIME);
    ApplyCache("SCRAPER_CMANIFEST_RETAIN_NONCURRENT", SCRAPER_CMANIFEST_RETAIN_NONCURRENT);
    ApplyCache("SCRAPER_CMANIFEST_RETENTION_TIME", SCRAPER_CMANIFEST_RETENTION_TIME);
    ApplyCache("SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES", SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES);
    ApplyCache("MAG_ROUND", MAG_ROUND);
    ApplyCache("NEURALNETWORKMULTIPLIER", NEURALNETWORKMULTIPLIER);
    ApplyCache("CPID_MAG_LIMIT", CPID_MAG_LIMIT);
    ApplyCache("SCRAPER_CONVERGENCE_MINIMUM", SCRAPER_CONVERGENCE_MINIMUM);
    ApplyCache("SCRAPER_CONVERGENCE_RATIO", SCRAPER_CONVERGENCE_RATIO);
    ApplyCache("CONVERGENCE_BY_PROJECT_RATIO", CONVERGENCE_BY_PROJECT_RATIO);
    ApplyCache("ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD", ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD);
    ApplyCache("SCRAPER_MISBEHAVING_NODE_BANSCORE", SCRAPER_MISBEHAVING_NODE_BANSCORE);
    ApplyCache("REQUIRE_TEAM_WHITELIST_MEMBERSHIP", REQUIRE_TEAM_WHITELIST_MEMBERSHIP);
    ApplyCache("TEAM_WHITELIST", TEAM_WHITELIST);
    ApplyCache("SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD", SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD);

    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "scrapersleep = " + std::to_string(nScraperSleep));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "activebeforesb = " + std::to_string(nActiveBeforeSB));

    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_RETAIN_NONCURRENT_FILES = " + std::to_string(SCRAPER_RETAIN_NONCURRENT_FILES));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_FILE_RETENTION_TIME = " + std::to_string(SCRAPER_FILE_RETENTION_TIME));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "EXPLORER_EXTENDED_FILE_RETENTION_TIME = " + std::to_string(EXPLORER_EXTENDED_FILE_RETENTION_TIME));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_CMANIFEST_RETAIN_NONCURRENT = " + std::to_string(SCRAPER_CMANIFEST_RETAIN_NONCURRENT));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_CMANIFEST_RETENTION_TIME = " + std::to_string(SCRAPER_CMANIFEST_RETENTION_TIME));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES = " + std::to_string(SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "MAG_ROUND = " + std::to_string(MAG_ROUND));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "NEURALNETWORKMULTIPLIER = " + std::to_string(NEURALNETWORKMULTIPLIER));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "CPID_MAG_LIMIT = " + std::to_string(CPID_MAG_LIMIT));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_CONVERGENCE_MINIMUM = " + std::to_string(SCRAPER_CONVERGENCE_MINIMUM));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_CONVERGENCE_RATIO = " + std::to_string(SCRAPER_CONVERGENCE_RATIO));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "CONVERGENCE_BY_PROJECT_RATIO = " + std::to_string(CONVERGENCE_BY_PROJECT_RATIO));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD = " + std::to_string(ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_MISBEHAVING_NODE_BANSCORE = " + std::to_string(SCRAPER_MISBEHAVING_NODE_BANSCORE));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "REQUIRE_TEAM_WHITELIST_MEMBERSHIP = " + std::to_string(REQUIRE_TEAM_WHITELIST_MEMBERSHIP));
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "TEAM_WHITELIST = " + TEAM_WHITELIST);
    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD = " + std::to_string(SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD));

    AppCacheSection mScrapers = ReadCacheSection(Section::SCRAPER);

    _log(logattribute::INFO, "ScraperApplyAppCacheEntries", "For information - authorized scraper address list");
    for (auto const& entry : mScrapers)
    {
        _log(logattribute::INFO, "ScraperApplyAppCacheEntries",
             "Scraper entry: " + entry.first + ", " + entry.second.value + ", " + DateTimeStrFormat("%x %H:%M:%S", entry.second.timestamp));
    }
}



// This is the "main" scraper function.
// It will be instantiated as a separate thread if -scraper is specified as a startup argument,
// and operate in a continuous while loop.
// It can also be called in "single shot" mode.
void Scraper(bool bSingleShot)
{

    // Initialize these while still single-threaded. They cannot be initialized during declaration because GetDataDir()
    // gives the wrong value that early. If they are already initialized then leave them alone (because this function
    // can be called in singleshot mode.
    if (pathDataDir.empty())
    {
        pathDataDir = GetDataDir();
        pathScraper = pathDataDir  / "Scraper";
    }

    // Initialize log singleton. Must be after the imbue.
    ScraperLogInstance();

    if (!bSingleShot)
        _log(logattribute::INFO, "Scraper", "Starting Scraper thread.");
    else
        _log(logattribute::INFO, "Scraper", "Running in single shot mode.");

    uint256 nmScraperFileManifestHash;

    // The scraper thread loop...
    while (!fShutdown)
    {
        // Only proceed if wallet is in sync. Check every 8 seconds since no callback is available.
        // We do NOT want to filter statistics with an out-of-date beacon list or project whitelist.
        // If called in singleshot mode, wallet will most likely be in sync, because the calling functions check
        // beforehand.
        while (OutOfSyncByAge())
        {
            // Set atomic out of sync flag to true.
            fOutOfSyncByAge = true;

            // Signal stats event to UI.
            uiInterface.NotifyScraperEvent(scrapereventtypes::OutOfSync, CT_UPDATING, {});

            _log(logattribute::INFO, "Scraper", "Wallet not in sync. Sleeping for 8 seconds.");
            MilliSleep(8000);
        }

        // Set atomic out of sync flag to false.
        fOutOfSyncByAge = false;

        nSyncTime = GetAdjustedTime();

        // Now that we are in sync, refresh from the AppCache and check for proper directory/file structure.
        // Also delete any unauthorized CScraperManifests received before the wallet was in sync.
        {
            LOCK(cs_Scraper);
            _log(logattribute::INFO, "LOCK", "cs_Scraper");

            ScraperDirectoryAndConfigSanity();

            // UnauthorizedCScraperManifests should only be seen on the first invocation after getting in sync
            // See the comment on the function.

            LOCK(CScraperManifest::cs_mapManifest);
            _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

            ScraperDeleteUnauthorizedCScraperManifests();

            // End LOCK(CScraperManifest::cs_mapManifest)
            _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

            // End LOCK(cs_Scraper)
            _log(logattribute::INFO, "ENDLOCK", "cs_Scraper");
        }

        int64_t sbage = SuperblockAge();
        int64_t nScraperThreadStartTime = GetAdjustedTime();
        CBitcoinAddress AddressOut;
        CKey KeyOut;

        // Give nActiveBeforeSB seconds before superblock needed before we sync
        // Note there is a small while loop here to cull incoming manifests from
        // other scrapers while this one is quiescent. An unlikely but possible
        // situation because the nActiveBeforeSB may be set differently on other
        // scrapers.
        // If Scraper is called in singleshot mode, then skip the wait.
        if (!bSingleShot && sbage <= (86400 - nActiveBeforeSB) && sbage >= 0)
        {
            // Don't let nBeforeSBSleep go less than zero, which could happen without max if wallet
            // started with sbage already older than 86400 - nActiveBeforeSB.
            int64_t nBeforeSBSleep = std::max(86400 - nActiveBeforeSB - sbage, (int64_t) 0);

            while (GetAdjustedTime() - nScraperThreadStartTime < nBeforeSBSleep)
            {
                // Signal stats event to UI.
                uiInterface.NotifyScraperEvent(scrapereventtypes::Sleep, CT_NEW, {});

                // Take a lock on the whole scraper for this...
                {
                    LOCK(cs_Scraper);
                    _log(logattribute::INFO, "LOCK", "cs_Scraper");

                    // The only things we do here while quiescent
                    ScraperDirectoryAndConfigSanity();
                    ScraperCullAndBinCScraperManifests();

                    // End LOCK(cs_Scraper)
                    _log(logattribute::INFO, "ENDLOCK", "cs_Scraper");
                }

                // Need the log archive check here, because we don't run housekeeping in this while loop.
                ScraperLogger& log = ScraperLogInstance();

                fs::path plogfile_out;

                if (log.archive(false, plogfile_out))
                {
                    _log(logattribute::INFO, "Scraper", "Archived scraper.log to " + plogfile_out.filename().string());
                }

                sbage = SuperblockAge();
                _log(logattribute::INFO, "Scraper", "Superblock not needed. age=" + std::to_string(sbage));
                _log(logattribute::INFO, "Scraper", "Sleeping for " + std::to_string(nScraperSleep / 1000) +" seconds");

                MilliSleep(nScraperSleep);
            }
        }

        else if (sbage <= -1)
            _log(logattribute::ERR, "Scraper", "RPC error occurred, check logs");

        // This is the section to download statistics. Only do if authorized.
        else if (IsScraperAuthorized() || IsScraperAuthorizedToBroadcastManifests(AddressOut, KeyOut))
        {
            // Take a lock on cs_Scraper for the main activity portion of the loop.
            LOCK(cs_Scraper);
            _log(logattribute::INFO, "LOCK", "cs_Scraper");

            // Signal stats event to UI.
            uiInterface.NotifyScraperEvent(scrapereventtypes::Stats, CT_UPDATING, {});

            // Get a read-only view of the current project whitelist:
            const NN::WhitelistSnapshot projectWhitelist = NN::GetWhitelist().Snapshot();

            // Delete manifest entries not on whitelist. Take a lock on cs_StructScraperFileManifest for this.
            {
                LOCK(cs_StructScraperFileManifest);
                _log(logattribute::INFO, "LOCK", "download statistics block: cs_StructScraperFileManifest");

                ScraperFileManifestMap::iterator entry;

                for (entry = StructScraperFileManifest.mScraperFileManifest.begin(); entry != StructScraperFileManifest.mScraperFileManifest.end(); )
                {
                    ScraperFileManifestMap::iterator entry_copy = entry++;

                    if (!projectWhitelist.Contains(entry_copy->second.project))
                    {
                        _log(logattribute::INFO, "Scraper", "Removing manifest entry for non-whitelisted project: " + entry_copy->first);
                        DeleteScraperFileManifestEntry(entry_copy->second);
                    }
                }

                // End LOCK(cs_StructScraperFileManifest)
                _log(logattribute::INFO, "ENDLOCK", "download statistics block: cs_StructScraperFileManifest");
            }

            AuthenticationETagClear();

            // Note a lock on cs_StructScraperFileManifest is taken in StoreBeaconList,
            // and the block hash for the consensus block height is updated in the struct.
            if (!StoreBeaconList(pathScraper / "BeaconList.csv.gz"))
                _log(logattribute::ERR, "Scraper", "StoreBeaconList error occurred");
            else
                _log(logattribute::INFO, "Scraper", "Stored Beacon List");

            // If team filtering is set by policy then pull down and retrieve team ID's as needed. This loads the TeamIDMap global.
            // Note that the call(s) to ScraperDirectoryAndConfigSanity() above will preload the team ID map from the persisted file
            // if it exists, so this will minimize the work that DownloadProjectTeamFiles() has to do, unless explorer mode (fExplorer) is true.
            if (REQUIRE_TEAM_WHITELIST_MEMBERSHIP || fExplorer) DownloadProjectTeamFiles(projectWhitelist);

            DownloadProjectRacFilesByCPID(projectWhitelist);

            // If explorer mode is set (fExplorer is true), then download host files. These are currently not use for any other processing,
            // so there is no corresponding Process function for the host files.
            if (fExplorer) DownloadProjectHostFiles(projectWhitelist);

            _log(logattribute::INFO, "Scraper", "download size so far: " + std::to_string(ndownloadsize) + " upload size so far: " + std::to_string(nuploadsize));

            ScraperStats mScraperStats = GetScraperStatsByConsensusBeaconList().mScraperStats;

            _log(logattribute::INFO, "Scraper", "mScraperStats has the following number of elements: " + std::to_string(mScraperStats.size()));

            if (!StoreStats(pathScraper / "Stats.csv.gz", mScraperStats))
                _log(logattribute::ERR, "Scraper", "StoreStats error occurred");
            else
                _log(logattribute::INFO, "Scraper", "Stored stats.");

            // Signal stats event to UI.
            uiInterface.NotifyScraperEvent(scrapereventtypes::Stats, CT_NEW, {});

            // End LOCK(cs_Scraper)
            _log(logattribute::INFO, "ENDLOCK", "cs_Scraper");
        }

        // This is the section to send out manifests. Only do if authorized.
        if (IsScraperAuthorizedToBroadcastManifests(AddressOut, KeyOut))
        {
            // This will push out a new CScraperManifest if the hash has changed of mScraperFileManifestHash.
            // The idea here is to run the scraper loop a number of times over a period of time approaching
            // the due time for the superblock. A number of stats files only update once per day, but a few update
            // as often as once per hour. If one of these file changes during the run up to the superblock, a
            // new manifest should be published, but the older ones retained up to the SCRAPER_CMANIFEST_RETENTION_TIME limit
            // to provide additional matching options.

            // Publish and/or local delete CScraperManifests.
            {
                LOCK2(cs_StructScraperFileManifest, CScraperManifest::cs_mapManifest);
                _log(logattribute::INFO, "LOCK2", "manifest send block: cs_StructScraperFileManifest, CScraperManifest::cs_mapManifest");

                // If the hash doesn't match (a new one is available), or there are none, then publish a new one.
                if (nmScraperFileManifestHash != StructScraperFileManifest.nFileManifestMapHash
                        || !CScraperManifest::mapManifest.size())
                {
                    _log(logattribute::INFO, "Scraper", "Publishing new CScraperManifest.");

                    // scraper key used for signing is the key returned by IsScraperAuthorizedToBroadcastManifests(KeyOut).
                    if (ScraperSendFileManifestContents(AddressOut, KeyOut))
                        uiInterface.NotifyScraperEvent(scrapereventtypes::Manifest, CT_NEW, {});
                }

                nmScraperFileManifestHash = StructScraperFileManifest.nFileManifestMapHash;

                // End LOCK(cs_StructScraperFileManifest)
                _log(logattribute::INFO, "ENDLOCK2", "manifest send block: cs_StructScraperFileManifest, CScraperManifest::cs_mapManifest");
            }
        }

        // Don't do this if called with singleshot, because ScraperGetSuperblockContract will be done afterwards by
        // the function that called the singleshot.
        if (!bSingleShot)
        {
            ScraperHousekeeping();

            _log(logattribute::INFO, "Scraper", "Sleeping for " + std::to_string(nScraperSleep / 1000) +" seconds");
            MilliSleep(nScraperSleep);
        }
        else
            // This will break from the outer while loop if in singleshot mode and end execution after one pass.
            // otherwise in a continuous while loop with nScraperSleep between iterations.
            break;
    }
}


void ScraperSingleShot()
{
    // Going through Scraper function in single shot mode.

    _log(logattribute::INFO, "ScraperSingleShot", "Calling Scraper function in single shot mode.");

    Scraper(true);
}


// This is the non-scraper "neural-network" node thread...
void NeuralNetwork()
{
    // Initialize these while still single-threaded. They cannot be initialized during declaration because GetDataDir()
    // gives the wrong value that early. Don't initialize here if the scraper thread is running, or if already initialized.
    if (!fScraperActive && pathDataDir.empty())
    {
        pathDataDir = GetDataDir();
        pathScraper = pathDataDir  / "Scraper";

        // Initialize log singleton. Must be after the imbue.
        ScraperLogInstance();
    }


    _log(logattribute::INFO, "NeuralNetwork", "Starting Neural Network housekeeping thread (new C++ implementation). \n"
                                              "Note that this does NOT mean the NN is active. This simply does housekeeping "
                                              "functions.");

    while(!fShutdown)
    {
        // Only proceed if wallet is in sync. Check every 8 seconds since no callback is available.
        // We do NOT want to filter statistics with an out-of-date beacon list or project whitelist.
        while (OutOfSyncByAge())
        {
            // Set atomic out of sync flag to true.
            fOutOfSyncByAge = true;

            // Signal stats event to UI.
            uiInterface.NotifyScraperEvent(scrapereventtypes::OutOfSync, CT_NEW, {});

            _log(logattribute::INFO, "NeuralNetwork", "Wallet not in sync. Sleeping for 8 seconds.");
            MilliSleep(8000);
        }

        // Set atomic out of sync flag to false.
        fOutOfSyncByAge = false;

        nSyncTime = GetAdjustedTime();

        // ScraperHousekeeping items are only run in this thread if not handled by the Scraper() thread.
        if (!fScraperActive)
        {
            LOCK(cs_Scraper);
            _log(logattribute::INFO, "LOCK", "cs_Scraper");

            ScraperDirectoryAndConfigSanity();
            // UnauthorizedCScraperManifests should only be seen on the first invocation after getting in sync
            // See the comment on the function.

            LOCK(CScraperManifest::cs_mapManifest);
            _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

            ScraperDeleteUnauthorizedCScraperManifests();

            // END LOCK(CScraperManifest::cs_mapManifest)
            _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

            ScraperHousekeeping();

            // END LOCK(cs_Scraper)
            _log(logattribute::INFO, "ENDLOCK", "cs_Scraper");
        }

        // Use the same sleep interval configured for the scraper.
        _log(logattribute::INFO, "NeuralNetwork", "Sleeping for " + std::to_string(nScraperSleep / 1000) +" seconds");

        MilliSleep(nScraperSleep);
    }
}

UniValue testnewsb(const UniValue& params, bool fHelp);

bool ScraperHousekeeping()
{
    // Periodically generate converged manifests and generate SB core and "contract"
    // This will probably be reduced to the commented out call as we near final testing,
    // because ScraperGetSuperblockContract(false) is called from the neuralnet native interface
    // with the boolean false, meaning don't store the stats.
    // Lock both cs_Scraper and cs_StructScraperFileManifest.

    NN::Superblock superblock;

    {
        LOCK2(cs_Scraper, cs_StructScraperFileManifest);

        superblock = ScraperGetSuperblockContract(true, false);
    }

    {
        LOCK(CScraperManifest::cs_mapManifest);

        unsigned int nPendingDeleted = 0;

        _log(logattribute::INFO, "ScraperHousekeeping", "Size of mapPendingDeletedManifest before delete = "
             + std::to_string(CScraperManifest::mapPendingDeletedManifest.size()));

        // Make sure deleted manifests pending permanent deletion are culled.
        nPendingDeleted = CScraperManifest::DeletePendingDeletedManifests();
        _log(logattribute::INFO, "ScraperHousekeeping", "Permanently deleted " + std::to_string(nPendingDeleted) + " manifest(s) pending permanent deletion.");
        _log(logattribute::INFO, "ScraperHousekeeping", "Size of mapPendingDeletedManifest after delete = "
             + std::to_string(CScraperManifest::mapPendingDeletedManifest.size()));
    }

    if (LogInstance().WillLogCategory(BCLog::LogFlags::SCRAPER) && superblock.WellFormed())
    {
        UniValue input_params(UniValue::VARR);

        // Set hint bits to 3 for testnet and 5 for mainnet to force collisions for testing.
        int nReducedCacheBits;

        nReducedCacheBits = fTestNet ?  3 : 5;

        input_params.push_back(nReducedCacheBits);
        testnewsb(input_params, false);
    }

    // Show this node's contract hash in the log.
    _log(logattribute::INFO, "ScraperHousekeeping", "superblock contract hash = " + superblock.GetHash().ToString());

    ScraperLogger& log = ScraperLogInstance();

    fs::path plogfile_out;

    if (log.archive(false, plogfile_out))
        _log(logattribute::INFO, "ScraperHousekeeping", "Archived scraper.log to " + plogfile_out.filename().string());

    //log.closelogfile();

    return true;
}


std::string lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    return s;
}

// A lock on cs_Scraper should be taken before calling this function.
bool ScraperDirectoryAndConfigSanity()
{
    ScraperApplyAppCacheEntries();

    // Check to see if the Scraper directory exists and is a directory. If not create it.
    if (fs::exists(pathScraper))
    {
        // If it is a normal file, this is not right. Remove the file and replace with the Scraper directory.
        if (fs::is_regular_file(pathScraper))
        {
            fs::remove(pathScraper);
            fs::create_directory(pathScraper);
        }
        // Only do the file manifest to directory alignments if the scraper is active.
        else if (fScraperActive)
        {
            // Load the manifest file from the Scraper directory into mScraperFileManifest, if mScraperFileManifest is empty.
            // Lock the manifest while it is being manipulated.
            {
                LOCK(cs_StructScraperFileManifest);
                _log(logattribute::INFO, "LOCK", "align directory with manifest file: cs_StructScraperFileManifest");

                if (StructScraperFileManifest.mScraperFileManifest.empty())
                {
                    _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Loading Manifest");
                    if (!LoadScraperFileManifest(pathScraper / "Manifest.csv.gz"))
                        _log(logattribute::ERR, "ScraperDirectoryAndConfigSanity", "Error occurred loading manifest");
                    else
                        _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Loaded Manifest file into map.");
                }

                // Align the Scraper directory with the Manifest file.
                // First remove orphan files with no Manifest entry.
                // Check to see if the file exists in the manifest and if the hash matches. If it doesn't
                // remove it.
                ScraperFileManifestMap::iterator entry;

                for (fs::directory_entry& dir : fs::directory_iterator(pathScraper))
                {
                    std::string filename = dir.path().filename().string();

                    if (dir.path().filename() != "Manifest.csv.gz"
                            && dir.path().filename() != "BeaconList.csv.gz"
                            && dir.path().filename() != "Stats.csv.gz"
                            && dir.path().filename() != "ConvergedStats.csv.gz"
                            && dir.path().filename() != "TeamIDs.csv.gz"
                            && dir.path().filename() != "VerifiedBeacons.dat"
                            && fs::is_regular_file(dir))
                    {
                        entry = StructScraperFileManifest.mScraperFileManifest.find(dir.path().filename().string());
                        
                        if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY))
                        {
                            _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Iterating through directory - checking file " + filename);
                        }
                        
                        if (entry == StructScraperFileManifest.mScraperFileManifest.end())
                        {
                            fs::remove(dir.path());
                            _log(logattribute::WARNING, "ScraperDirectoryAndConfigSanity", "Removing orphan file not in Manifest: " + filename);
                            continue;
                        }

                        // Only do the expensive hash checking on files that are included in published manifests.
                        if (!entry->second.excludefromcsmanifest)
                        {
                            if (entry->second.hash != GetFileHash(dir))
                            {
                                _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "File failed hash check. Removing file.");
                                fs::remove(dir.path());
                            }
                        }
                    }
                }

                // Now iterate through the Manifest map and remove entries with no file, or entries and files older than
                // nRetentionTime, whether they are current or not, and remove non-current files regardless of time
                //if fScraperRetainNonCurrentFiles is false.
                for (entry = StructScraperFileManifest.mScraperFileManifest.begin(); entry != StructScraperFileManifest.mScraperFileManifest.end(); )
                {
                    ScraperFileManifestMap::iterator entry_copy = entry++;

                    int64_t nFileRetentionTime = fExplorer ? EXPLORER_EXTENDED_FILE_RETENTION_TIME : SCRAPER_FILE_RETENTION_TIME;
                    
                    if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY))
                    {
                        _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Iterating through map - checking map entry " + entry_copy->first);
                    }

                    if (!fs::exists(pathScraper / entry_copy->first)
                            || ((GetAdjustedTime() - entry_copy->second.timestamp) > nFileRetentionTime)
                            || (!SCRAPER_RETAIN_NONCURRENT_FILES && entry_copy->second.current == false))
                    {
                        _log(logattribute::WARNING, "ScraperDirectoryAndConfigSanity", "Removing stale or orphan manifest entry: " + entry_copy->first);
                        DeleteScraperFileManifestEntry(entry_copy->second);
                    }
                }

                // End LOCK(cs_StructScraperFileManifest)
                _log(logattribute::INFO, "ENDLOCK", "align directory with manifest file: cs_StructScraperFileManifest");
            }

            // If network policy is set to filter on whitelisted teams, then load team ID map from file. This will prevent the heavyweight
            // team file downloads for projects whose team ID's have already been found and stored, unless explorer mode (fExplorer) is true.
            if (REQUIRE_TEAM_WHITELIST_MEMBERSHIP)
            {
                LOCK(cs_TeamIDMap);
                _log(logattribute::INFO, "LOCK", "cs_TeamIDMap");

                if (TeamIDMap.empty())
                {
                    _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Loading team IDs");
                    if (!LoadTeamIDList(pathScraper / "TeamIDs.csv.gz"))
                        _log(logattribute::WARNING, "ScraperDirectoryAndConfigSanity", "Unable to load team IDs. This is normal for first time startup.");
                    else
                    {
                        _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Loaded team IDs file into map.");
                        if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY))
                        {
                            _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "TeamIDMap contents:");
                            for (const auto& iter : TeamIDMap)
                            {
                                _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity", "Project = " + iter.first);
                                for (const auto& iter2 : iter.second)
                                {
                                    _log(logattribute::INFO, "ScraperDirectoryAndConfigSanity",
                                         "Team = " + iter2.first + ", TeamID = " + std::to_string(iter2.second));
                                }
                            }
                        }
                    }
                }

                _log(logattribute::INFO, "ENDLOCK", "cs_TeamIDMap");
            }

            // If the verified beacons global has not been loaded from disk, then load it.
            // Log a warning if unsuccessful.
            {
                LOCK(cs_VerifiedBeacons);
                _log(logattribute::INFO, "LOCK", "cs_VerifiedBeacons");

                if (!GetVerifiedBeacons().LoadedFromDisk && !LoadGlobalVerifiedBeacons())
                {
                    _log(logattribute::WARNING, "ScraperDirectoryAndConfigSanity", "Initial verified beacon load from file failed. "
                                                        "This is not necessarily a problem.");
                }

                _log(logattribute::INFO, "ENDLOCK", "cs_VerifiedBeacons");
            }
        } // if fScraperActive
    }
    else
    {
        fs::create_directory(pathScraper);
    }

    return true;
}

void AuthenticationETagClear()
{
    fs::path file = fs::current_path() / "auth.dat";

    if (fs::exists(file))
        fs::remove(file);
}

/**********************
* Populate UserPass   *
**********************/

bool UserpassPopulated()
{
    if (vuserpass.empty())
    {
        _log(logattribute::INFO, "UserpassPopulated", "Userpass vector currently empty; populating");

        userpass up;

        if (up.import())
            _log(logattribute::INFO, "UserPassPopulated", "Successfully populated userpass vector");

        else
        {
            _log(logattribute::CRITICAL, "UserPassPopulated", "Failed to populate userpass vector");

            return false;
        }
    }

    _log(logattribute::INFO, "UserPassPopulated", "Userpass is populated; Contains " + std::to_string(vuserpass.size()) + " projects");

    return true;
}





/**********************
* Project Host Files  *
**********************/

bool DownloadProjectHostFiles(const NN::WhitelistSnapshot& projectWhitelist)
{
    // If fExplorer is false then skip processing. (This should not be called anyway, but return immediately just in case.
    if (!fExplorer)
    {
        _log(logattribute::INFO, "DownloadProjectHostFiles", "Not in explorer mode. Skipping host file download and processing.");
        return false;
    }

    if (!projectWhitelist.Populated())
    {
        _log(logattribute::CRITICAL, "DownloadProjectHostFiles", "Whitelist is not populated");

        return false;
    }

    _log(logattribute::INFO, "DownloadProjectHostFiles", "Whitelist is populated; Contains " + std::to_string(projectWhitelist.size()) + " projects");

    if (!UserpassPopulated())
    {
        _log(logattribute::CRITICAL, "DownloadProjectHostFiles", "Userpass is not populated");

        return false;
    }

    for (const auto& prjs : projectWhitelist)
    {
        _log(logattribute::INFO, "DownloadProjectHostFiles", "Downloading project host file for " + prjs.m_name);

        // Grab ETag of host file
        Http http;
        std::string sHostETag;

        bool buserpass = false;
        std::string userpass;

        for (const auto& up : vuserpass)
        {
            if (up.first == prjs.m_name)
            {
                buserpass = true;

                userpass = up.second;

                break;
            }
        }

        try
        {
            sHostETag = http.GetEtag(prjs.StatsUrl("host"), userpass);
        }
        catch (const std::runtime_error& e)
        {
            _log(logattribute::ERR, "DownloadProjectHostFiles", "Failed to pull host header file for " + prjs.m_name);
            continue;
        }

        if (sHostETag.empty())
        {
            _log(logattribute::ERR, "DownloadProjectHostFiles", "ETag for project is empty" + prjs.m_name);

            continue;
        }
        else
            _log(logattribute::INFO, "DownloadProjectHostFiles", "Successfully pulled host header file for " + prjs.m_name);

        if (buserpass)
        {
            authdata ad(lowercase(prjs.m_name));

            ad.setoutputdata("host", prjs.m_name, sHostETag);

            if (!ad.xport())
                _log(logattribute::CRITICAL, "DownloadProjectHostFiles", "Failed to export etag for " + prjs.m_name + " to authentication file");
        }

        std::string host_file_name;
        fs::path host_file;

        // Use eTag versioning.
        host_file_name = prjs.m_name + "-" + sHostETag + "-host.gz";
        host_file = pathScraper / host_file_name;

        // If the file with the same eTag already exists, don't download it again.
        if (fs::exists(host_file))
        {
            _log(logattribute::INFO, "DownloadProjectHostFiles", "Etag file for " + prjs.m_name + " already exists");
            continue;
        }

        try
        {
            http.Download(prjs.StatsUrl("host"), host_file, userpass);
        }
        catch(const std::runtime_error& e)
        {
            _log(logattribute::ERR, "DownloadProjectHostFiles", "Failed to download project host file for " + prjs.m_name);
            continue;
        }

        // Save host xml files to file manifest map with exclude from CSManifest flag set to true.
        AlignScraperFileManifestEntries(host_file, "host", prjs.m_name, true);
    }

    return true;
}





/**********************
* Project Team Files  *
**********************/

bool DownloadProjectTeamFiles(const NN::WhitelistSnapshot& projectWhitelist)
{
    if (!projectWhitelist.Populated())
    {
        _log(logattribute::CRITICAL, "DownloadProjectTeamFiles", "Whitelist is not populated");

        return false;
    }

    _log(logattribute::INFO, "DownloadProjectTeamFiles", "Whitelist is populated; Contains " + std::to_string(projectWhitelist.size()) + " projects");

    if (!UserpassPopulated())
    {
        _log(logattribute::CRITICAL, "DownloadProjectTeamFiles", "Userpass is not populated");

        return false;
    }

    for (const auto& prjs : projectWhitelist)
    {
        LOCK(cs_TeamIDMap);
        _log(logattribute::INFO, "LOCK", "cs_TeamIDMap");

        const auto iter = TeamIDMap.find(prjs.m_name);
        bool fProjTeamIDsMissing = false;

        if (iter == TeamIDMap.end() || iter->second.size() != GetTeamWhiteList().size()) fProjTeamIDsMissing = true;

        // If fExplorer is false, which means we do not need to retain team files, and there are no TeamID entries missing,
        // then skip processing altogether.
        if (!fExplorer && !fProjTeamIDsMissing)
        {
            _log(logattribute::INFO, "DownloadProjectTeamFiles", "Correct team whitelist entries already in the team ID map for "
                 + prjs.m_name + " project. Skipping team file download and processing.");
            continue;
        }

        _log(logattribute::INFO, "DownloadProjectTeamFiles", "Downloading project file for " + prjs.m_name);

        // Grab ETag of team file
        Http http;
        std::string sTeamETag;

        bool buserpass = false;
        std::string userpass;

        for (const auto& up : vuserpass)
        {
            if (up.first == prjs.m_name)
            {
                buserpass = true;

                userpass = up.second;

                break;
            }
        }

        try
        {
            sTeamETag = http.GetEtag(prjs.StatsUrl("team"), userpass);
        }
        catch (const std::runtime_error& e)
        {
            _log(logattribute::ERR, "DownloadProjectTeamFiles", "Failed to pull team header file for " + prjs.m_name);
            continue;
        }

        if (sTeamETag.empty())
        {
            _log(logattribute::ERR, "DownloadProjectTeamFiles", "ETag for project is empty" + prjs.m_name);

            continue;
        }
        else
            _log(logattribute::INFO, "DownloadProjectTeamFiles", "Successfully pulled team header file for " + prjs.m_name);

        if (buserpass)
        {
            authdata ad(lowercase(prjs.m_name));

            ad.setoutputdata("team", prjs.m_name, sTeamETag);

            if (!ad.xport())
                _log(logattribute::CRITICAL, "DownloadProjectTeamFiles", "Failed to export etag for " + prjs.m_name + " to authentication file");
        }

        std::string team_file_name;
        fs::path team_file;
        bool bDownloadFlag = false;
        bool bETagChanged = false;

        // Detect change in ETag from in memory versioning.
        // ProjTeamETags is not persisted to disk. There would be little to be gained by doing so. The scrapers are restarted very
        // rarely, and on restart, this would only save downloading team files for those projects that have one or TeamIDs missing AND
        // an ETag had NOT changed since the last pull. Not worth the complexity.
        auto const& iPrevETag = ProjTeamETags.find(prjs.m_name);

        if (iPrevETag == ProjTeamETags.end() || iPrevETag->second != sTeamETag)
        {
            bETagChanged  = true;

            _log(logattribute::INFO, "DownloadProjectTeamFiles", "Team header file ETag has changed for " + prjs.m_name);
        }


        if (fExplorer)
        {
            // Use eTag versioning ON THE DISK with eTag versioned team files per project.
            team_file_name = prjs.m_name + "-" + sTeamETag + "-team.gz";
            team_file = pathScraper / team_file_name;

            // If the file with the same eTag already exists, don't download it again. Leave bDownloadFlag false.
            if (fs::exists(team_file))
            {
                _log(logattribute::INFO, "DownloadProjectTeamFiles", "Etag file for " + prjs.m_name + " already exists");
                 // continue;
            }
            else
            {
                bDownloadFlag = true;
            }
        }
        else
        {
            // Not in explorer mode...
            // No versioning ON THE DISK for the individual team files for a given project. However, if the eTag pulled from the header
            // does not match the entry in ProjTeamETags, then download the file and process. Note that this combined with the size check
            // above means that the size of the inner map for the mTeamIDs for this project already doesn't match, which means either there
            // were teams that cannot be associated (-1 entries in the file), or there was an addition to or deletion from the team
            // whitelist. Either way if the ETag has changed under this condition, the -1 entries may be subject to change so the team file
            // must be downloaded and processed to see if it has and update. If the ETag matches what was in the map, then the state
            // has not changed since the team file was last processed, and no need to download and process again.
            team_file_name = prjs.m_name + "-team.gz";
            team_file = pathScraper / team_file_name;

            if (bETagChanged)
            {
                if (fs::exists(team_file)) fs::remove(team_file);

                bDownloadFlag = true;
            }
        }

        // If a new team file at the project site is detected, then download new file. (I.e. bDownload flag is true).
        if (bDownloadFlag)
        {
            try
            {
                http.Download(prjs.StatsUrl("team"), team_file, userpass);
            }
            catch(const std::runtime_error& e)
            {
                _log(logattribute::ERR, "DownloadProjectTeamFiles", "Failed to download project team file for " + prjs.m_name);
                continue;
            }
        }

        // If in explorer mode and new file downloaded, save team xml files to file manifest map with exclude from CSManifest flag set to true.
        // If not in explorer mode, this is not necessary, because the team xml file is just temporary and can be discarded after
        // processing.
        if (fExplorer && bDownloadFlag) AlignScraperFileManifestEntries(team_file, "team", prjs.m_name, true);

        // If require team whitelist is set and bETagChanged is true, then process the file. This also populates/updated the team whitelist TeamIDs
        // in the TeamIDMap and the ETag entries in the ProjTeamETags map.
        if (REQUIRE_TEAM_WHITELIST_MEMBERSHIP && bETagChanged) ProcessProjectTeamFile(prjs.m_name, team_file, sTeamETag);

        _log(logattribute::INFO, "ENDLOCK", "cs_TeamIDMap");
    }

    return true;
}


// Note this should be called with a lock held on cs_TeamIDMap, which is intended to protect both
// TeamIDMap and ProjTeamETags.
bool ProcessProjectTeamFile(const std::string& project, const fs::path& file, const std::string& etag)
{
    std::map<std::string, int64_t> mTeamIdsForProject;

    // If passed an empty file, immediately return false.
    if (file.string().empty())
        return false;

    fsbridge::ifstream ingzfile(file, std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(logattribute::ERR, "ProcessProjectTeamFile", "Failed to open team gzip file (" + file.filename().string() + ")");

        return false;
    }

    _log(logattribute::INFO, "ProcessProjectTeamFile", "Opening team file (" + file.filename().string() + ")");

    boostio::filtering_istream in;

    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    _log(logattribute::INFO, "ProcessProjectTeamFile", "Started processing " + file.filename().string());

    std::vector<std::string> vTeamWhiteList = GetTeamWhiteList();

    std::string line;
    stringbuilder builder;

    while (std::getline(in, line))
    {
        if (line == "<team>")
            builder.clear();
        else if (line == "</team>")
        {
            const std::string& data = builder.value();
            builder.clear();

            const std::string& sTeamID = ExtractXML(data, "<id>", "</id>");
            const std::string& sTeamName = ExtractXML(data, "<name>", "</name>");

            // See if the team name is in the team whitelist.
            auto iter = find(vTeamWhiteList.begin(), vTeamWhiteList.end(), sTeamName);
            // If it is not continue on to next team.
            if (iter == vTeamWhiteList.end())
                continue;

            int64_t nTeamID = 0;

            try
            {
                nTeamID = atoi64(sTeamID);
            }
            catch (const std::exception&)
            {
                _log(logattribute::ERR, "ProccessProjectTeamFile", tfm::format("Ignoring bad team id for team %s.", sTeamName));
                continue;
            }

            mTeamIdsForProject[sTeamName] = nTeamID;
        }
        else
            builder.append(line);
    }

    if (mTeamIdsForProject.empty())
    {
        _log(logattribute::CRITICAL, "ProcessProjectTeamFile", "Error in data processing of " + file.string());

        ingzfile.close();

        if (fs::exists(file)) fs::remove(file);

        return false;
    }

    ingzfile.close();

    // Insert or update team IDs for the project into the team ID map. This must be done before the StoreTeamIDList.
    TeamIDMap[project] = mTeamIdsForProject;

    // Populate/update ProjTeamETags with the eTag to provide in memory versioning.
    ProjTeamETags[project] = etag;

    if (mTeamIdsForProject.size() < vTeamWhiteList.size())
        _log(logattribute::WARNING, "ProcessProjectTeamFile", "Unable to determine team IDs for one or more whitelisted teams. This is not necessarily an error.");

    // The below is not an ideal implementation, because the entire map is going to be written out to disk each time.
    // The TeamIDs file is actually very small though, and this primitive implementation will suffice.
    _log(logattribute::INFO, "ProcessProjectTeamFile", "Persisting Team ID entries to disk.");
    if (!StoreTeamIDList(pathScraper / "TeamIDs.csv.gz"))
        _log(logattribute::ERR, "ProcessProjectTeamFile", "StoreTeamIDList error occurred.");
    else
        _log(logattribute::INFO, "ProcessProjectTeamFile", "Stored Team ID entries.");

    // If not explorer mode, delete input file after processing.
    if (!fExplorer && fs::exists(file)) fs::remove(file);

    _log(logattribute::INFO, "ProcessProjectTeamFile", "Finished processing " + file.filename().string());

    return true;
}


/**********************
* Project RAC Files   *
**********************/

bool DownloadProjectRacFilesByCPID(const NN::WhitelistSnapshot& projectWhitelist)
{
    if (!projectWhitelist.Populated())
    {
        _log(logattribute::CRITICAL, "DownloadProjectRacFiles", "Whitelist is not populated");

        return false;
    }

    _log(logattribute::INFO, "DownloadProjectRacFiles", "Whitelist is populated; Contains " + std::to_string(projectWhitelist.size()) + " projects");

    if (!UserpassPopulated())
    {
        _log(logattribute::CRITICAL, "DownloadProjectRacFiles", "Userpass is not populated");

        return false;
    }

    // Get a consensus map of Beacons.
    BeaconConsensus Consensus = GetConsensusBeaconList();
    _log(logattribute::INFO, "DownloadProjectRacFiles", "Getting consensus map of Beacons.");

    // Check if any verified beacons are now active, and if so, remove from the ScraperVerifiedBeacons map.
    UpdateVerifiedBeaconsFromConsensus(Consensus);

    if (LogInstance().WillLogCategory(BCLog::LogFlags::SCRAPER))
    {
        for (const auto& entry : Consensus.mPendingMap)
        {
            _log(logattribute::INFO, "DownloadProjectRacFilesByCPID", "Pending beacon verification code "
                 + entry.first + ", CPID " + entry.second.cpid + ", "
                 + DateTimeStrFormat("%Y%m%d%H%M%S", entry.second.timestamp));
        }
    }

    for (const auto& prjs : projectWhitelist)
    {
        _log(logattribute::INFO, "DownloadProjectRacFiles", "Downloading project file for " + prjs.m_name);

        // Grab ETag of rac file
        Http http;
        std::string sRacETag;

        bool buserpass = false;
        std::string userpass;

        for (const auto& up : vuserpass)
        {
            if (up.first == prjs.m_name)
            {
                buserpass = true;

                userpass = up.second;

                break;
            }
        }

        try
        {
            sRacETag = http.GetEtag(prjs.StatsUrl("user"), userpass);
        } catch (const std::runtime_error& e)
        {
            _log(logattribute::ERR, "DownloadProjectRacFiles", "Failed to pull rac header file for " + prjs.m_name);
            continue;
        }

        if (sRacETag.empty())
        {
            _log(logattribute::ERR, "DownloadProjectRacFiles", "ETag for project is empty" + prjs.m_name);

            continue;
        }

        else
            _log(logattribute::INFO, "DownloadProjectRacFiles", "Successfully pulled rac header file for " + prjs.m_name);

        if (buserpass)
        {
            authdata ad(lowercase(prjs.m_name));

            ad.setoutputdata("user", prjs.m_name, sRacETag);

            if (!ad.xport())
                _log(logattribute::CRITICAL, "DownloadProjectRacFiles", "Failed to export etag for " + prjs.m_name + " to authentication file");
        }

        std::string rac_file_name;
        fs::path rac_file;

        std::string processed_rac_file_name;
        fs::path processed_rac_file;

        processed_rac_file_name = prjs.m_name + "-" + sRacETag + ".csv" + ".gz";
        processed_rac_file = pathScraper / processed_rac_file_name;

        if (fExplorer)
        {
            // Use eTag versioning for source file.
            rac_file_name = prjs.m_name + "-" + sRacETag + "-user.gz";
            rac_file = pathScraper / rac_file_name;

            //  If the file was already processed, both should be here. If both here, skip processing.
            if (fs::exists(rac_file) && fs::exists(processed_rac_file))
            {
                _log(logattribute::INFO, "DownloadProjectRacFiles", "Etag file for " + prjs.m_name + " already exists");
                continue;
            }
        }
        else
        {
            // No versioning for source file. If file exists delete it and download anew, unless processed file already present.
            rac_file_name = prjs.m_name + "-user.gz";
            rac_file = pathScraper / rac_file_name;

            if (fs::exists(rac_file)) fs::remove(rac_file);

            //  If the file was already processed, skip processing.
            if (fs::exists(processed_rac_file))
            {
                _log(logattribute::INFO, "DownloadProjectRacFiles", "Etag file for " + prjs.m_name + " already exists");
                continue;
            }
        }

        try
        {
            http.Download(prjs.StatsUrl("user"), rac_file, userpass);
        }
        catch(const std::runtime_error& e)
        {
            _log(logattribute::ERR, "DownloadProjectRacFiles", "Failed to download project rac file for " + prjs.m_name);
            continue;
        }

        // If in explorer mode, save user (rac) source xml files to file manifest map with exclude from CSManifest flag set to true.
        if (fExplorer) AlignScraperFileManifestEntries(rac_file, "user_source", prjs.m_name, true);

        // Now that the source file is handled, process the file.
        ProcessProjectRacFileByCPID(prjs.m_name, rac_file, sRacETag, Consensus);
    }

    // After processing, update global structure with the timestamp of the latest file in the manifest.
    {
        LOCK(cs_StructScraperFileManifest);
        _log(logattribute::INFO, "LOCK", "user (rac) files struct update post process: cs_StructScraperFileManifest");

        int64_t nMaxTime = 0;
        for (const auto& entry : StructScraperFileManifest.mScraperFileManifest)
        {
            // Only consider processed (user) files
            if (entry.second.filetype == "user") nMaxTime = std::max(nMaxTime, entry.second.timestamp);
        }

        StructScraperFileManifest.timestamp = nMaxTime;

        // End LOCK(cs_StructScraperFileManifest)
        _log(logattribute::INFO, "ENDLOCK", "user (rac) files struct update post process: cs_StructScraperFileManifest");
    }
    return true;
}




// This version uses a consensus beacon map (and teamid, if team filtering is specified by policy) to filter statistics.
bool ProcessProjectRacFileByCPID(const std::string& project, const fs::path& file, const std::string& etag, BeaconConsensus& Consensus)
{
    // Set fileerror flag to true until made false by the completion of one successful injection of user stats into stream.
    bool bfileerror = true;

    // If passed an empty file, immediately return false.
    if (file.string().empty())
        return false;

    fsbridge::ifstream ingzfile(file, std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(logattribute::ERR, "ProcessProjectRacFileByCPID", "Failed to open rac gzip file (" + file.string() + ")");

        return false;
    }

    _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Opening rac file (" + file.string() + ")");

    boostio::filtering_istream in;

    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    fs::path gzetagfile = pathScraper / (project + "-" + etag + ".csv" + ".gz");

    fsbridge::ofstream outgzfile(gzetagfile, std::ios_base::out | std::ios_base::binary);
    boostio::filtering_ostream out;
    out.push(boostio::gzip_compressor());
    out.push(outgzfile);

    _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Started processing " + file.string());

    std::map<std::string, int64_t> mTeamIDsForProject = {};
    // Take a lock on cs_TeamIDMap to populate local whitelist TeamID vector for this project.
    if (REQUIRE_TEAM_WHITELIST_MEMBERSHIP)
    {
        LOCK(cs_TeamIDMap);
        _log(logattribute::INFO, "LOCK", "cs_TeamIDMap");

        mTeamIDsForProject = TeamIDMap.find(project)->second;

        _log(logattribute::INFO, "ENDLOCK", "cs_TeamIDMap");
    }

    ScraperVerifiedBeacons LocalVerifiedBeacons;

    std::string line;
    stringbuilder builder;
    out << "# total_credit,expavg_time,expavgcredit,cpid" << std::endl;
    while (std::getline(in, line))
    {
        if (line == "<user>")
            builder.clear();
        else if (line == "</user>")
        {
            const std::string& data = builder.value();
            builder.clear();

            const std::string& cpid = ExtractXML(data, "<cpid>", "</cpid>");

            // If the CPID is not already active, attempt to verify it if it
            // is pending.

            if (Consensus.mBeaconMap.count(cpid) < 1)
            {
                const std::string username = ExtractXML(data, "<name>", "</name>");

                // Base58-encoded beacon verification code sizes fall within:
                if (username.size() < 26 || username.size() > 28) continue;

                // The username has to be temporarily changed to a "verification code" that is
                // a base58 encoded version of the public key of the pending beacon, so that
                // it will match the mPendingMap entry. This is the crux of the user validation.
                const auto iter_pair = Consensus.mPendingMap.find(username);

                if (iter_pair == Consensus.mPendingMap.end()) continue;
                if (iter_pair->second.cpid != cpid) continue;

                // This copies the pending beacon entry into the local VerifiedBeacons map and updates
                // the time entry.
                LocalVerifiedBeacons.mVerifiedMap[iter_pair->first] = iter_pair->second;

                _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Verified pending beacon for address "
                     + iter_pair->first + ", cpid " + iter_pair->second.cpid);

                LocalVerifiedBeacons.timestamp = GetAdjustedTime();
            }

            // Only do this if team membership filtering is specified by network policy.
            if (REQUIRE_TEAM_WHITELIST_MEMBERSHIP)
            {
                // Set initial flag for whether user is on team whitelist to false.
                bool bOnTeamWhitelist = false;

                const std::string& sTeamID = ExtractXML(data, "<teamid>", "</teamid>");
                int64_t nTeamID = 0;

                try
                {
                    nTeamID = atoi64(sTeamID);
                }
                catch (const std::exception&)
                {
                    _log(logattribute::ERR, "ProcessProjectRacFileByCPID", "Bad team id in user stats file data.");
                    continue;
                }

                // Check to see if the user's team ID is in the whitelist team ID map for the project.
                for (auto const& iTeam : mTeamIDsForProject)
                {
                    if (iTeam.second == nTeamID)
                        bOnTeamWhitelist = true;
                }

                //If not continue the while loop and do not put the users stats for that project in the outputstatistics file.
                if (!bOnTeamWhitelist) continue;
            }

            // User beacon verified. Append its statistics to the CSV output.
            out << ExtractXML(data, "<total_credit>", "</total_credit>") << ","
                << ExtractXML(data, "<expavg_time>", "</expavg_time>") << ","
                << ExtractXML(data, "<expavg_credit>", "</expavg_credit>") << ","
                << cpid
                << std::endl;

            // If we get here at least once then there is at least one CPID being put in the file.
            // So set the bfileerror flag to false.
            bfileerror = false;
        }
        else
            builder.append(line);
    }

    // Minimize lock time on cs_VerifiedBeacons. The reason that a local temporary map
    // is used above, and then a separate insert loop is used here for the global is to
    // avoid holding the lock for the entire time the while loop processes the user stats
    // file, since for large projects this could be for a relatively long time. (This
    // would be required because to update the global the assignment must be by reference.)
    {
        LOCK(cs_VerifiedBeacons);
        _log(logattribute::INFO, "LOCK", "cs_VerifiedBeacons");

        ScraperVerifiedBeacons& GlobalVerifiedBeacons = GetVerifiedBeacons();

        for (const auto& iter_pair : LocalVerifiedBeacons.mVerifiedMap)
        {
            GlobalVerifiedBeacons.mVerifiedMap[iter_pair.first] = iter_pair.second;
        }

        GlobalVerifiedBeacons.timestamp = LocalVerifiedBeacons.timestamp;

        if (LogInstance().WillLogCategory(BCLog::LogFlags::SCRAPER))
        {
            for (const auto& iter_pair : GlobalVerifiedBeacons.mVerifiedMap)
            {
                _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Global mVerifiedMap entry "
                     + iter_pair.first + ", cpid " + iter_pair.second.cpid);

            }
        }

        _log(logattribute::INFO, "ENDLOCK", "cs_VerifiedBeacons");
    }


    if (bfileerror)
    {
        _log(logattribute::CRITICAL, "ProcessProjectRacFileByCPID", "Error in data processing of " + file.string() + "; Aborted processing");

        ingzfile.close();
        outgzfile.flush();
        outgzfile.close();

        // Remove the source file because it was bad. (Probable incomplete download.)
        if (fs::exists(file))
            fs::remove(file);

        // Remove the errored out processed file.
        if (fs::exists(gzetagfile))
            fs::remove(gzetagfile);

        return false;
    }

    _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Finished processing " + file.string());

    ingzfile.close();
    out.flush();
    out.reset();
    outgzfile.close();

    // Hash the file.
    
    uint256 nFileHash = GetFileHash(gzetagfile);
    _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "FileHash by GetFileHash " + nFileHash.ToString());


    try
    {
        size_t filea = fs::file_size(file);
        fs::path temp = gzetagfile;
        size_t fileb = fs::file_size(temp);

        _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Processed new rac file " + file.string() + "(" + std::to_string(filea) + " -> " + std::to_string(fileb) + ")");

        ndownloadsize += (int64_t)filea;
        nuploadsize += (int64_t)fileb;
    }
    catch (fs::filesystem_error& e)
    {
        _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "FS Error -> " + std::string(e.what()));
    }

    // If not in explorer mode, no need to retain source file.
    if (!fExplorer) fs::remove(file);

    // Here, regardless of explorer mode, save processed rac files to file manifest map with exclude from CSManifest flag set to false.
    AlignScraperFileManifestEntries(gzetagfile, "user", project, false);

    _log(logattribute::INFO, "ProcessProjectRacFileByCPID", "Complete Process");

    return true;
}


uint256 GetFileHash(const fs::path& inputfile)
{
    // open input file, and associate with CAutoFile
    FILE *file = fsbridge::fopen(inputfile, "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    uint256 nHash;
    
    if (filein.IsNull())
        return nHash;

    // use file size to size memory buffer
    int dataSize = boost::filesystem::file_size(inputfile);
    std::vector<unsigned char> vchData;
    vchData.resize(dataSize);

    // read data and checksum from file
    try
    {
        filein.read((char *)&vchData[0], dataSize);
    }
    catch (std::exception &e)
    {
        return nHash;
    }

    filein.fclose();

    CDataStream ssFile(vchData, SER_DISK, CLIENT_VERSION);

    nHash = Hash(ssFile.begin(), ssFile.end());

    return nHash;
}


// Note that cs_StructScraperFileManifest needs to be taken before calling.
uint256 GetmScraperFileManifestHash()
{
    uint256 nHash;
    CDataStream ss(SER_NETWORK, 1);

    for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
    {
        // The purpose of the hash on the mScraperFileManifest map is to be able
        // to decide when to publish a new manifest based on a change. If the CScraperManifest
        // is set to include noncurrent files, then all file entries should be included
        // in the map hash, because they will all be included in the published manifest.
        // If, however, only current files should be included, only the current files
        // in the map will be included in the hash, because otherwise if non-current files
        // are deleted by aging rules, the hash would change but the actual content of the
        // CScraperManifest would not, and so the publishing of the manifest would fail.
        // This was a minor error caught in corner-case testing, and fixed by the below filter.
        //if (SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES || entry.second.current)
        //{
            ss << entry.second.filename
               << entry.second.project
               << entry.second.hash
               << entry.second.timestamp
               << entry.second.current;
        //}
     }

    nHash = Hash(ss.begin(), ss.end());

    return nHash;
}

/***********************
* Persistance          *
************************/

bool LoadBeaconList(const fs::path& file, ScraperBeaconMap& mBeaconMap)
{
    fsbridge::ifstream ingzfile(file, std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(logattribute::ERR, "LoadBeaconList", "Failed to open beacon gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string line;

    int64_t ntimestamp;
    
    // Header -- throw away.
    std::getline(in, line);

    while (std::getline(in, line))
    {
        ScraperBeaconEntry LoadEntry;
        std::string key;

        std::vector<std::string> vline = split(line, ",");

        key = vline[0];

        std::istringstream sstimestamp(vline[1]);
        sstimestamp >> ntimestamp;
        LoadEntry.timestamp = ntimestamp;

        LoadEntry.value = vline[2];

        mBeaconMap[key] = LoadEntry;
    }

    return true;
}



bool LoadTeamIDList(const fs::path& file)
{
    fsbridge::ifstream ingzfile(file, std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(logattribute::ERR, "LoadTeamIDList", "Failed to open Team ID gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string line;
    std::string separator = "<>";

    // Header. This is used to construct the team names vector, since the team IDs were stored in the same order.
    std::getline(in, line);

    // This is to detect an existing TeamID.csv.gz file that contains the wrong separators. The load will be aborted,
    // This will cause an overwrite of the file with the correct separators when the team files are processed fresh.
    if (line.find(separator) == std::string::npos) return false;

    // This is in the form Project, Gridcoin, ...."
    std::vector<std::string> vTeamNames = split(line, separator);
    _log(logattribute::INFO, "LoadTeamIDList", "Size of vTeamNames = " + std::to_string(vTeamNames.size()));

    while (std::getline(in, line))
    {
        std::string sProject = {};
        std::map<std::string, int64_t> mTeamIDsForProject = {};

        std::vector<std::string> vline = split(line, separator);

        unsigned int iTeamName = 0;
        // Populate team IDs into map.
        for (const auto& iter : vline)
        {
            int64_t nTeamID;

            // Skip (probably stale or bad entry with more or less team IDs than the header.
            if (vline.size() != vTeamNames.size())
                continue;

            // The first element is the project
            if (!iTeamName)
                sProject = iter;
            else
            {
                try
                {
                    nTeamID = atoi64(iter);
                }
                catch (std::exception&)
                {
                    _log(logattribute::ERR, "LoadTeamIDList", "Ignoring invalid team id found in team id file.");
                    continue;
                }

                // Don't populate a map entry for a TeamID of -1, because that indicates the association does not exist.
                if (nTeamID != -1)
                {
                    std::string sTeamName = vTeamNames.at(iTeamName);

                    mTeamIDsForProject[sTeamName] = nTeamID;
                }
            }

            iTeamName++;
        }

        LOCK(cs_TeamIDMap);
        _log(logattribute::INFO, "LOCK", "cs_TeamIDMap");

        // Insert into whitelist team ID map.
        if (!sProject.empty())
            TeamIDMap[sProject] = mTeamIDsForProject;

        _log(logattribute::INFO, "ENDLOCK", "cs_TeamIDMap");
    }

    return true;
}



bool StoreBeaconList(const fs::path& file)
{
    BeaconConsensus Consensus = GetConsensusBeaconList();

    _log(logattribute::INFO, "StoreBeaconList", "ReadCacheSection element count: " + std::to_string(NN::GetBeaconRegistry().Beacons().size()));
    _log(logattribute::INFO, "StoreBeaconList", "mBeaconMap element count: " + std::to_string(Consensus.mBeaconMap.size()));

    // Update block hash for block at consensus height to StructScraperFileManifest.
    // Requires a lock.
    {
        LOCK(cs_StructScraperFileManifest);
        _log(logattribute::INFO, "LOCK", "store beacon list - update consensus block hash: cs_StructScraperFileManifest");

        StructScraperFileManifest.nConsensusBlockHash = Consensus.nBlockHash;

        // End LOCK(cs_StructScraperFileManifest)
        _log(logattribute::INFO, "ENDLOCK", "store beacon list - update consensus block hash: cs_StructScraperFileManifest");
    }

    if (fs::exists(file))
        fs::remove(file);

    fsbridge::ofstream outgzfile(file, std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(logattribute::ERR, "StoreBeaconList", "Failed to open beacon list gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(logattribute::INFO, "StoreBeaconList", "Started processing " + file.string());
    
    // Header
    stream << "CPID," << "Time," << "Beacon\n";

    for (auto const& entry : Consensus.mBeaconMap)
    {
        std::string sBeaconEntry = entry.first + "," + std::to_string(entry.second.timestamp) + "," + entry.second.value + "\n";
        stream << sBeaconEntry;
    }

    _log(logattribute::INFO, "StoreBeaconList", "Finished processing beacon data from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(logattribute::INFO, "StoreBeaconList", "Process Complete.");

    return true;
}



bool StoreTeamIDList(const fs::path& file)
{
    LOCK(cs_TeamIDMap);

    if (fs::exists(file))
        fs::remove(file);

    fsbridge::ofstream outgzfile(file, std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(logattribute::ERR, "StoreTeamIDList", "Failed to open team ID list gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(logattribute::INFO, "StoreTeamIDList", "Started processing " + file.string());

    // Header
    stream << "Project";

    std::vector<std::string> vTeamWhiteList = GetTeamWhiteList();
    std::set<std::string> setTeamWhiteList;

    // Ensure that the team names are in the correct order.
    for (auto const& iTeam: vTeamWhiteList)
        setTeamWhiteList.insert(iTeam);

    for (auto const& iTeam: setTeamWhiteList)
        stream << "<>" << iTeam;

    stream << std::endl;

    _log(logattribute::INFO, "StoreTeamIDList", "TeamIDMap size = " + std::to_string(TeamIDMap.size()));

    // Data
    for (auto const& iProject : TeamIDMap)
    {
        std::string sProjectEntry = {};

        stream << iProject.first;

        for (auto const& iTeam: setTeamWhiteList)
        {
            // iter will point to the key in the inner map for that team. The second value of the iter will then be
            // the TeamID. If it doesn't exist, store a -1 as a "NA" placeholder.
            auto const& iter = iProject.second.find(iTeam);

            if (iter != iProject.second.end())
            {
                sProjectEntry += "<>" + std::to_string(iter->second);
            }
            else
            {
                sProjectEntry += "<>-1";
            }
        }

        stream << sProjectEntry << std::endl;
    }

    _log(logattribute::INFO, "StoreTeamIDList", "Finished processing Team ID data from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(logattribute::INFO, "StoreTeamIDList", "Process Complete.");

    return true;
}



// Insert entry into Manifest. Note that cs_StructScraperFileManifest needs to be taken before calling.
bool InsertScraperFileManifestEntry(ScraperFileManifestEntry& entry)
{
    // This less readable form is so we know whether the element already existed or not.
    std::pair<ScraperFileManifestMap::iterator, bool> ret;
    {
        ret = StructScraperFileManifest.mScraperFileManifest.insert(std::make_pair(entry.filename, entry));
        // If successful insert, rehash map and record in struct for easy comparison later. If already
        // exists, hash is unchanged.
        if (ret.second)
        {
            StructScraperFileManifest.nFileManifestMapHash = GetmScraperFileManifestHash();

            _log(logattribute::INFO, "InsertScraperFileManifestEntry", "Inserted File Manifest Entry and stored modified nFileManifestMapHash.");
        }
    }

    // True if insert was sucessful, false if entry with key (hash) already exists in map.
    return ret.second;
}

// Delete entry from Manifest and corresponding file if it exists. Note that cs_StructScraperFileManifest needs to be taken before calling.
unsigned int DeleteScraperFileManifestEntry(ScraperFileManifestEntry& entry)
{
    unsigned int ret;

    // Delete corresponding file if it exists.
    if (fs::exists(pathScraper / entry.filename))
        fs::remove(pathScraper / entry.filename);

    ret = StructScraperFileManifest.mScraperFileManifest.erase(entry.filename);

    // If an element was deleted then rehash the map and store hash in struct.
    if (ret)
    {
        StructScraperFileManifest.nFileManifestMapHash = GetmScraperFileManifestHash();

        _log(logattribute::INFO, "DeleteScraperFileManifestEntry", "Deleted File Manifest Entry and stored modified nFileManifestMapHash.");
    }

    // Returns number of elements erased, either 0 or 1.
    return ret;
}



// Mark manifest entry non-current. The reason this is encapsulated in a function is
// to  ensure the rehash is done. Note that cs_StructScraperFileManifest needs to be
// taken before calling.
bool MarkScraperFileManifestEntryNonCurrent(ScraperFileManifestEntry& entry)
{
    entry.current = false;

    StructScraperFileManifest.nFileManifestMapHash = GetmScraperFileManifestHash();

    _log(logattribute::INFO, "DeleteScraperFileManifestEntry", "Marked File Manifest Entry non-current and stored modified nFileManifestMapHash.");

    return true;
}


void AlignScraperFileManifestEntries(const fs::path& file, const std::string& filetype, const std::string& sProject, const bool& excludefromcsmanifest)
{
    ScraperFileManifestEntry NewRecord;

    std::string file_name = file.filename().string();

    NewRecord.filename = file_name;
    NewRecord.project = sProject;
    NewRecord.hash = GetFileHash(file);
    NewRecord.timestamp = GetAdjustedTime();
    NewRecord.current = true;
    NewRecord.excludefromcsmanifest = excludefromcsmanifest;
    NewRecord.filetype = filetype;

    // Code block to lock StructScraperFileManifest during record insertion and delete because we want this atomic.
    {
        LOCK(cs_StructScraperFileManifest);

        if (excludefromcsmanifest)
        {
            _log(logattribute::INFO, "LOCK", "saved manifest for downloaded "+ filetype + " files: AlignScraperFileManifestEntries: cs_StructScraperFileManifest");
        }
        else
        {
            _log(logattribute::INFO, "LOCK", "saved manifest for processed "+ filetype + " files: AlignScraperFileManifestEntries: cs_StructScraperFileManifest");
        }

        // Iterate mScraperFileManifest to find any prior filetype records for the same project and change current flag to false,
        // or delete if older than SCRAPER_FILE_RETENTION_TIME or non-current and fScraperRetainNonCurrentFiles
        // is false.

        ScraperFileManifestMap::iterator entry;
        for (entry = StructScraperFileManifest.mScraperFileManifest.begin(); entry != StructScraperFileManifest.mScraperFileManifest.end(); )
        {
            ScraperFileManifestMap::iterator entry_copy = entry++;

            if (entry_copy->second.project == sProject && entry_copy->second.current == true && entry_copy->second.filetype == filetype)
            {
                _log(logattribute::INFO, "AlignScraperFileManifestEntries", "Marking old project manifest "+ filetype + " entry as current = false.");
                MarkScraperFileManifestEntryNonCurrent(entry_copy->second);
            }

            // If filetype records are older than EXPLORER_EXTENDED_FILE_RETENTION_TIME delete record, or if fScraperRetainNonCurrentFiles is false,
            // delete all non-current records, including the one just marked non-current. (EXPLORER_EXTENDED_FILE_RETENTION_TIME rather
            // then SCRAPER_FILE_RETENTION_TIME is used, because this section is only active if fExplorer is true.)
            if (entry_copy->second.filetype == filetype && (((GetAdjustedTime() - entry_copy->second.timestamp) > EXPLORER_EXTENDED_FILE_RETENTION_TIME)
                                                          || (entry_copy->second.project == sProject && entry_copy->second.current == false && !SCRAPER_RETAIN_NONCURRENT_FILES)))
            {
                DeleteScraperFileManifestEntry(entry_copy->second);
            }
        }

        if (!InsertScraperFileManifestEntry(NewRecord))
            _log(logattribute::WARNING, "AlignScraperFileManifestEntries", "Manifest entry already exists for " + NewRecord.hash.ToString() + " " + file_name);
        else
            _log(logattribute::INFO, "AlignScraperFileManifestEntries", "Created manifest entry for " + NewRecord.hash.ToString() + " " + file_name);

        // The below is not an ideal implementation, because the entire map is going to be written out to disk each time.
        // The manifest file is actually very small though, and this primitive implementation will suffice.
        _log(logattribute::INFO, "AlignScraperFileManifestEntries", "Persisting manifest entry to disk.");
        if (!StoreScraperFileManifest(pathScraper / "Manifest.csv.gz"))
            _log(logattribute::ERR, "AlignScraperFileManifestEntries", "StoreScraperFileManifest error occurred");
        else
            _log(logattribute::INFO, "AlignScraperFileManifestEntries", "Stored Manifest");

        // End LOCK(cs_StructScraperFileManifest)
        if (excludefromcsmanifest)
        {
            _log(logattribute::INFO, "ENDLOCK", "saved manifest for downloaded "+ filetype + " files: AlignScraperFileManifestEntries: cs_StructScraperFileManifest");
        }
        else
        {
            _log(logattribute::INFO, "ENDLOCK", "saved manifest for processed "+ filetype + " files: AlignScraperFileManifestEntries: cs_StructScraperFileManifest");
        }
    }
}


bool LoadScraperFileManifest(const fs::path& file)
{
    fsbridge::ifstream ingzfile(file, std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(logattribute::ERR, "LoadScraperFileManifest", "Failed to open manifest gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string line;

    ScraperFileManifestEntry LoadEntry;

    int64_t ntimestamp;

    // Header - throw away.
    std::getline(in, line);
    
    while (std::getline(in, line))
    {

        std::vector<std::string> vline = split(line, ",");

        uint256 nhash;
        nhash.SetHex(vline[0].c_str());
        LoadEntry.hash = nhash;

        LoadEntry.current = std::stoi(vline[1]);

        std::istringstream sstimestamp(vline[2]);
        sstimestamp >> ntimestamp;
        LoadEntry.timestamp = ntimestamp;

        LoadEntry.project = vline[3];
        
        LoadEntry.filename = vline[4];

        // This handles startup with legacy manifest file without excludefromcsmanifest column.
        if (vline.size() >= 6)
        {
            // Intended for explorer mode, where files not to be included in CScraperManifest
            // are to be maintained, such as team and host files.
            LoadEntry.excludefromcsmanifest = std::stoi(vline[5]);
        }
        else
        {
            // The default if the field is not there is false. (Because scraper ver 1 all files are to be
            // included.)
            LoadEntry.excludefromcsmanifest = false;
        }

        // This handles startup with legacy manifest file without filetype column.
        if (vline.size() >= 7)
        {
            // In scraper ver 2, we have to support explorer mode, which includes retention of other files besides
            // user statistics.
            LoadEntry.filetype = vline[6];
        }
        else
        {
            // The default if the field is not there is user. (Because scraper ver 1 all files in the manifest are
            // user.)
            LoadEntry.filetype = "user";
        }

        // Lock cs_StructScraperFileManifest before updating
        // global structure.
        {
            LOCK(cs_StructScraperFileManifest);
            _log(logattribute::INFO, "LOCK", "load scraper file manifest - update entry: cs_StructScraperFileManifest");

            InsertScraperFileManifestEntry(LoadEntry);

            // End LOCK(cs_StructScraperFileManifest
            _log(logattribute::INFO, "ENDLOCK", "load scraper file manifest - update entry: cs_StructScraperFileManifest");
        }
    }

    return true;
}


bool StoreScraperFileManifest(const fs::path& file)
{
    if (fs::exists(file))
        fs::remove(file);

    fsbridge::ofstream outgzfile(file, std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(logattribute::ERR, "StoreScraperFileManifest", "Failed to open manifest gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(logattribute::INFO, "StoreScraperFileManifest", "Started processing " + file.string());

    //Lock StructScraperFileManifest during serialize to string.
    {
        LOCK(cs_StructScraperFileManifest);
        _log(logattribute::INFO, "LOCK", "store scraper file manifest to file: cs_StructScraperFileManifest");
        
        // Header.
        stream << "Hash," << "Current," << "Time," << "Project," << "Filename," << "ExcludeFromCSManifest," << "Filetype" << "\n";
        
        for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
        {
            uint256 nEntryHash = entry.second.hash;

            std::string sScraperFileManifestEntry = nEntryHash.GetHex() + ","
                    + std::to_string(entry.second.current) + ","
                    + std::to_string(entry.second.timestamp) + ","
                    + entry.second.project + ","
                    + entry.first + ","
                    + std::to_string(entry.second.excludefromcsmanifest) + ","
                    + entry.second.filetype + "\n";
            stream << sScraperFileManifestEntry;
        }

        // end LOCK(cs_StructScraperFileManifest)
        _log(logattribute::INFO, "ENDLOCK", "store scraper file manifest to file: cs_StructScraperFileManifest");
    }

    _log(logattribute::INFO, "StoreScraperFileManifest", "Finished processing manifest from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(logattribute::INFO, "StoreScraperFileManifest", "Process Complete.");

    return true;
}



bool StoreStats(const fs::path& file, const ScraperStats& mScraperStats)
{
    if (fs::exists(file))
        fs::remove(file);

    fsbridge::ofstream outgzfile(file, std::ios_base::out | std::ios_base::binary);

    if (!outgzfile)
    {
        _log(logattribute::ERR, "StoreStats", "Failed to open stats gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(logattribute::INFO, "StoreStats", "Started processing " + file.string());

    // Header.
    stream << "StatsType," << "Project," << "CPID," << "TC," << "RAT," << "RAC," << "AvgRAC," << "Mag\n";

    for (auto const& entry : mScraperStats)
    {
        // This nonsense is to align the key to the columns of the csv.
        std::string sobjectIDforcsv;

        switch(entry.first.objecttype)
        {
        case statsobjecttype::byCPIDbyProject:
        {
            sobjectIDforcsv = entry.first.objectID;
            break;
        }
        case statsobjecttype::byProject:
        {
            sobjectIDforcsv = entry.first.objectID + ",";
            break;
        }
        case statsobjecttype::byCPID:
        {
            sobjectIDforcsv = "," + entry.first.objectID;
            break;
        }
        case statsobjecttype::NetworkWide:
        {
            sobjectIDforcsv = ",";
            break;
        }
        }

        std::string sScraperStatsEntry = GetTextForstatsobjecttype(entry.first.objecttype) + ","
                + sobjectIDforcsv + ","
                + std::to_string(entry.second.statsvalue.dTC) + ","
                + std::to_string(entry.second.statsvalue.dRAT) + ","
                + std::to_string(entry.second.statsvalue.dRAC) + ","
                + std::to_string(entry.second.statsvalue.dAvgRAC) + ","
                + std::to_string(entry.second.statsvalue.dMag) + ","
                + "\n";
        stream << sScraperStatsEntry;
    }

    _log(logattribute::INFO, "StoreStats", "Finished processing stats from map.");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    outgzfile.flush();
    outgzfile.close();

    _log(logattribute::INFO, "StoreStats", "Process Complete.");

    return true;
}

/***********************
* Stats Computations   *
************************/



bool LoadProjectFileToStatsByCPID(const std::string& project, const fs::path& file, const double& projectmag, const ScraperBeaconMap& mBeaconMap, ScraperStats& mScraperStats)
{
    fsbridge::ifstream ingzfile(file, std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(logattribute::ERR, "LoadProjectFileToStatsByCPID", "Failed to open project user stats gzip file (" + file.string() + ")");

        return false;
    }

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    bool bResult = ProcessProjectStatsFromStreamByCPID(project, in, projectmag, mBeaconMap, mScraperStats);

    return bResult;
}



bool LoadProjectObjectToStatsByCPID(const std::string& project, const CSerializeData& ProjectData, const double& projectmag, const ScraperBeaconMap& mBeaconMap, ScraperStats& mScraperStats)
{
    boostio::basic_array_source<char> input_source(&ProjectData[0], ProjectData.size());
    boostio::stream<boostio::basic_array_source<char>> ingzss(input_source);

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzss);

    bool bResult = ProcessProjectStatsFromStreamByCPID(project, in, projectmag, mBeaconMap, mScraperStats);

    return bResult;
}



bool ProcessProjectStatsFromStreamByCPID(const std::string& project, boostio::filtering_istream& sUncompressedIn,
                                         const double& projectmag, const ScraperBeaconMap& mBeaconMap, ScraperStats& mScraperStats)
{
    std::vector<std::string> vXML;

    // Lets vector the user blocks
    std::string line;
    double dProjectRAC = 0.0;
    while (std::getline(sUncompressedIn, line))
    {
        if (line[0] == '#')
            continue;

        std::vector<std::string> fields;
        boost::split(fields, line, boost::is_any_of(","), boost::token_compress_on);

        if (fields.size() < 4)
            continue;

        ScraperObjectStats statsentry = {};

        const std::string& sTC = fields[0];
        const std::string& sRAT = fields[1];
        const std::string& sRAC = fields[2];
        const std::string& cpid = fields[3];

        // Replace blank strings with zeros.
        statsentry.statsvalue.dTC = (sTC.empty()) ? 0.0 : std::stod(sTC);
        statsentry.statsvalue.dRAT = (sRAT.empty()) ? 0.0 : std::stod(sRAT);
        statsentry.statsvalue.dRAC = (sRAC.empty()) ? 0.0 : std::stod(sRAC);
        // At the individual (byCPIDbyProject) level the AvgRAC is the same as the RAC.
        statsentry.statsvalue.dAvgRAC = statsentry.statsvalue.dRAC;
        // Mag is dealt with on the second pass... so is left at 0.0 on the first pass.

        statsentry.statskey.objecttype = statsobjecttype::byCPIDbyProject;
        statsentry.statskey.objectID = project + "," + cpid;

        // Insert stats entry into map by the key.
        mScraperStats[statsentry.statskey] = statsentry;

        // Increment project
        dProjectRAC += statsentry.statsvalue.dRAC;
    }

    _log(logattribute::INFO, "LoadProjectObjectToStatsByCPID", "There are " + std::to_string(mScraperStats.size()) + " CPID entries for " + project);

    // The mScraperStats here is scoped to only this project so we do not need project filtering here.
    ScraperStats::iterator entry;

    for (auto const& entry : mScraperStats)
    {
        ScraperObjectStats statsentry;

        statsentry.statskey = entry.first;
        statsentry.statsvalue.dTC = entry.second.statsvalue.dTC;
        statsentry.statsvalue.dRAT = entry.second.statsvalue.dRAT;
        statsentry.statsvalue.dRAC = entry.second.statsvalue.dRAC;
        // As per the above the individual (byCPIDbyProject) level the AvgRAC is the same as the RAC.
        statsentry.statsvalue.dAvgRAC = entry.second.statsvalue.dAvgRAC;
        statsentry.statsvalue.dMag = MagRound(entry.second.statsvalue.dRAC / dProjectRAC * projectmag);

        // Update map entry with the magnitude.
        mScraperStats[statsentry.statskey] = statsentry;
    }

    // Due to rounding to MAG_ROUND, the actual total project magnitude will not be exactly projectmag,
    // but it should be very close. Roll up project statistics.
    ScraperObjectStats ProjectStatsEntry = {};

    ProjectStatsEntry.statskey.objecttype = statsobjecttype::byProject;
    ProjectStatsEntry.statskey.objectID = project;

    unsigned int nCPIDCount = 0;
    for (auto const& entry : mScraperStats)
    {
        ProjectStatsEntry.statsvalue.dTC += entry.second.statsvalue.dTC;
        ProjectStatsEntry.statsvalue.dRAT += entry.second.statsvalue.dRAT;
        ProjectStatsEntry.statsvalue.dRAC += entry.second.statsvalue.dRAC;
        ProjectStatsEntry.statsvalue.dMag += entry.second.statsvalue.dMag;

        nCPIDCount++;
    }

    //Compute AvgRAC for project across CPIDs and set.
    (nCPIDCount > 0) ? ProjectStatsEntry.statsvalue.dAvgRAC = ProjectStatsEntry.statsvalue.dRAC / nCPIDCount : ProjectStatsEntry.statsvalue.dAvgRAC = 0.0;

    // Insert project level map entry.
    mScraperStats[ProjectStatsEntry.statskey] = ProjectStatsEntry;

    return true;

}



// ------------------------------------------------ In ------------------- both In/Out
bool ProcessNetworkWideFromProjectStats(ScraperBeaconMap& mBeaconMap, ScraperStats& mScraperStats)
{
    // We are going to cut across projects and group by CPID.

    //Also track the network wide rollup.
    ScraperObjectStats NetworkWideStatsEntry = {};

    NetworkWideStatsEntry.statskey.objecttype = statsobjecttype::NetworkWide;
    // ObjectID is blank string for network-wide.
    NetworkWideStatsEntry.statskey.objectID = "";

    unsigned int nCPIDProjectCount = 0;
    for (auto const& beaconentry : mBeaconMap)
    {
        ScraperObjectStats CPIDStatsEntry = {};

        CPIDStatsEntry.statskey.objecttype = statsobjecttype::byCPID;
        CPIDStatsEntry.statskey.objectID = beaconentry.first;

        unsigned int nProjectCount = 0;
        for (auto const& innerentry : mScraperStats)
        {
            // Only select the individual byCPIDbyProject stats for the selected CPID. Leave out the project rollup (byProj) ones,
            // otherwise dimension mixing will result.

            std::string objectID = innerentry.first.objectID;

            std::size_t found = objectID.find(CPIDStatsEntry.statskey.objectID);

            if (innerentry.first.objecttype == statsobjecttype::byCPIDbyProject && found!=std::string::npos)
            {
                CPIDStatsEntry.statsvalue.dTC += innerentry.second.statsvalue.dTC;
                CPIDStatsEntry.statsvalue.dRAT += innerentry.second.statsvalue.dRAT;
                CPIDStatsEntry.statsvalue.dRAC += innerentry.second.statsvalue.dRAC;
                CPIDStatsEntry.statsvalue.dMag += innerentry.second.statsvalue.dMag;
                // Note the following is VERY inelegant. It CAPS the CPID magnitude to CPID_MAG_LIMIT.
                // No attempt to renormalize the magnitudes due to this cap is done at this time. This means
                // The total magnitude across projects will NOT match the total across all CPIDs and the network.
                CPIDStatsEntry.statsvalue.dMag = std::min(CPID_MAG_LIMIT, CPIDStatsEntry.statsvalue.dMag);

                nProjectCount++;
                nCPIDProjectCount++;
            }
        }

        // Compute CPID AvgRAC across the projects for that CPID and set.
        (nProjectCount > 0) ? CPIDStatsEntry.statsvalue.dAvgRAC = CPIDStatsEntry.statsvalue.dRAC / nProjectCount : CPIDStatsEntry.statsvalue.dAvgRAC = 0.0;

        // Insert the byCPID entry into the overall map.
        mScraperStats[CPIDStatsEntry.statskey] = CPIDStatsEntry;

        // Increement the network wide stats.
        NetworkWideStatsEntry.statsvalue.dTC += CPIDStatsEntry.statsvalue.dTC;
        NetworkWideStatsEntry.statsvalue.dRAT += CPIDStatsEntry.statsvalue.dRAT;
        NetworkWideStatsEntry.statsvalue.dRAC += CPIDStatsEntry.statsvalue.dRAC;
        NetworkWideStatsEntry.statsvalue.dMag += CPIDStatsEntry.statsvalue.dMag;
    }

    // Compute Network AvgRAC across all ByCPIDByProject elements and set.
    (nCPIDProjectCount > 0) ? NetworkWideStatsEntry.statsvalue.dAvgRAC = NetworkWideStatsEntry.statsvalue.dRAC / nCPIDProjectCount : NetworkWideStatsEntry.statsvalue.dAvgRAC = 0.0;

    // Insert the (single) network-wide entry into the overall map.
    mScraperStats[NetworkWideStatsEntry.statskey] = NetworkWideStatsEntry;

    return true;
}

// Note that this function essentially constructs the scraper stats from the current state of the scraper, which is all of the current files at the time
// the function is called.
ScraperStatsAndVerifiedBeacons GetScraperStatsByConsensusBeaconList()
{
    _log(logattribute::INFO, "GetScraperStatsByConsensusBeaconList", "Beginning stats processing.");

    // Enumerate the count of active projects from the file manifest. Since the manifest is
    // constructed starting with the whitelist, and then using only the current files, this
    // will always be less than or equal to the whitelist count from whitelist.
    unsigned int nActiveProjects = 0;
    {
        LOCK(cs_StructScraperFileManifest);
        _log(logattribute::INFO, "LOCK", "get scraper stats by consensus beacon list - count active projects: cs_StructScraperFileManifest");

        for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
        {
            //
            if (entry.second.current && !entry.second.excludefromcsmanifest) nActiveProjects++;
        }

        // End LOCK(cs_StructScraperFileManifest)
        _log(logattribute::INFO, "ENDLOCK", "get scraper stats by consensus beacon list - count active projects: cs_StructScraperFileManifest");
    }
    double dMagnitudePerProject = NEURALNETWORKMULTIPLIER / nActiveProjects;

    //Get the Consensus Beacon map and initialize mScraperStats.
    BeaconConsensus Consensus = GetConsensusBeaconList();

    ScraperStats mScraperStats;

    {
        LOCK(cs_StructScraperFileManifest);
        _log(logattribute::INFO, "LOCK", "get scraper stats by consensus beacon list - load project file to stats: cs_StructScraperFileManifest");

        for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
        {

            if (entry.second.current && !entry.second.excludefromcsmanifest)
            {
                std::string project = entry.first;
                fs::path file = pathScraper / entry.second.filename;
                ScraperStats mProjectScraperStats;

                _log(logattribute::INFO, "GetScraperStatsByConsensusBeaconList", "Processing stats for project: " + project);

                LoadProjectFileToStatsByCPID(project, file, dMagnitudePerProject, Consensus.mBeaconMap, mProjectScraperStats);

                // Insert into overall map.
                for (auto const& entry2 : mProjectScraperStats)
                {
                    mScraperStats[entry2.first] = entry2.second;
                }
            }
        }

        // End LOCK(cs_StructScraperFileManifest)
        _log(logattribute::INFO, "ENDLOCK", "get scraper stats by consensus beacon list - load project file to stats: cs_StructScraperFileManifest");
    }

    ProcessNetworkWideFromProjectStats(Consensus.mBeaconMap, mScraperStats);

    // Since this function uses the current project files for statistics, it also makes sense to use the current verified beacons map.

    LOCK(cs_VerifiedBeacons);
    _log(logattribute::INFO, "LOCK", "cs_VerifiedBeacons");

    ScraperVerifiedBeacons& verified_beacons = GetVerifiedBeacons();

    ScraperStatsAndVerifiedBeacons stats_and_verified_beacons;

    stats_and_verified_beacons.mScraperStats = mScraperStats;
    stats_and_verified_beacons.mVerifiedMap = verified_beacons.mVerifiedMap;

    _log(logattribute::INFO, "GetScraperStatsByConsensusBeaconList", "Completed stats processing");

    _log(logattribute::INFO, "ENDLOCK", "cs_VerifiedBeacons");

    return stats_and_verified_beacons;
}


ScraperStatsAndVerifiedBeacons GetScraperStatsByConvergedManifest(const ConvergedManifest& StructConvergedManifest)
{
    _log(logattribute::INFO, "GetScraperStatsByConvergedManifest", "Beginning stats processing.");

    ScraperStatsAndVerifiedBeacons stats_and_verified_beacons;

    // Enumerate the count of active projects from the dummy converged manifest. One of the parts
    // is the beacon list, is not a project, which is why that should not be included in the count.
    // Populate the verified beacons map, and if it is don't count that either.
    ScraperPendingBeaconMap VerifiedBeaconMap;

    int exclude_parts_from_count = 1;

    const auto& iter = StructConvergedManifest.ConvergedManifestPartsMap.find("VerifiedBeacons");
    if (iter != StructConvergedManifest.ConvergedManifestPartsMap.end())
    {
        CDataStream part(iter->second, SER_NETWORK, 1);

        try
        {
            part >> VerifiedBeaconMap;
        }
        catch (const std::exception& e)
        {
            _log(logattribute::WARNING, __func__, "failed to deserialize verified beacons part: " + std::string(e.what()));
        }

        ++exclude_parts_from_count;
    }

    stats_and_verified_beacons.mVerifiedMap = VerifiedBeaconMap;

    unsigned int nActiveProjects = StructConvergedManifest.ConvergedManifestPartsMap.size() - exclude_parts_from_count;
    _log(logattribute::INFO, "GetScraperStatsByConvergedManifest", "Number of active projects in converged manifest = " + std::to_string(nActiveProjects));

    double dMagnitudePerProject = NEURALNETWORKMULTIPLIER / nActiveProjects;

    //Get the Consensus Beacon map and initialize mScraperStats.
    ScraperBeaconMap mBeaconMap;
    LoadBeaconListFromConvergedManifest(StructConvergedManifest, mBeaconMap);

    ScraperStats mScraperStats;

    for (auto entry = StructConvergedManifest.ConvergedManifestPartsMap.begin(); entry != StructConvergedManifest.ConvergedManifestPartsMap.end(); ++entry)
    {
        std::string project = entry->first;
        ScraperStats mProjectScraperStats;

        // Do not process the BeaconList or VerifiedBeacons as a project stats file.
        if (project != "BeaconList" && project != "VerifiedBeacons")
        {
            _log(logattribute::INFO, "GetScraperStatsByConvergedManifest", "Processing stats for project: " + project);

            LoadProjectObjectToStatsByCPID(project, entry->second, dMagnitudePerProject, mBeaconMap, mProjectScraperStats);

            // Insert into overall map.
            for (auto const& entry2 : mProjectScraperStats)
            {
                mScraperStats[entry2.first] = entry2.second;
            }
        }
    }

    ProcessNetworkWideFromProjectStats(mBeaconMap, mScraperStats);

    stats_and_verified_beacons.mScraperStats = mScraperStats;

    _log(logattribute::INFO, "GetScraperStatsByConvergedManifest", "Completed stats processing");

    return stats_and_verified_beacons;
}

// This function should only be used as part of the superblock validation in bv11+.
ScraperStatsAndVerifiedBeacons GetScraperStatsFromSingleManifest(CScraperManifest& manifest)
{
    _log(logattribute::INFO, "GetScraperStatsFromSingleManifest", "Beginning stats processing.");

    // Create a dummy converged manifest
    ConvergedManifest StructDummyConvergedManifest;

    ScraperStatsAndVerifiedBeacons stats_and_verified_beacons {};

    // Fill out the dummy ConvergedManifest structure. Note this assumes one-to-one part to project statistics BLOB. Needs to
    // be fixed for more than one part per BLOB. This is easy in this case, because it is all from/referring to one manifest.

    StructDummyConvergedManifest.ConsensusBlock = manifest.ConsensusBlock;
    StructDummyConvergedManifest.timestamp = GetAdjustedTime();
    StructDummyConvergedManifest.bByParts = false;

    int iPartNum = 0;
    CDataStream ss(SER_NETWORK,1);
    WriteCompactSize(ss, manifest.vParts.size());
    uint256 nContentHashCheck;

    for (const auto& iter : manifest.vParts)
    {
        std::string sProject;

        if (iPartNum == 0)
            sProject = "BeaconList";
        else
            sProject = manifest.projects[iPartNum-1].project;

        // Copy the parts data into the map keyed by project.
        StructDummyConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair(sProject, iter->data));

        // Serialize the hash to doublecheck the content hash.
        ss << iter->hash;

        iPartNum++;
    }
    ss << StructDummyConvergedManifest.ConsensusBlock;

    nContentHashCheck = Hash(ss.begin(), ss.end());

    if (nContentHashCheck != manifest.nContentHash)
    {
        _log(logattribute::ERR, "GetScraperStatsFromSingleManifest", "Selected Manifest content hash check failed! nContentHashCheck = "
             + nContentHashCheck.GetHex() + " and nContentHash = " + manifest.nContentHash.GetHex());
        // Content hash check failed. Return empty mScraperStats
        return stats_and_verified_beacons;
    }
    else // Content matches.
    {
        // The DummyConvergedManifest content hash is NOT the same as the hash above from the CScraper::manifest, because it needs to be in the order of the
        // map key and on the data, not the order of vParts by the part hash. So, unfortunately, we have to walk through the map again to hash it correctly.
        CDataStream ss2(SER_NETWORK,1);
        for (const auto& iter : StructDummyConvergedManifest.ConvergedManifestPartsMap)
            ss2 << iter.second;

        StructDummyConvergedManifest.nContentHash = Hash(ss2.begin(), ss2.end());
    }

    // Enumerate the count of active projects from the dummy converged manifest. One of the parts
    // is the beacon list, is not a project, which is why that should not be included in the count.
    // Populate the verified beacons map, and if it is don't count that either.
    ScraperPendingBeaconMap VerifiedBeaconMap;

    int exclude_parts_from_count = 1;

    const auto& iter = StructDummyConvergedManifest.ConvergedManifestPartsMap.find("VerifiedBeacons");
    if (iter != StructDummyConvergedManifest.ConvergedManifestPartsMap.end())
    {
        CDataStream part(iter->second, SER_NETWORK, 1);

        try
        {
            part >> VerifiedBeaconMap;
        }
        catch (const std::exception& e)
        {
            _log(logattribute::WARNING, __func__, "failed to deserialize verified beacons part: " + std::string(e.what()));
        }

        ++exclude_parts_from_count;
    }

    stats_and_verified_beacons.mVerifiedMap = VerifiedBeaconMap;

    unsigned int nActiveProjects = StructDummyConvergedManifest.ConvergedManifestPartsMap.size() - exclude_parts_from_count;
    _log(logattribute::INFO, "GetScraperStatsFromSingleManifest", "Number of active projects in converged manifest = " + std::to_string(nActiveProjects));

    double dMagnitudePerProject = NEURALNETWORKMULTIPLIER / nActiveProjects;

    //Get the Consensus Beacon map and initialize mScraperStats.
    ScraperBeaconMap mBeaconMap;
    LoadBeaconListFromConvergedManifest(StructDummyConvergedManifest, mBeaconMap);

    for (auto entry = StructDummyConvergedManifest.ConvergedManifestPartsMap.begin(); entry != StructDummyConvergedManifest.ConvergedManifestPartsMap.end(); ++entry)
    {
        std::string project = entry->first;
        ScraperStats mProjectScraperStats;

        // Do not process the BeaconList or VerifiedBeacons as a project stats file.
        if (project != "BeaconList" && project != "VerifiedBeacons")
        {
            _log(logattribute::INFO, "GetScraperStatsFromSingleManifest", "Processing stats for project: " + project);

            LoadProjectObjectToStatsByCPID(project, entry->second, dMagnitudePerProject, mBeaconMap, mProjectScraperStats);

            // Insert into overall map.
            for (auto const& entry2 : mProjectScraperStats)
            {
                stats_and_verified_beacons.mScraperStats[entry2.first] = entry2.second;
            }
        }
    }

    ProcessNetworkWideFromProjectStats(mBeaconMap, stats_and_verified_beacons.mScraperStats);

    _log(logattribute::INFO, "GetScraperStatsFromSingleManifest", "Completed stats processing");

    return stats_and_verified_beacons;
}

/***********************
* Scraper networking   *
************************/

bool ScraperSaveCScraperManifestToFiles(uint256 nManifestHash)
{
    // Check to see if the hash exists in the manifest map, and if not, bail.
    LOCK(CScraperManifest::cs_mapManifest);
    _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

    // Select manifest based on provided hash.
    auto pair = CScraperManifest::mapManifest.find(nManifestHash);

    if (pair == CScraperManifest::mapManifest.end())
    {
        _log(logattribute::ERR, "ScraperSaveCScraperManifestToFiles", "Specified manifest hash does not exist. Save unsuccessful.");
        return false;
    }

    // Make sure the Scraper directory itself exists, because this function could be called from outside
    // the scraper thread loop, and therefore the directory may not have been set up yet.
    {
        LOCK(cs_Scraper);
        _log(logattribute::INFO, "LOCK", "cs_Scraper");

        ScraperDirectoryAndConfigSanity();

        // End LOCK(cs_Scraper).
        _log(logattribute::INFO, "ENDLOCK", "cs_Scraper");
    }

    fs::path savepath = pathScraper / "manifest_dump";

    // Check to see if the Scraper manifest_dump directory exists and is a directory. If not create it.
    if (fs::exists(savepath))
    {
        // If it is a normal file, this is not right. Remove the file and replace with the directory.
        if (fs::is_regular_file(savepath))
        {
            fs::remove(savepath);
            fs::create_directory(savepath);
        }
    }
    else
        fs::create_directory(savepath);

    // Add on the hash subdirectory to the path. 7 digits of the hash is good enough.
    savepath = savepath / nManifestHash.GetHex().substr(0, 7);

    // Check to see if the Scraper manifest_dump/hash directory exists and is a directory. If not create it.
    if (fs::exists(savepath))
    {
        // If it is a normal file, this is not right. Remove the file and replace with the directory.
        if (fs::is_regular_file(savepath))
        {
            fs::remove(savepath);
            fs::create_directory(savepath);
        }
    }
    else
        fs::create_directory(savepath);

    // This is from the map find above.
    const CScraperManifest& manifest = *pair->second;

    // Write out to files the parts. Note this assumes one-to-one part to file. Needs to
    // be fixed for more than one part per file.
    int iPartNum = 0;
    for (const auto& iter : manifest.vParts)
    {
        std::string outputfile;
        fs::path outputfilewpath;

        if (iPartNum == 0)
            outputfile = "BeaconList.csv.gz";
        else
            outputfile = manifest.projects[iPartNum-1].project + "-" + manifest.projects[iPartNum-1].ETag + ".csv.gz";

        outputfilewpath = savepath / outputfile;

        fsbridge::ofstream outfile(outputfilewpath, std::ios_base::out | std::ios_base::binary);

        if (!outfile)
        {
            _log(logattribute::ERR, "ScraperSaveCScraperManifestToFiles", "Failed to open file (" + outputfile + ")");

            return false;
        }

        outfile.write((const char*)iter->data.data(), iter->data.size());

        outfile.flush();
        outfile.close();

        iPartNum++;
    }

    _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

    return true;
}

// The idea here is that there are two levels of authorization. The first level is whether any
// node can operate as a "scraper", in other words, download the stats files themselves.
// The second level, which is the IsScraperAuthorizedToBroadcastManifests() function,
// is to authorize a particular node to actually be able to publish manifests.
// The second function is intended to override the first, with the first being a network wide
// policy. So to be clear, if the network wide policy has IsScraperAuthorized() set to false
// then ONLY nodes that have IsScraperAuthorizedToBroadcastManifests() can download stats at all.
// If IsScraperAuthorized() is set to true, then you have two levels of operation allowed.
// Nodes can run -scraper and download stats for themselves. They will only be able to publish
// manifests if for that node IsScraperAuthorizedToBroadcastManifests() evaluates to true.
// This allows flexibility in network policy, and will allow us to convert from a scraper based
// approach to convergence back to individual node stats download and convergence without a lot of
// headaches.
bool IsScraperAuthorized()
{
    return ALLOW_NONSCRAPER_NODE_STATS_DOWNLOAD;
}

// This checks to see if the local node is authorized to publish manifests. Note that this code could be
// modified to bypass this check, so messages sent will also be validated on receipt by the complement
// to this function, IsManifestAuthorized(CKey& Key) in the CScraperManifest class.
bool IsScraperAuthorizedToBroadcastManifests(CBitcoinAddress& AddressOut, CKey& KeyOut)
{

    AppCacheSection mScrapers = {};
    {
        LOCK(cs_main);
        _log(logattribute::INFO, "LOCK", "cs_main");

        mScrapers = ReadCacheSection(Section::SCRAPER);

        _log(logattribute::INFO, "ENDLOCK", "cs_main");
    }

    std::string sScraperAddressFromConfig = GetArg("-scraperkey", "false");

    // Check against the -scraperkey config entry first and return quickly to avoid extra work.
    // If the config entry exists and is in the map (i.e. in the appcache)...
    auto entry = mScrapers.find(sScraperAddressFromConfig);

    // If the address (entry) exists in the config and appcache...
    if (sScraperAddressFromConfig != "false" && entry != mScrapers.end())
    {
        _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "Entry from config/command line found in AppCache.");

        // ... and is enabled...
        if (entry->second.value == "true" || entry->second.value == "1")
        {
            _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "Entry in appcache is enabled.");

            CBitcoinAddress address(sScraperAddressFromConfig);
            //CPubKey ScraperPubKey(ParseHex(sScraperAddressFromConfig));

            CKeyID KeyID;
            address.GetKeyID(KeyID);

            // ... and the address is valid...
            if (address.IsValid())
            {
                _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "The address is valid.");
                _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "(Doublecheck) The address is " + address.ToString());

                // ... and it exists in the wallet...
                LOCK(pwalletMain->cs_wallet);
                _log(logattribute::INFO, "LOCK", "pwalletMain->cs_wallet");

                if (pwalletMain->GetKey(KeyID, KeyOut))
                {
                    // ... and the key returned from the wallet is valid and matches the provided public key...
                    assert(KeyOut.IsValid());

                    _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "The wallet key for the address is valid.");

                    AddressOut = address;

                    // Note that KeyOut here will have the correct key to use by THIS node.

                    _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests",
                         "Found address " + sScraperAddressFromConfig + " in both the wallet and appcache. \n"
                         "This scraper is authorized to publish manifests.");

                    _log(logattribute::INFO, "ENDLOCK", "pwalletMain->cs_wallet");
                    return true;
                }
                else
                {
                    _log(logattribute::WARNING, "IsScraperAuthorizedToBroadcastManifests",
                         "Key not found in the wallet for matching address. Please check that the wallet is unlocked "
                         "(preferably for staking only).");
                }

                _log(logattribute::INFO, "ENDLOCK", "pwalletMain->cs_wallet");
            }
        }
    }
    // If a -scraperkey config entry has not been specified, we will walk through all of the addresses in the wallet
    // until we hit the first one that is in the list.
    else
    {
        LOCK(pwalletMain->cs_wallet);
        _log(logattribute::INFO, "LOCK", "pwalletMain->cs_wallet");

        for (auto const& item : pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;

            std::string sScraperAddress = address.ToString();
            _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "Checking address " + sScraperAddress);

            entry = mScrapers.find(sScraperAddress);

            // The address is found in the appcache...
            if (entry != mScrapers.end())
            {
                _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "Entry found in AppCache");

                // ... and is enabled...
                if (entry->second.value == "true" || entry->second.value == "1")
                {
                    _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "AppCache entry enabled");

                    CKeyID KeyID;
                    address.GetKeyID(KeyID);

                    // ... and the address is valid...
                    if (address.IsValid())
                    {
                        _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "The address is valid.");
                        _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "(Doublecheck) The address is " + address.ToString());

                        // ... and it exists in the wallet... (It SHOULD here... it came from the map...)
                        if (pwalletMain->GetKey(KeyID, KeyOut))
                        {
                            // ... and the key returned from the wallet is valid ...
                            assert(KeyOut.IsValid());

                            _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests", "The wallet key for the address is valid.");

                            AddressOut = address;

                            // Note that KeyOut here will have the correct key to use by THIS node.

                            _log(logattribute::INFO, "IsScraperAuthorizedToBroadcastManifests",
                                 "Found address " + sScraperAddress + " in both the wallet and appcache. \n"
                                 "This scraper is authorized to publish manifests.");

                            _log(logattribute::INFO, "ENDLOCK", "pwalletMain->cs_wallet");
                            return true;
                        }
                        else
                        {
                            _log(logattribute::WARNING, "IsScraperAuthorizedToBroadcastManifests",
                                 "Key not found in the wallet for matching address. Please check that the wallet is unlocked "
                                 "(preferably for staking only).");
                        }
                    }
                }
            }
        }

        _log(logattribute::INFO, "ENDLOCK", "pwalletMain->cs_wallet");
    }

    // If we made it here, there is no match or valid key in the wallet

    _log(logattribute::WARNING, "IsScraperAuthorizedToBroadcastManifests", "No key found in wallet that matches authorized scrapers in appcache.");

    return false;
}


// This function computes the average time between manifests as a function of the last 10 received manifests
// plus the nTime provided as the argument. This gives ten intervals for sampling between manifests. If the
// average time between manifests is less than 50% of the nScraperSleep interval, or the most recent manifest
// for a scraper is more than five minutes in the future (accounts for clock skew) then the publishing rate
// of the scraper is deemed too high. This is actually used in CScraperManifest::IsManifestAuthorized to ban
// a scraper that is abusing the network by sending too many manifests over a very short period of time.
bool IsScraperMaximumManifestPublishingRateExceeded(int64_t& nTime, CPubKey& PubKey)
{
    mmCSManifestsBinnedByScraper mMapCSManifestBinnedByScraper;

    {
        LOCK(CScraperManifest::cs_mapManifest);

        mMapCSManifestBinnedByScraper = BinCScraperManifestsByScraper();
    }

    CKeyID ManifestKeyID = PubKey.GetID();

    CBitcoinAddress ManifestAddress;
    ManifestAddress.Set(ManifestKeyID);

    // This is the address corresponding to the manifest public key, and is the scraper ID key in the outer map.
    std::string sManifestAddress = ManifestAddress.ToString();

    const auto& iScraper = mMapCSManifestBinnedByScraper.find(sManifestAddress);

    if (iScraper == mMapCSManifestBinnedByScraper.end())
    {
        // There are no previous manifests on this node corresponding to the supplied public key.
        return false;
    }

    unsigned int nIntervals = 0;
    int64_t nCurrentTime = GetAdjustedTime();
    int64_t nBeginTime = 0;
    int64_t nEndTime = 0;
    int64_t nTotalTime = 0;
    int64_t nAvgTimeBetweenManifests = 0;

    std::multimap<int64_t, ScraperID, std::greater <int64_t>> mScraperManifests;

    // Insert manifest referenced by the argument first (the "incoming" manifest). Note that it may NOT have the most recent time.
    // This is followed by the rest so that we have a unified map with the incoming in the right order.
    mScraperManifests.insert(std::make_pair(nTime, sManifestAddress));

    // Insert the rest of the manifests for the scraper matching the public key.
    for (const auto& iManifest : iScraper->second)
    {
        mScraperManifests.insert(std::make_pair(iManifest.first, sManifestAddress));
    }

    // Remember the map is sorted in descending order of time, so the end comes before the beginning.
    for (const auto& iManifest : mScraperManifests)
    {
        ++nIntervals;

        if (nIntervals == 1)
        {
            // Set the end of the measurement interval to the most recent manifest for the scraper.
            nEndTime = iManifest.first;
        }

        // Set the beginning time of the interval to the time at this element
        nBeginTime = iManifest.first;

        // Go till 10 intervals (between samples) OR time interval reaches 5 expected scraper updates at 3 nScraperSleep scraper cycles per update,
        // whichever occurs first.
        if (nIntervals == 10 || (nCurrentTime - nBeginTime) >= nScraperSleep * 3 * 5 / 1000) break;
    }

    // Do not allow the most recent manifest from a scraper to be more than five minutes into the future from GetAdjustedTime. (This takes
    // into account reasonable clock skew between the scraper and this node, but prevents future dating manifests to try and fool the rate calculation.)
    // Note that this is regardless of the minimum sample size below.
    if (nEndTime - nCurrentTime > 300)
    {
        _log(logattribute::CRITICAL, "IsScraperMaximumManifestPublishingRateExceeded", "Scraper " + sManifestAddress +
             " has published a manifest more than 5 minutes in the future. Banning the scraper.");

        return true;
    }

    // We are not going to allow less than 5 intervals in the sample. If it is less than 5 intervals, it has either passed the break
    // condition above (or even slower), or there really are very few published total, in which the sample size is too small to judge
    // the rate.
    if (nIntervals < 5) return false;


    // nTotalTime cannot be negative because of the sort order of mScraperManifests, and nIntervals is protected against being zero
    // by the above conditional.
    nTotalTime = nEndTime - nBeginTime;
    nAvgTimeBetweenManifests = nTotalTime / nIntervals;

    // nScraperSleep is in milliseconds. If the average interval is less than 25% of nScraperSleep in seconds, ban the scraper.
    // Note that this is at least a factor of 12 faster than the expected rate given usual project update velocity.
    if (nAvgTimeBetweenManifests < nScraperSleep / 4000)
    {
        _log(logattribute::CRITICAL, "IsScraperMaximumManifestPublishingRateExceeded", "Scraper " + sManifestAddress +
             " has published too many manifests in too short a time:\n" +
             "Number of manifests sampled = " + std::to_string(nIntervals + 1) + "\n"
             "nEndTime = " + std::to_string(nEndTime) + "\n" +
             "nBeginTime = " + std::to_string(nBeginTime) + "\n" +
             "nTotalTime = " + std::to_string(nTotalTime) + "\n" +
             "nAvgTimeBetweenManifests = " + std::to_string(nAvgTimeBetweenManifests) +"\n" +
             "Banning the scraper.\n");

        return true;
    }
    else
    {
        return false;
    }
}

// This function is necessary because some CScraperManifest messages are likely to be received before the wallet is in sync. Therefore, they
// cannot be checked at that time by the deserialize check. Instead, while the wallet is not in sync, the local CScraperManifest flag
// bCheckedAuthorized will be set to false on any manifests received during that time. Once the wallet is in sync, this function will be
// called and will walk the mapManifest and check all Manifests to ensure the PubKey in the manifest is in the
// authorized scraper list in the AppCache. If it passes the flag will be set to true. If it fails, the manifest will be deleted. All manifests
// must be checked, because we have to deal with another condition where a scraper is deauthorized by network policy. This means manifests may
// not be authorized even if the bCheckedAuthorized is true from a prior check.

// A lock needs to be taken on CScraperManifest::cs_mapManifest before calling this function.
unsigned int ScraperDeleteUnauthorizedCScraperManifests()
{
    unsigned int nDeleted = 0;

    for (auto iter = CScraperManifest::mapManifest.begin(); iter != CScraperManifest::mapManifest.end(); )
    {
        CScraperManifest& manifest = *iter->second;

        // We are not going to do anything with the banscore here, but it is an out parameter of IsManifestAuthorized.
        unsigned int banscore_out = 0;

        if (CScraperManifest::IsManifestAuthorized(manifest.nTime, manifest.pubkey, banscore_out))
        {
            manifest.bCheckedAuthorized = true;
            ++iter;
        }
        else
        {
            _log(logattribute::WARNING, "ScraperDeleteUnauthorizedCScraperManifests", "Deleting unauthorized manifest with hash " + iter->first.GetHex());
            // Delete from CScraperManifest map (also advances iter to the next valid element). Immediate flag is set, because there should be
            // no pending delete retention grace for this.
            iter = CScraperManifest::DeleteManifest(iter, true);
            nDeleted++;
        }
    }

    return nDeleted;
}


// A lock needs to be taken on cs_StructScraperFileManifest for this function.
// The sCManifestName is the public key of the scraper in address form.
bool ScraperSendFileManifestContents(CBitcoinAddress& Address, CKey& Key)
{
    // This "broadcasts" the current ScraperFileManifest contents to the network.

    auto manifest = std::unique_ptr<CScraperManifest>(new CScraperManifest());

    // The manifest name is the authorized address of the scraper.
    manifest->sCManifestName = Address.ToString();

    // Also store local sCManifestName, because the manifest will be std::moved by addManifest.
    std::string sCManifestName = Address.ToString();

    manifest->nTime = StructScraperFileManifest.timestamp;

    // Also store local nTime, because the manifest will be std::moved by addManifest.
    int64_t nTime = StructScraperFileManifest.timestamp;

    manifest->ConsensusBlock = StructScraperFileManifest.nConsensusBlockHash;

    // This will have to be changed to support files bigger than 32 MB, where more than one
    // part per object will be required.
    int iPartNum = 0;

    // Inject the BeaconList part.
    {
        // Read in BeaconList
        fs::path inputfile = "BeaconList.csv.gz";
        fs::path inputfilewpath = pathScraper / inputfile;

        // open input file, and associate with CAutoFile
        FILE *file = fsbridge::fopen(inputfilewpath, "rb");
        CAutoFile filein(file, SER_DISK, CLIENT_VERSION);

        if (filein.IsNull())
        {
            _log(logattribute::ERR, "ScraperSendFileManifestContents", "Failed to open file (" + inputfile.string() + ")");
            return false;
        }

        // use file size to size memory buffer
        int dataSize = boost::filesystem::file_size(inputfilewpath);
        std::vector<unsigned char> vchData;
        vchData.resize(dataSize);

        // read data from file
        try
        {
            filein.read((char *)&vchData[0], dataSize);
        }
        catch (std::exception &e)
        {
            _log(logattribute::ERR, "ScraperSendFileManifestContents", "Failed to read file (" + inputfile.string() + ")");
            return false;
        }

        filein.fclose();

        // The first part number will be the BeaconList.
        manifest->BeaconList = iPartNum;
        manifest->BeaconList_c = 0;

        CDataStream part(std::move(vchData), SER_NETWORK, 1);

        manifest->addPartData(std::move(part));

        iPartNum++;
    }

    // Inject the VerifiedBeaconList as a "project" called VerifiedBeacons. This is inelegant, but
    // will maintain compatibility with older nodes. The older nodes will simply ignore this extra part
    // because it will never match any whitelisted project. Only include it if it is not empty.
    {
        LOCK(cs_VerifiedBeacons);
        _log(logattribute::INFO, "LOCK", "cs_VerifiedBeacons");

        ScraperVerifiedBeacons& ScraperVerifiedBeacons = GetVerifiedBeacons();

        if (!ScraperVerifiedBeacons.mVerifiedMap.empty())
        {
            CScraperManifest::dentry ProjectEntry;

            ProjectEntry.project = "VerifiedBeacons";
            ProjectEntry.LastModified = ScraperVerifiedBeacons.timestamp;
            ProjectEntry.current = true;

            // For now each object will only have one part.
            ProjectEntry.part1 = iPartNum;
            ProjectEntry.partc = 0;
            ProjectEntry.GridcoinTeamID = -1; //Not used anymore

            ProjectEntry.last = 1;

            manifest->projects.push_back(ProjectEntry);

            CDataStream part(SER_NETWORK, 1);

            part << ScraperVerifiedBeacons.mVerifiedMap;

            manifest->addPartData(std::move(part));

            iPartNum++;
        }

        _log(logattribute::INFO, "ENDLOCK", "cs_VerifiedBeacons");
    }


    for (auto const& entry : StructScraperFileManifest.mScraperFileManifest)
    {
        // If SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES is false, only include current files to send across the network.
        // Also continue (exclude) if it is a non-publishable entry (excludefromcsmanifest is true).
        if ((!SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES && !entry.second.current) || entry.second.excludefromcsmanifest)
            continue;

        fs::path inputfile = entry.first;

        //_log(logattribute::INFO, "ScraperSendFileManifestContents", "Input file for CScraperManifest is " + inputfile.string());

        fs::path inputfilewpath = pathScraper / inputfile;

        // open input file, and associate with CAutoFile
        FILE *file = fsbridge::fopen(inputfilewpath, "rb");
        CAutoFile filein(file, SER_DISK, CLIENT_VERSION);

        if (filein.IsNull())
        {
            _log(logattribute::ERR, "ScraperSendFileManifestContents", "Failed to open file (" + inputfile.string() + ")");
            return false;
        }

        // use file size to size memory buffer
        int dataSize = boost::filesystem::file_size(inputfilewpath);
        std::vector<unsigned char> vchData;
        vchData.resize(dataSize);

        // read data from file
        try
        {
            filein.read((char *)&vchData[0], dataSize);
        }
        catch (std::exception &e)
        {
            _log(logattribute::ERR, "ScraperSendFileManifestContents", "Failed to read file (" + inputfile.string() + ")");
            return false;
        }

        filein.fclose();


        CScraperManifest::dentry ProjectEntry;

        ProjectEntry.project = entry.second.project;
        std::string sProject = entry.second.project + "-";

        std::string sinputfile = inputfile.string();
        std::string suffix = ".csv.gz";

        // Remove project-
        sinputfile.erase(sinputfile.find(sProject), sProject.length());
        // Remove suffix. What is left is the ETag.
        ProjectEntry.ETag = sinputfile.erase(sinputfile.find(suffix), suffix.length());

        ProjectEntry.LastModified = entry.second.timestamp;

        // For now each object will only have one part.
        ProjectEntry.part1 = iPartNum;
        ProjectEntry.partc = 0;
        ProjectEntry.GridcoinTeamID = -1; //Not used anymore

        ProjectEntry.current = entry.second.current;

        ProjectEntry.last = 1;

        manifest->projects.push_back(ProjectEntry);

        CDataStream part(vchData, SER_NETWORK, 1);

        manifest->addPartData(std::move(part));

        iPartNum++;
    }

    // "Sign" and "send".

    LOCK2(CScraperManifest::cs_mapManifest, CSplitBlob::cs_mapParts);

    bool bAddManifestSuccessful = CScraperManifest::addManifest(std::move(manifest), Key);

    if (bAddManifestSuccessful)
        _log(logattribute::INFO, "ScraperSendFileManifestContents", "addManifest (send) from this scraper (address "
             + sCManifestName + ") successful, timestamp "
             + DateTimeStrFormat("%x %H:%M:%S", nTime) + " with " + std::to_string(iPartNum) + " parts.");
    else
        _log(logattribute::ERR, "ScraperSendFileManifestContents", "addManifest (send) from this scraper (address "
             + sCManifestName + ") FAILED, timestamp "
             + DateTimeStrFormat("%x %H:%M:%S", nTime));

    return bAddManifestSuccessful;
}


// ------------------------------------ This an out parameter.
bool ScraperConstructConvergedManifest(ConvergedManifest& StructConvergedManifest)
{
    bool bConvergenceSuccessful = false;

    // Call ScraperDeleteCScraperManifests() to ensure we have culled old manifests. This will
    // return a map of manifests binned by Scraper after the culling.
    mmCSManifestsBinnedByScraper mMapCSManifestsBinnedByScraper = ScraperCullAndBinCScraperManifests();
    
    // Do a map for unique manifest times ordered by descending time then content hash.
    std::multimap<int64_t, uint256, std::greater<int64_t>> mManifestsBinnedByTime;
    // and also by content hash, then scraperID and manifest (not content) hash.
    std::multimap<uint256, std::pair<ScraperID, uint256>> mManifestsBinnedbyContent;
    std::multimap<uint256, std::pair<ScraperID, uint256>>::iterator convergence;
    
    unsigned int nScraperCount = mMapCSManifestsBinnedByScraper.size();

    _log(logattribute::INFO, "ScraperConstructConvergedManifest", "Number of Scrapers with manifests = " + std::to_string(nScraperCount));

    for (const auto& iter : mMapCSManifestsBinnedByScraper)
    {
        // iter.second is the mCSManifest
        for (const auto& iter_inner : iter.second)
        {
            // Insert into mManifestsBinnedByTime multimap. Iter_inner.first is the manifest time,
            // iter_inner.second.second is the manifest CONTENT hash.
            mManifestsBinnedByTime.insert(std::make_pair(iter_inner.first, iter_inner.second.second));

            // Even though this is a multimap on purpose because we are going to count occurances of the same key,
            // We need to prevent the insertion of a second entry with the same content from the same scraper. This
            // could otherwise happen if a scraper is shutdown and restarted, and it publishes a new manifest
            // before it receives manifests from the other nodes (including its own prior manifests).
            // ------------------------------------------------  manifest CONTENT hash
            auto range = mManifestsBinnedbyContent.equal_range(iter_inner.second.second);
            bool bAlreadyExists = false;
            for (auto iter3 = range.first; iter3 != range.second; ++iter3)
            {
                // ---- ScraperID ------ Candidate scraperID to insert
                if (iter3->second.first == iter.first)
                    bAlreadyExists = true;
            }

            if (!bAlreadyExists)
            {
                // Insert into mManifestsBinnedbyContent ------------- content hash --------------------- ScraperID ------ manifest hash.
                mManifestsBinnedbyContent.insert(std::make_pair(iter_inner.second.second, std::make_pair(iter.first, iter_inner.second.first)));
                _log(logattribute::INFO, "ScraperConstructConvergedManifest", "mManifestsBinnedbyContent insert, timestamp "
                                  + DateTimeStrFormat("%x %H:%M:%S", iter_inner.first)
                                  + ", content hash "+ iter_inner.second.second.GetHex()
                                  + ", scraper ID " + iter.first
                                  + ", manifest hash " + iter_inner.second.first.GetHex());
            }
        }
    }
    
    // Walk the time map (backwards in time because the sort order is descending), and select the first
    // manifest content hash that meets the convergence rule.
    for (const auto& iter : mManifestsBinnedByTime)
    {
        // Notice the below is NOT using the time. We switch to the content only. The time is only used to make sure
        // we test the convergence of the manifests in time order, but once a content hash is selected based on the time,
        // only the content hash is used to count occurrences in the multimap, because the times for the same
        // content hash manifest will be different across different scrapers.
        unsigned int nIdenticalContentManifestCount = mManifestsBinnedbyContent.count(iter.second);
        if (nIdenticalContentManifestCount >= NumScrapersForSupermajority(nScraperCount))
        {
            // Find the first one of equivalent content manifests.
            convergence = mManifestsBinnedbyContent.find(iter.second);

            _log(logattribute::INFO, "ScraperConstructConvergedManifest", "Found convergence on manifest " + convergence->second.second.GetHex()
                 + " at " + DateTimeStrFormat("%x %H:%M:%S",  iter.first)
                 + " with " + std::to_string(nIdenticalContentManifestCount) + " scrapers out of " + std::to_string(nScraperCount)
                 + " agreeing.");

            _log(logattribute::INFO, "ScraperConstructConvergedManifest", "Content hash " + iter.second.GetHex());

            auto ConvergenceRange = mManifestsBinnedbyContent.equal_range(iter.second);

            // Record included scrapers in convergence.
            for (auto iter2 = ConvergenceRange.first; iter2 != ConvergenceRange.second; ++iter2)
            {
                // -------------------------------------------------  ScraperID ------------ manifest hash
                StructConvergedManifest.mIncludedScraperManifests[iter2->second.first] = iter2->second.second;
            }

            // Record scrapers that are not part of the convergence by iterating through the top level of the double map (which is keyed by ScraperID)
            for (const auto& iScraper : mMapCSManifestsBinnedByScraper)
            {
                // If the scraper is not found in the mIncludedScraperManifests, then it was not part of the convergence.
                if (StructConvergedManifest.mIncludedScraperManifests.find(iScraper.first) == StructConvergedManifest.mIncludedScraperManifests.end())
                {
                    StructConvergedManifest.vExcludedScrapers.push_back(iScraper.first);
                    _log(logattribute::INFO, "ScraperConstructConvergedManifest", "Scraper " + iScraper.first + " not in convergence.");
                }
                else
                {
                    StructConvergedManifest.vIncludedScrapers.push_back(iScraper.first);
                    // Scraper was in the convergence.
                    _log(logattribute::INFO, "ScraperConstructConvergedManifest", "Scraper " + iScraper.first + " in convergence.");
                }
            }

            // Retrieve the complete list of scrapers from the AppCache to determine scrapers not publishing at all.
            AppCacheSection mScrapers = ReadCacheSection(Section::SCRAPER);

            for (const auto& iScraper : mScrapers)
            {
                // Only include scrapers enabled in protocol.

                if (iScraper.second.value == "true" || iScraper.second.value == "1")
                {
                    if (std::find(std::begin(StructConvergedManifest.vExcludedScrapers), std::end(StructConvergedManifest.vExcludedScrapers), iScraper.first)
                            == std::end(StructConvergedManifest.vExcludedScrapers)
                        && std::find(std::begin(StructConvergedManifest.vIncludedScrapers), std::end(StructConvergedManifest.vIncludedScrapers), iScraper.first)
                            == std::end(StructConvergedManifest.vIncludedScrapers))
                    {
                         StructConvergedManifest.vScrapersNotPublishing.push_back(iScraper.first);
                         _log(logattribute::INFO, "ScraperConstructConvergedManifest", "Scraper " + iScraper.first + " authorized but not publishing.");
                    }
                }
            }

            bConvergenceSuccessful = true;

            // Note this break is VERY important, it prevents considering essentially the same manifest that meets convergence multiple times.
            break;
        }
    }

    // Get a read-only view of the current project whitelist to fill out the
    // excluded projects vector later on:
    const NN::WhitelistSnapshot projectWhitelist = NN::GetWhitelist().Snapshot();

    if (bConvergenceSuccessful)
    {
        LOCK(CScraperManifest::cs_mapManifest);
        _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

        // Select agreed upon (converged) CScraper manifest based on converged hash.
        auto pair = CScraperManifest::mapManifest.find(convergence->second.second);
        const CScraperManifest& manifest = *pair->second;

        // Fill out the ConvergedManifest structure. Note this assumes one-to-one part to project statistics BLOB. Needs to
        // be fixed for more than one part per BLOB. This is easy in this case, because it is all from/referring to one manifest.

        StructConvergedManifest.ConsensusBlock = manifest.ConsensusBlock;
        StructConvergedManifest.timestamp = GetAdjustedTime();
        StructConvergedManifest.bByParts = false;

        int iPartNum = 0;
        CDataStream ss(SER_NETWORK,1);
        WriteCompactSize(ss, manifest.vParts.size());
        uint256 nContentHashCheck;

        for (const auto& iter : manifest.vParts)
        {
            std::string sProject;

            if (iPartNum == 0)
                sProject = "BeaconList";
            else
                sProject = manifest.projects[iPartNum-1].project;

            // Copy the parts data into the map keyed by project. This will also pick up the
            // VerifiedBeacons "project" if present.
            StructConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair(sProject, iter->data));

            // Serialize the hash to doublecheck the content hash.
            ss << iter->hash;

            iPartNum++;
        }
        ss << StructConvergedManifest.ConsensusBlock;

        nContentHashCheck = Hash(ss.begin(), ss.end());

        if (nContentHashCheck != convergence->first)
        {
            bConvergenceSuccessful = false;
            _log(logattribute::ERR, "ScraperConstructConvergedManifest", "Selected Converged Manifest content hash check failed! nContentHashCheck = "
                 + nContentHashCheck.GetHex() + " and nContentHash = " + StructConvergedManifest.nContentHash.GetHex());
            // Reinitialize StructConvergedManifest
            StructConvergedManifest = {};
        }
        else // Content matches so we have a confirmed convergence.
        {
            // Copy the MANIFEST content hash into the ConvergedManifest.
            StructConvergedManifest.nUnderlyingManifestContentHash = convergence->first;

            // The ConvergedManifest content hash is NOT the same as the hash above from the CScraper::manifest, because it needs to be in the order of the
            // map key and on the data, not the order of vParts by the part hash. So, unfortunately, we have to walk through the map again to hash it correctly.
            CDataStream ss2(SER_NETWORK,1);
            for (const auto& iter : StructConvergedManifest.ConvergedManifestPartsMap)
                ss2 << iter.second;

            StructConvergedManifest.nContentHash = Hash(ss2.begin(), ss2.end());

            // Determine if there is an excluded project. If so, set convergence back to false and drop back to project level to try and recover project by project.
            for (const auto& iProjects : projectWhitelist)
            {
                if (StructConvergedManifest.ConvergedManifestPartsMap.find(iProjects.m_name) == StructConvergedManifest.ConvergedManifestPartsMap.end())
                {
                    _log(logattribute::WARNING, "ScraperConstructConvergedManifest", "Project "
                         + iProjects.m_name
                         + " was excluded because the converged manifests from the scrapers all excluded the project. \n"
                         + "Falling back to attempt convergence by project to try and recover excluded project.");
                    
                    bConvergenceSuccessful = false;
                    
                    // Since we are falling back to project level and discarding this convergence, no need to process any more once one missed project is found.
                    break;
                }

                if (StructConvergedManifest.ConvergedManifestPartsMap.find("BeaconList") == StructConvergedManifest.ConvergedManifestPartsMap.end())
                {
                    _log(logattribute::WARNING, "ScraperConstructConvergedManifest", "BeaconList was not found in the converged manifests from the scrapers. \n"
                         "Falling back to attempt convergence by project.");

                    bConvergenceSuccessful = false;

                    // Since we are falling back to project level and discarding this convergence, no need to process any more if BeaconList is missing.
                    break;
                }
            }
        }

        _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");
    }

    if (!bConvergenceSuccessful)
    {
        _log(logattribute::INFO, "ScraperConstructConvergedManifest", "No convergence on manifests by content at the manifest level.");

        // Reinitialize StructConvergedManifest
        StructConvergedManifest = {};

        // Try to form a convergence by project objects (parts)...
        bConvergenceSuccessful = ScraperConstructConvergedManifestByProject(projectWhitelist, mMapCSManifestsBinnedByScraper, StructConvergedManifest);

        // If we have reached here. All attempts at convergence have failed. Reinitialize StructConvergedManifest to eliminate stale or
        // partially filled-in data.
        if (!bConvergenceSuccessful)
            StructConvergedManifest = {};
    }

    // Signal UI of the status of convergence attempt.
    if (bConvergenceSuccessful)
        uiInterface.NotifyScraperEvent(scrapereventtypes::Convergence, CT_NEW, {});
    else
        uiInterface.NotifyScraperEvent(scrapereventtypes::Convergence, CT_DELETED, {});

    return bConvergenceSuccessful;
    
}

// Subordinate function to ScraperConstructConvergedManifest to try to find a convergence at the Project (part) level
// if there is no convergence at the manifest level.
// ------------------------------------------------------------------------ In ------------------------------------------------- Out
bool ScraperConstructConvergedManifestByProject(const NN::WhitelistSnapshot& projectWhitelist,
                                                mmCSManifestsBinnedByScraper& mMapCSManifestsBinnedByScraper, ConvergedManifest& StructConvergedManifest)
{
    bool bConvergenceSuccessful = false;

    CDataStream ss(SER_NETWORK,1);
    uint256 nConvergedConsensusBlock;
    int64_t nConvergedConsensusTime = 0;
    uint256 nManifestHashForConvergedBeaconList;

    // We are going to do this for each project in the whitelist.
    unsigned int iCountSuccessfulConvergedProjects = 0;
    unsigned int nScraperCount = mMapCSManifestsBinnedByScraper.size();

    _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "Number of Scrapers with manifests = " + std::to_string(nScraperCount));

    for (const auto& iWhitelistProject : projectWhitelist)
    {
        // Do a map for unique ProjectObject times ordered by descending time then content hash. Note that for Project Objects (Parts),
        // the content hash is the object hash. We also need the consensus block here, because we are "composing" the manifest by
        // parts, so we will need to choose the latest consensus block by manifest time. This will occur naturally below if tracked in
        // this manner. We will also want the BeaconList from the associated manifest.
        // ------ manifest time --- object hash - consensus block hash - manifest hash.
        std::multimap<int64_t, std::tuple<uint256, uint256, uint256>, std::greater<int64_t>> mProjectObjectsBinnedByTime;
        // and also by project object (content) hash, then scraperID and project.
        std::multimap<uint256, std::pair<ScraperID, std::string>> mProjectObjectsBinnedbyContent;
        std::multimap<uint256, std::pair<ScraperID, std::string>>::iterator ProjectConvergence;

        {
            LOCK(CScraperManifest::cs_mapManifest);
            _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

            // For the selected project in the whitelist, walk each scraper.
            for (const auto& iter : mMapCSManifestsBinnedByScraper)
            {
                // iter.second is the mCSManifest. Walk each manifest in each scraper.
                for (const auto& iter_inner : iter.second)
                {
                    // This is the referenced CScraperManifest hash
                    uint256 nCSManifestHash = iter_inner.second.first;

                    // Select manifest based on provided hash.
                    auto pair = CScraperManifest::mapManifest.find(nCSManifestHash);
                    CScraperManifest& manifest = *pair->second;

                    // Find the part number in the manifest that corresponds to the whitelisted project.
                    // Once we find a part that corresponds to the selected project in the given manifest, then break,
                    // because there can only be one part in a manifest corresponding to a given project.
                    int nPart = -1;
                    int64_t nProjectObjectTime = 0;
                    uint256 nProjectObjectHash;
                    for (const auto& vectoriter : manifest.projects)
                    {
                        if (vectoriter.project == iWhitelistProject.m_name)
                        {
                            nPart = vectoriter.part1;
                            nProjectObjectTime = vectoriter.LastModified;
                            break;
                        }
                    }

                    // Part -1 means not found, Part 0 is the beacon list, so needs to be greater than zero.
                    if (nPart > 0)
                    {
                        // Get the hash of the part referenced in the manifest.
                        nProjectObjectHash = manifest.vParts[nPart]->hash;

                        // Insert into mManifestsBinnedByTime multimap.
                        mProjectObjectsBinnedByTime.insert(std::make_pair(nProjectObjectTime, std::make_tuple(nProjectObjectHash, manifest.ConsensusBlock, *manifest.phash)));

                        // Even though this is a multimap on purpose because we are going to count occurances of the same key,
                        // We need to prevent the insertion of a second entry with the same content from the same scraper. This is
                        // even more true here at the part level than at the manifest level, because if both SCRAPER_CMANIFEST_RETAIN_NONCURRENT
                        // and SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES are true, then there can be many references
                        // to the same part by different manifests of the same scraper in addition to across scrapers.
                        auto range = mProjectObjectsBinnedbyContent.equal_range(nProjectObjectHash);
                        bool bAlreadyExists = false;
                        for (auto iter3 = range.first; iter3 != range.second; ++iter3)
                        {
                            // ---- ScraperID ------ Candidate scraperID to insert
                            if (iter3->second.first == iter.first)
                                bAlreadyExists = true;
                        }

                        if (!bAlreadyExists)
                        {
                            // Insert into mProjectObjectsBinnedbyContent -------- content hash ------------------- ScraperID -------- Project.
                            mProjectObjectsBinnedbyContent.insert(std::make_pair(nProjectObjectHash, std::make_pair(iter.first, iWhitelistProject.m_name)));
                            //_log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "mProjectObjectsBinnedbyContent insert "
                            //                 + nProjectObjectHash.GetHex() + ", " + iter.first + ", " + iWhitelistProject.m_name);
                            _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "mProjectObjectsBinnedbyContent insert, timestamp "
                                              + DateTimeStrFormat("%x %H:%M:%S", manifest.nTime)
                                              + ", content hash "+ nProjectObjectHash.GetHex()
                                              + ", scraper ID " + iter.first
                                              + ", project " + iWhitelistProject.m_name
                                              + ", manifest hash " + nCSManifestHash.GetHex());

                        }
                    }
                }
            }
            _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");
        }

        // Walk the time map (backwards in time because the sort order is descending), and select the first
        // Project Part (Object) content hash that meets the convergence rule.
        for (const auto& iter : mProjectObjectsBinnedByTime)
        {
            // Notice the below is NOT using the time. We switch to the content only. The time is only used to make sure
            // we test the convergence of the project objects in time order, but once a content hash is selected based on the time,
            // only the content hash is used to count occurrences in the multimap, because the times for the same
            // project object (part hash) will be different across different manifests and different scrapers.
            unsigned int nIdenticalContentManifestCount = mProjectObjectsBinnedbyContent.count(std::get<0>(iter.second));
            if (nIdenticalContentManifestCount >= NumScrapersForSupermajority(nScraperCount))
            {
                // Find the first one of equivalent parts ------------------ by object hash.
                ProjectConvergence = mProjectObjectsBinnedbyContent.find(std::get<0>(iter.second));

                _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "Found convergence on project object " + ProjectConvergence->first.GetHex()
                     + " for project " + iWhitelistProject.m_name
                     + " with " + std::to_string(nIdenticalContentManifestCount) + " scrapers out of " + std::to_string(nScraperCount)
                     + " agreeing.");

                // Get the actual part ----------------- by object hash.
                auto iPart = CSplitBlob::mapParts.find(std::get<0>(iter.second));

                uint256 nContentHashCheck = Hash(iPart->second.data.begin(), iPart->second.data.end());

                if (nContentHashCheck != iPart->first)
                {
                    _log(logattribute::ERR, "ScraperConstructConvergedManifestByProject", "Selected Converged Project Object content hash check failed! nContentHashCheck = "
                         + nContentHashCheck.GetHex() + " and nContentHash = " + iPart->first.GetHex());
                    break;
                }

                auto ProjectConvergenceRange = mProjectObjectsBinnedbyContent.equal_range(std::get<0>(iter.second));

                // Record included scrapers included for the project level convergence keyed by project and the reverse. A multimap is convenient here for both.
                for (auto iter2 = ProjectConvergenceRange.first; iter2 != ProjectConvergenceRange.second; ++iter2)
                {
                    // ------------------------------------------------------------------------- project -------------- ScraperID.
                    StructConvergedManifest.mIncludedScrapersbyProject.insert(std::make_pair(iter2->second.second, iter2->second.first));
                    // ------------------------------------------------------------------------ ScraperID -------------- project.
                    StructConvergedManifest.mIncludedProjectsbyScraper.insert(std::make_pair(iter2->second.first, iter2->second.second));
                }


                // Put Project Object (Part) in StructConvergedManifest keyed by project.
                StructConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair(iWhitelistProject.m_name, iPart->second.data));

                // If the indirectly referenced manifest has a consensus time that is greater than already recorded, replace with that time, and also
                // change the consensus block to the referred to consensus block. (Note that this is scoped at even above the individual project level, so
                // the result after iterating through all projects will be the latest manifest time and consensus block that corresponds to any of the
                // parts that meet convergence.) We will also get the manifest hash too, so we can retrieve the associated BeaconList that was used.
                if (iter.first > nConvergedConsensusTime)
                {
                    nConvergedConsensusTime = iter.first;
                    nConvergedConsensusBlock = std::get<1>(iter.second);
                    nManifestHashForConvergedBeaconList = std::get<2>(iter.second);
                }

                iCountSuccessfulConvergedProjects++;

                // Note this break is VERY important, it prevents considering essentially the same project object that meets convergence multiple times.
                break;
            }
        }
    }

    // If we meet the rule of CONVERGENCE_BY_PROJECT_RATIO, then proceed to fill out the rest of the map.
    if ((double)iCountSuccessfulConvergedProjects / (double)projectWhitelist.size() >= CONVERGENCE_BY_PROJECT_RATIO)
    {
        // Fill out the the rest of the ConvergedManifest structure. Note this assumes one-to-one part to project statistics BLOB. Needs to
        // be fixed for more than one part per BLOB. This is easy in this case, because it is all from/referring to one manifest.

        // Lets use the BeaconList from the manifest referred to by nManifestHashForConvergedBeaconList. Technically there is no exact answer to
        // the BeaconList that should be used in the convergence when putting it together at the individual part level, because each project part
        // could have used a different BeaconList (subject to the consensus ladder. It makes sense to use the "newest" one that is associated
        // with a manifest that has the newest part associated with a successful part (project) level convergence.

        LOCK(CScraperManifest::cs_mapManifest);
        _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

        // Select manifest based on provided hash.
        auto pair = CScraperManifest::mapManifest.find(nManifestHashForConvergedBeaconList);
        CScraperManifest& manifest = *pair->second;

        // Bail if BeaconList is not found or empty.
        if (pair == CScraperManifest::mapManifest.end() || manifest.vParts[0]->data.size() == 0)
        {
            _log(logattribute::WARNING, "ScraperConstructConvergedManifestByProject", "BeaconList was not found in the converged manifests from the scrapers. \n"
                 "Falling back to attempt convergence by project.");

            bConvergenceSuccessful = false;
        }
        else
        {
            // The vParts[0] is always the BeaconList.
            StructConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair("BeaconList", manifest.vParts[0]->data));

            // Also include the VerifiedBeaconList "project" if present in the parts.
            int nPart = -1;
            for (const auto& iter : manifest.projects)
            {
                if (iter.project == "VerifiedBeacons")
                {
                    nPart = iter.part1;
                    break;
                }
            }

            // The normal (active) beacon list is always part 0, so nPart from the above loop
            // must be greater than zero, or else the VerifiedBeacons was not included in the
            // manifest. Note it does NOT have to be present, but needs to be put in the
            // converged manifest if it is.
            if (nPart > 0)
            {
                StructConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair("VerifiedBeacons", manifest.vParts[nPart]->data));
            }

            StructConvergedManifest.ConsensusBlock = nConvergedConsensusBlock;

            // The ConvergedManifest content hash is in the order of the map key and on the data.
            for (const auto& iter : StructConvergedManifest.ConvergedManifestPartsMap)
                ss << iter.second;

            StructConvergedManifest.nContentHash = Hash(ss.begin(), ss.end());
            StructConvergedManifest.timestamp = GetAdjustedTime();
            StructConvergedManifest.bByParts = true;

            bConvergenceSuccessful = true;

            _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "Successful convergence by project: "
                 + std::to_string(iCountSuccessfulConvergedProjects) + " out of " + std::to_string(projectWhitelist.size())
                 + " projects at "
                 + DateTimeStrFormat("%x %H:%M:%S",  StructConvergedManifest.timestamp));

            // Fill out the the excluded projects vector and the included scraper count (by project) map
            for (const auto& iProjects : projectWhitelist)
            {
                if (StructConvergedManifest.ConvergedManifestPartsMap.find(iProjects.m_name) == StructConvergedManifest.ConvergedManifestPartsMap.end())
                {
                    // Project in whitelist was not in the map, so it goes in the exclusion vector.
                    StructConvergedManifest.vExcludedProjects.push_back(iProjects.m_name);
                    _log(logattribute::WARNING, "ScraperConstructConvergedManifestByProject", "Project "
                         + iProjects.m_name
                         + " was excluded because there was no convergence from the scrapers for this project at the project level.");

                    continue;
                }

                unsigned int nScraperConvergenceCount = StructConvergedManifest.mIncludedScrapersbyProject.count(iProjects.m_name);
                StructConvergedManifest.mScraperConvergenceCountbyProject.insert(std::make_pair(iProjects.m_name, nScraperConvergenceCount));

                _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "Project " + iProjects.m_name
                                  + ": " + std::to_string(nScraperConvergenceCount) + " scraper(s) converged");
            }

            // Fill out the included and excluded scraper vector for scrapers that did not participate in any project level convergence.
            for (const auto& iScraper : mMapCSManifestsBinnedByScraper)
            {
                if (StructConvergedManifest.mIncludedProjectsbyScraper.count(iScraper.first))
                {
                    StructConvergedManifest.vIncludedScrapers.push_back(iScraper.first);
                    _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "Scraper "
                                      + iScraper.first
                                      + " was included in one or more project level convergences.");
                }
                else
                {
                    StructConvergedManifest.vExcludedScrapers.push_back(iScraper.first);
                    _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "Scraper "
                         + iScraper.first
                         + " was excluded because it was not included in any project level convergence.");
                }
            }

            // Retrieve the complete list of scrapers from the AppCache to determine scrapers not publishing at all.
            AppCacheSection mScrapers = ReadCacheSection(Section::SCRAPER);

            for (const auto& iScraper : mScrapers)
            {
                // Only include scrapers enabled in protocol.
                if (iScraper.second.value == "true" || iScraper.second.value == "1")
                {
                    if (std::find(std::begin(StructConvergedManifest.vExcludedScrapers), std::end(StructConvergedManifest.vExcludedScrapers), iScraper.first)
                            == std::end(StructConvergedManifest.vExcludedScrapers)
                        && std::find(std::begin(StructConvergedManifest.vIncludedScrapers), std::end(StructConvergedManifest.vIncludedScrapers), iScraper.first)
                            == std::end(StructConvergedManifest.vIncludedScrapers))
                    {
                         StructConvergedManifest.vScrapersNotPublishing.push_back(iScraper.first);
                         _log(logattribute::INFO, "ScraperConstructConvergedManifesByProject", "Scraper " + iScraper.first + " authorized but not publishing.");
                    }
                }
            }

            _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");
        }
    }

    if (!bConvergenceSuccessful)
    {
        // Reinitialize StructConvergedManifest.
        StructConvergedManifest = {};

        _log(logattribute::INFO, "ScraperConstructConvergedManifestByProject", "No convergence on manifests by projects.");
    }

    return bConvergenceSuccessful;
}


// A lock should be taken on CScraperManifest::cs_Manifest before calling this function.
mmCSManifestsBinnedByScraper BinCScraperManifestsByScraper()
{
    mmCSManifestsBinnedByScraper mMapCSManifestsBinnedByScraper;

    // Make use of the ordered element feature of above map to bin by scraper and then order by manifest time.
    for (auto iter = CScraperManifest::mapManifest.begin(); iter != CScraperManifest::mapManifest.end(); ++iter)
    {
        CScraperManifest& manifest = *iter->second;

        std::string sManifestName = manifest.sCManifestName;
        int64_t nTime = manifest.nTime;
        uint256 nHash = *manifest.phash;
        uint256 nContentHash = manifest.nContentHash;

        mCSManifest mManifestInner;

        auto BinIter = mMapCSManifestsBinnedByScraper.find(sManifestName);

        // No scraper bin yet - so new insert.
        if (BinIter == mMapCSManifestsBinnedByScraper.end())
        {
            mManifestInner.insert(std::make_pair(nTime, std::make_pair(nHash, nContentHash)));
            mMapCSManifestsBinnedByScraper.insert(std::make_pair(sManifestName, mManifestInner));
        }
        else
        {
            mManifestInner = BinIter->second;
            mManifestInner.insert(std::make_pair(nTime, std::make_pair(nHash, nContentHash)));
            std::swap(mManifestInner, BinIter->second);
        }
    }
    
    return mMapCSManifestsBinnedByScraper;
}


mmCSManifestsBinnedByScraper ScraperCullAndBinCScraperManifests()
{
    // Apply the SCRAPER_CMANIFEST_RETAIN_NONCURRENT bool and if false delete any existing
    // CScraperManifests other than the current one for each scraper.

    _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "Deleting old CScraperManifests.");

    LOCK(CScraperManifest::cs_mapManifest);
    _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

    // First check for unauthorized manifests just in case a scraper has been deauthorized.
    // This is only done if in sync.
    if (!fOutOfSyncByAge)
    {
        unsigned int nDeleted = ScraperDeleteUnauthorizedCScraperManifests();
        if (nDeleted) _log(logattribute::WARNING, "ScraperDeleteCScraperManifests", "Deleted " + std::to_string(nDeleted) + " unauthorized manifests.");
    }

    // Bin by scraper and order by manifest time within scraper bin.
    mmCSManifestsBinnedByScraper mMapCSManifestsBinnedByScraper = BinCScraperManifestsByScraper();

    _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "mMapCSManifestsBinnedByScraper size = " + std::to_string(mMapCSManifestsBinnedByScraper.size()));

    if (!SCRAPER_CMANIFEST_RETAIN_NONCURRENT)
    {
        // For each scraper, delete every manifest EXCEPT the latest.
        for (auto iter = mMapCSManifestsBinnedByScraper.begin(); iter != mMapCSManifestsBinnedByScraper.end(); ++iter)
        {
            mCSManifest mManifestInner = iter->second;

            _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "mManifestInner size = " + std::to_string(mManifestInner.size()) +
                 " for " + iter->first + " scraper");

            // This preserves the LATEST CScraperManifest entry for the given scraper, because the inner map is in descending order,
            // and the first element is therefore the LATEST, and is skipped.
            for (auto iter_inner = ++mManifestInner.begin(); iter_inner != mManifestInner.end(); ++iter_inner)
            {
                
                _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "Deleting non-current manifest " + iter_inner->second.first.GetHex()
                     + " from scraper source " + iter->first);
                
                // Delete from CScraperManifest map
                CScraperManifest::DeleteManifest(iter_inner->second.first);
            }
        }
    }

    // If any CScraperManifest has exceeded SCRAPER_CMANIFEST_RETENTION_TIME, then delete.
    for (auto iter = CScraperManifest::mapManifest.begin(); iter != CScraperManifest::mapManifest.end(); )
    {
        CScraperManifest& manifest = *iter->second;
        
        if (GetAdjustedTime() - manifest.nTime > SCRAPER_CMANIFEST_RETENTION_TIME)
        {
            _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "Deleting old CScraperManifest with hash " + iter->first.GetHex());
            // Delete from CScraperManifest map
            iter = CScraperManifest::DeleteManifest(iter);
        }
        else
            ++iter;
    }

    // Also delete old entries that have exceeded retention time from the ConvergedScraperStatsCache. This follows
    // SCRAPER_CMANIFEST_RETENTION_TIME as well.
    {
        LOCK(cs_ConvergedScraperStatsCache);
        _log(logattribute::INFO, "LOCK", "cs_ConvergedScraperStatsCache");

        ConvergedScraperStatsCache.DeleteOldConvergenceFromPastConvergencesMap();

        _log(logattribute::INFO, "ENDLOCK", "cs_ConvergedScraperStatsCache");
    }

    unsigned int nPendingDeleted = 0;

    _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "Size of mapPendingDeletedManifest before delete = "
         + std::to_string(CScraperManifest::mapPendingDeletedManifest.size()));

    // Clear old CScraperManifests out of mapPendingDeletedManifest.
    nPendingDeleted = CScraperManifest::DeletePendingDeletedManifests();
    _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "Permanently deleted " + std::to_string(nPendingDeleted) + " manifest(s) pending permanent deletion.");
    _log(logattribute::INFO, "ScraperDeleteCScraperManifests", "Size of mapPendingDeletedManifest = "
         + std::to_string(CScraperManifest::mapPendingDeletedManifest.size()));

    // Reload mMapCSManifestsBinnedByScraper after deletions. This is not particularly efficient, but the map is not
    // that large. (The lock on CScraperManifest::cs_mapManifest is still held from above.)
    mMapCSManifestsBinnedByScraper = BinCScraperManifestsByScraper();

    _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

    return mMapCSManifestsBinnedByScraper;
}



// ---------------------------------------------- In ---------------------------------------- Out
bool LoadBeaconListFromConvergedManifest(const ConvergedManifest& StructConvergedManifest, ScraperBeaconMap& mBeaconMap)
{
    // Find the beacon list.
    auto iter = StructConvergedManifest.ConvergedManifestPartsMap.find("BeaconList");

    // Bail if the beacon list is not found, or the part is zero size (missing referenced part)
    if (iter == StructConvergedManifest.ConvergedManifestPartsMap.end() || iter->second.size() == 0) return false;

    boostio::basic_array_source<char> input_source(&iter->second[0], iter->second.size());
    boostio::stream<boostio::basic_array_source<char>> ingzss(input_source);

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzss);

    std::string line;

    int64_t ntimestamp;

    // Header -- throw away.
    std::getline(in, line);

    while (std::getline(in, line))
    {
        ScraperBeaconEntry LoadEntry;
        std::string key;

        std::vector<std::string> vline = split(line, ",");

        key = vline[0];

        std::istringstream sstimestamp(vline[1]);
        sstimestamp >> ntimestamp;
        LoadEntry.timestamp = ntimestamp;

        LoadEntry.value = vline[2];

        mBeaconMap[key] = LoadEntry;
    }

    _log(logattribute::INFO, "LoadBeaconListFromConvergedManifest", "mBeaconMap element count: " + std::to_string(mBeaconMap.size()));

    // If the Beacon Map has no entries return false.
    if (mBeaconMap.size())
    {
        return true;
    }
    else
    {
        return false;
    }
}

std::vector<uint160> GetVerifiedBeaconIDs(const ConvergedManifest& StructConvergedManifest)
{
    std::vector<uint160> result;
    ScraperPendingBeaconMap VerifiedBeaconMap;

    const auto& iter = StructConvergedManifest.ConvergedManifestPartsMap.find("VerifiedBeacons");
    if (iter != StructConvergedManifest.ConvergedManifestPartsMap.end())
    {
        CDataStream part(iter->second, SER_NETWORK, 1);

        try
        {
            part >> VerifiedBeaconMap;
        }
        catch (const std::exception& e)
        {
            _log(logattribute::WARNING, __func__, "failed to deserialize verified beacons part: " + std::string(e.what()));
        }

        for (const auto& entry : VerifiedBeaconMap)
        {
            CKeyID KeyID;

            KeyID.SetHex(entry.first);

            result.push_back(KeyID);
        }
    }

    return result;
}

std::vector<uint160> GetVerifiedBeaconIDs(const ScraperPendingBeaconMap& VerifiedBeaconMap)
{
    std::vector<uint160> result;

    for (const auto& entry : VerifiedBeaconMap)
    {
        CKeyID KeyID;

        KeyID.SetHex(entry.first);

        result.push_back(KeyID);
    }

    return result;
}


ScraperStatsAndVerifiedBeacons GetScraperStatsAndVerifiedBeacons(const ConvergedScraperStats &stats)
{
    ScraperStatsAndVerifiedBeacons stats_and_verified_beacons;

    const auto& iter = stats.Convergence.ConvergedManifestPartsMap.find("VerifiedBeacons");
    if (iter != stats.Convergence.ConvergedManifestPartsMap.end())
    {
        CDataStream part(iter->second, SER_NETWORK, 1);

        try
        {
            part >> stats_and_verified_beacons.mVerifiedMap;
        }
        catch (const std::exception& e)
        {
            _log(logattribute::WARNING, __func__, "failed to deserialize verified beacons part: " + std::string(e.what()));
        }
    }

    stats_and_verified_beacons.mScraperStats = stats.mScraperConvergedStats;

    return stats_and_verified_beacons;
}

// This is for rpc report functions.
ScraperPendingBeaconMap GetPendingBeaconsForReport()
{
    return GetConsensusBeaconList().mPendingMap;
}

ScraperPendingBeaconMap GetVerifiedBeaconsForReport(bool from_global)
{
    ScraperPendingBeaconMap VerifiedBeacons;

    if (from_global)
    {
        LOCK(cs_VerifiedBeacons);
        _log(logattribute::INFO, "LOCK", "cs_VerifiedBeacons");

        // An intentional copy.
        VerifiedBeacons = GetVerifiedBeacons().mVerifiedMap;

        _log(logattribute::INFO, "ENDLOCK", "cs_VerifiedBeacons");

    }
    else
    {
        LOCK(cs_ConvergedScraperStatsCache);
        _log(logattribute::INFO, "LOCK", "cs_ConvergedScraperStatsCache");

        // An intentional copy.
        VerifiedBeacons = GetScraperStatsAndVerifiedBeacons(ConvergedScraperStatsCache).mVerifiedMap;

        _log(logattribute::INFO, "ENDLOCK", "cs_ConvergedScraperStatsCache");
    }

    return VerifiedBeacons;
}


/***********************
*    Neural Network    *
************************/

NN::Superblock ScraperGetSuperblockContract(bool bStoreConvergedStats, bool bContractDirectFromStatsUpdate)
{
    NN::Superblock empty_superblock;

    // NOTE - OutOfSyncByAge calls PreviousBlockAge(), which takes a lock on cs_main. This is likely a deadlock culprit if called from here
    // and the scraper or neuralnet loop nearly simultaneously. So we use an atomic flag updated by the scraper or neuralnet loop.
    // If not in sync then immediately bail with an empty superblock.
    if (fOutOfSyncByAge) return empty_superblock;

    // Check the age of the ConvergedScraperStats cache. If less than nScraperSleep / 1000 old (for seconds) or clean, then simply report back the cache contents.
    // This prevents the relatively heavyweight stats computations from running too often. The time here may not exactly align with
    // the scraper loop if it is running, but that is ok. The scraper loop updates the time in the cache too.
    bool bConvergenceUpdateNeeded = true;
    {
        LOCK(cs_ConvergedScraperStatsCache);
        _log(logattribute::INFO, "LOCK", "cs_ConvergedScraperStatsCache");

        // If the cache is less than nScraperSleep in minutes old OR not dirty...
        if (GetAdjustedTime() - ConvergedScraperStatsCache.nTime < (nScraperSleep / 1000) || ConvergedScraperStatsCache.bClean)
            bConvergenceUpdateNeeded = false;

        // End LOCK(cs_ConvergedScraperStatsCache)
        _log(logattribute::INFO, "ENDLOCK", "cs_ConvergedScraperStatsCache");
    }

    ConvergedManifest StructConvergedManifest;
    ScraperBeaconMap mBeaconMap;
    NN::Superblock superblock;

    // if bConvergenceUpdate is needed, and...
    // If bContractDirectFromStatsUpdate is set to true, this means that this is being called from
    // ScraperSynchronizeDPOR() in fallback mode to force a single shot update of the stats files and
    // direct generation of the contract from the single shot run. This will return immediately with an empty SB if
    // IsScraperAuthorized() evaluates to false, because that means that by network policy, no non-scraper
    // stats downloads are allowed by unauthorized scraper nodes.
    // (If bConvergenceUpdate is not needed, then the scraper is operating by convergence already...
    if (bConvergenceUpdateNeeded)
    {
        if (!bContractDirectFromStatsUpdate)
        {
            // ScraperConstructConvergedManifest also culls old CScraperManifests. If no convergence, then
            // you can't make a SB core and you can't make a contract, so return the empty string. Also check
            // to make sure the BeaconMap has been populated properly.
            if (ScraperConstructConvergedManifest(StructConvergedManifest) && LoadBeaconListFromConvergedManifest(StructConvergedManifest, mBeaconMap))
            {
                ScraperStats mScraperConvergedStats = GetScraperStatsByConvergedManifest(StructConvergedManifest).mScraperStats;

                _log(logattribute::INFO, "ScraperGetSuperblockContract", "mScraperStats has the following number of elements: " + std::to_string(mScraperConvergedStats.size()));

                if (bStoreConvergedStats)
                {
                    if (!StoreStats(pathScraper / "ConvergedStats.csv.gz", mScraperConvergedStats))
                        _log(logattribute::ERR, "ScraperGetSuperblockContract", "StoreStats error occurred");
                    else
                        _log(logattribute::INFO, "ScraperGetSuperblockContract", "Stored converged stats.");
                }

                // I know this involves a copy operation, but it minimizes the lock time on the cache... we may want to
                // lock before and do a direct assignment, but that will lock the cache for the whole stats computation,
                // which is not really necessary.
                {
                    LOCK(cs_ConvergedScraperStatsCache);
                    _log(logattribute::INFO, "LOCK", "cs_ConvergedScraperStatsCache");

                    ConvergedScraperStatsCache.AddConvergenceToPastConvergencesMap();

                    NN::Superblock superblock_Prev = ConvergedScraperStatsCache.NewFormatSuperblock;

                    ConvergedScraperStatsCache.mScraperConvergedStats = mScraperConvergedStats;
                    ConvergedScraperStatsCache.nTime = GetAdjustedTime();
                    ConvergedScraperStatsCache.Convergence = StructConvergedManifest;

                    if (IsV11Enabled(nBestHeight + 1)) {
                        superblock = NN::Superblock::FromConvergence(ConvergedScraperStatsCache);
                    } else {
                        superblock = NN::Superblock::FromConvergence(ConvergedScraperStatsCache, 1);
                    }

                    ConvergedScraperStatsCache.NewFormatSuperblock = superblock;

                    // Mark the cache clean, because it was just updated.
                    ConvergedScraperStatsCache.bClean = true;

                    // Signal UI of SBContract status
                    if (superblock.WellFormed())
                    {
                        if (superblock_Prev.WellFormed())
                        {
                            // If the current is not empty and the previous is not empty and not the same, then there is an updated contract.
                            if (superblock.GetHash() != superblock_Prev.GetHash())
                                uiInterface.NotifyScraperEvent(scrapereventtypes::SBContract, CT_UPDATED, {});
                        }
                        else
                            // If the previous was empty and the current is not empty, then there is a new contract.
                            uiInterface.NotifyScraperEvent(scrapereventtypes::SBContract, CT_NEW, {});
                    }
                    else
                        if (superblock_Prev.WellFormed())
                            // If the current is empty and the previous was not empty, then the contract has been deleted.
                            uiInterface.NotifyScraperEvent(scrapereventtypes::SBContract, CT_DELETED, {});

                    // End LOCK(cs_ConvergedScraperStatsCache)
                    _log(logattribute::INFO, "ENDLOCK", "cs_ConvergedScraperStatsCache");
                }


                _log(logattribute::INFO, "ScraperGetSuperblockContract", "Superblock object generated from convergence");

                return superblock;
            }
            else
                return empty_superblock;
        }
        // If bContractDirectFromStatsUpdate is true, then this is the single shot pass.
        else if (IsScraperAuthorized())
        {
            // This part is the "second trip through from ScraperSynchronizeDPOR() as a fallback, if
            // authorized.

            // Do a single shot through the main scraper function to update all of the files.
            ScraperSingleShot();

            // Notice there is NO update to the ConvergedScraperStatsCache here, as that is not
            // appropriate for the single shot.
            ScraperStatsAndVerifiedBeacons stats_and_verified_beacons = GetScraperStatsByConsensusBeaconList();
            superblock = NN::Superblock::FromStats(stats_and_verified_beacons);

            // Signal the UI there is a contract.
            if(superblock.WellFormed())
                uiInterface.NotifyScraperEvent(scrapereventtypes::SBContract, CT_NEW, {});

            _log(logattribute::INFO, "ScraperGetSuperblockContract", "Superblock object generated from single shot");

            return superblock;
        }
    }
    else
    {
        // If we are here, we are using cached information.

        LOCK(cs_ConvergedScraperStatsCache);
        _log(logattribute::INFO, "LOCK", "cs_ConvergedScraperStatsCache");

        superblock = ConvergedScraperStatsCache.NewFormatSuperblock;

        // Signal the UI of the "updated" contract. This needs to be sent because the scraper loop could
        // have changed the state to something else, even though an update to the contract really hasn't happened,
        // because it is cached.
        uiInterface.NotifyScraperEvent(scrapereventtypes::SBContract, CT_UPDATED, {});

        _log(logattribute::INFO, "ScraperGetSuperblockContract", "Superblock object from cached converged stats");

        // End LOCK(cs_ConvergedScraperStatsCache)
        _log(logattribute::INFO, "ENDLOCK", "cs_ConvergedScraperStatsCache");

    }

    return superblock;
}

bool ScraperSynchronizeDPOR()
{
    // First check to see if there is already a scraper convergence and a contract formable. If so, then no update needed.
    // If the appropriate scrapers are running and the node is in communication, then this is most likely going to be
    // the path, which means very little work.
    //
    // Try again with the bool bContractDirectFromStatsUpdate set to true. This will cause a one-shot sync of the
    // Scraper file manifest to the stats sites and then a construction of the contract directly from resultant
    // mScraperStats, if the network policy allows one-off individual node stats downloads (i.e. IsScraperAuthorized()
    // is true). If IsScraperAuthorized() is false, then this will do nothing but return an empty string.
    //
    return ScraperGetSuperblockContract(false, false).WellFormed()
        || ScraperGetSuperblockContract(false, true).WellFormed();
}

//!
//! \brief The superblock validation function used for bv11+ (SB version 2+)
//!
//! This function supports four different levels of validation, and invalid
//! and unknown states.
//!
//!     Invalid,
//!     Unknown,
//!     CurrentCachedConvergence,
//!     CachedPastConvergence,
//!     ManifestLevelConvergence,
//!     ProjectLevelConvergence
//!
//! This is an enum class with six possible values.
//!
//! There are enumerated in increasing order of difficulty and decreasing
//! frequency. It is expected that the vast majority of time the superblock
//! should be validated from the current cached convergence or the cached
//! past convergence. In the case where a node has just started up when the
//! superblock comes in to be validated, there may be no cache entry that
//! corresponds to the incoming superblock contract. In that case we have to
//! fallback to trying to reconstruct the staking node's convergence at either
//! the manifest or project level from the received manifest and parts
//! information and see if the superblock formed from that matches the hash.
scraperSBvalidationtype ValidateSuperblock(const NN::Superblock& NewFormatSuperblock, bool bUseCache, unsigned int nReducedCacheBits)
{
    // Calculate the hash of the superblock to validate.
    NN::QuorumHash nNewFormatSuperblockHash = NewFormatSuperblock.GetHash();

    // convergence hash hint (reduced hash) from superblock... (used for cached lookup)
    uint32_t nReducedSBContentHash = NewFormatSuperblock.m_convergence_hint;

    // For testing purposes we allow additional bit shifting (specified via the default argument for the number of bits to retain
    // in the reduced size hint hashes). The default of nReducedCacheBits is 32, which results in 0 additional bit shift.
    // This clamps the nAdditionalBitShift to [0, 30] which corresponds to an nReducedCacheBits input range of 32 (default)
    // to 2 (minimum bits allowed).

    // This is going to affect UNCACHED validation ONLY, because that is the only place where we need to worry about hint collisions.
    unsigned int nAdditionalBitShift = std::min((unsigned int) 30, std::max((unsigned int) 0, (unsigned int) (32 - nReducedCacheBits)));

    _log(logattribute::INFO, "ValidateSuperblock", "nAdditionalBitShift = " + std::to_string(nAdditionalBitShift));

    // underlying manifest hash hint (reduced hash from superblock... (used for uncached lookup against manifests)
    uint32_t nUnderlyingManifestReducedContentHash = 0;
    if (!NewFormatSuperblock.ConvergedByProject()) nUnderlyingManifestReducedContentHash = NewFormatSuperblock.m_manifest_content_hint >> nAdditionalBitShift;

    _log(logattribute::INFO, "ValidateSuperblock", "NewFormatSuperblock.m_version = " + std::to_string(NewFormatSuperblock.m_version));

    if (bUseCache)
    {
        // Retrieve current convergence superblock. This will have the effect of updating
        // the convergence and populating the cache with the latest. If the cache has
        // already been updated via the housekeeping loop and is clean, this will be trivial.

        NN::Superblock CurrentNodeSuperblock = ScraperGetSuperblockContract(true, false);

        // If there is no superblock returned, the node is either out of sync, or has not
        // received enough manifests and parts to calculate a convergence. Return unknown.
        if (!CurrentNodeSuperblock.WellFormed()) return scraperSBvalidationtype::Unknown;

        LOCK(cs_ConvergedScraperStatsCache);

        // First check and see if superblock contract hash is the current one in the cache.
        _log(logattribute::INFO, "ValidateSuperblock", "ConvergedScraperStatsCache.NewFormatSuperblock.GetHash() = "
             + ConvergedScraperStatsCache.NewFormatSuperblock.GetHash().ToString());
        _log(logattribute::INFO, "ValidateSuperblock", "nNewFormatSuperblockHash = "
             + nNewFormatSuperblockHash.ToString());

        if (ConvergedScraperStatsCache.NewFormatSuperblock.GetHash() == nNewFormatSuperblockHash) return scraperSBvalidationtype::CurrentCachedConvergence;

        // if not validated with current cached contract, then check past ones in the cache. Note that it is
        // ok that due to a possible key collision for the uint32_t truncated hash hint, only the latest full hash that
        // matches the hint would be stored. Any others will be picked up from the uncached case below. This would
        // be extremely rare.
        auto found = ConvergedScraperStatsCache.PastConvergences.find(nReducedSBContentHash);

        if (found != ConvergedScraperStatsCache.PastConvergences.end())
        {
            if (found->second.first == nNewFormatSuperblockHash) return scraperSBvalidationtype::CachedPastConvergence;
        }
    }

    // Now for the hard stuff... the manifest level uncached case... borrowed from the ScraperConstructConvergedManifest function...

    // Call ScraperDeleteCScraperManifests(). This will return a map of manifests binned by Scraper after the culling.
    mmCSManifestsBinnedByScraper mMapCSManifestsBinnedByScraper = ScraperCullAndBinCScraperManifests();

    // Do a map for unique manifest times by content hash, then scraperID and manifest (not content) hash.
    std::multimap<uint256, std::pair<ScraperID, uint256>> mManifestsBinnedbyContent;

    unsigned int nScraperCount = mMapCSManifestsBinnedByScraper.size();

    // If the superblock indicates not converged by project, then reconstruct at the manifest level
    // else reconstruct at the project level.

    _log(logattribute::INFO, "ValidateSuperblock", "NewFormatSuperblock.ConvergedByProject() = " + std::to_string(NewFormatSuperblock.ConvergedByProject()));

    if (!NewFormatSuperblock.ConvergedByProject())
    {
        _log(logattribute::INFO, "ValidateSuperblock", "Number of Scrapers with manifests = " + std::to_string(nScraperCount));

        for (const auto& iter : mMapCSManifestsBinnedByScraper)
        {
            // iter.second is the mCSManifest
            for (const auto& iter_inner : iter.second)
            {
                // Insert into mManifestsBinnedByTime multimap. Iter_inner.first is the manifest time,
                // iter_inner.second.second is the manifest CONTENT hash.
                // mManifestsBinnedByTime.insert(std::make_pair(iter_inner.first, iter_inner.second.second));

                // Even though this is a multimap on purpose because we are going to count occurances of the same key,
                // We need to prevent the insertion of a second entry with the same content from the same scraper. This
                // could otherwise happen if a scraper is shutdown and restarted, and it publishes a new manifest
                // before it receives manifests from the other nodes (including its own prior manifests).
                // ------------------------------------------------  manifest CONTENT hash
                auto range = mManifestsBinnedbyContent.equal_range(iter_inner.second.second);
                bool bAlreadyExists = false;
                for (auto iter3 = range.first; iter3 != range.second; ++iter3)
                {
                    // ---- ScraperID ------ Candidate scraperID to insert
                    if (iter3->second.first == iter.first)
                        bAlreadyExists = true;
                }

                if (!bAlreadyExists)
                {
                    // Insert into mManifestsBinnedbyContent ------------- content hash --------------------- ScraperID ------ manifest hash.
                    mManifestsBinnedbyContent.insert(std::make_pair(iter_inner.second.second, std::make_pair(iter.first, iter_inner.second.first)));
                }
            }
        }

        // Find matching manifests (full hash) using the hint. We use a map here to prevent duplicates on purpose. Note that
        // we need this, because there is a small possibility that the hint could match MORE THAN ONE content hash, because
        // the reduced hash is only 32 bits.
        // content hash - manifest hash
        std::map<uint256, uint256> mMatchingManifestContentHashes;

        if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY))
        {
            _log(logattribute::INFO, "ValidateSuperblock", "nUnderlyingManifestReducedContentHash = " + std::to_string(nUnderlyingManifestReducedContentHash));
        }

        for (const auto& iter : mManifestsBinnedbyContent)
        {
            uint32_t nReducedManifestContentHash = iter.first.GetUint64() >> (32 + nAdditionalBitShift);

            if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY))
            {
                _log(logattribute::INFO, "ValidateSuperblock", "nReducedManifestContentHash = " + std::to_string(nReducedManifestContentHash));
            }

            // This has the effect of only storing the first one of the series of matching manifests that match the hint,
            // because of the insert. Below we will count the others matching to check for a supermajority.
            if (nReducedManifestContentHash == nUnderlyingManifestReducedContentHash) mMatchingManifestContentHashes.insert(std::make_pair(iter.first, iter.second.second));
        }

        // For each of the matching full content hashes, count the number of manifests by full content hash.
        // We continue until a group meets the supermajority rule--then there was an uncached past convergence and it is validated.
        // This scenario is typically due to a node that was started late in the scraper manifest generation cycle, so it
        // has all of the underlying manifests from the scrapers, but has not calculated and cached all of the convergences
        // that would be calculated and cached on the node from running a long time with the nScraperSleep interval NN loop.
        for (const auto& iter : mMatchingManifestContentHashes)
        {
            unsigned int nIdenticalContentManifestCount = mManifestsBinnedbyContent.count(iter.first);

            if (nIdenticalContentManifestCount >= NumScrapersForSupermajority(nScraperCount))
            {

                CScraperManifest CandidateManifest;

                {
                    LOCK(CScraperManifest::cs_mapManifest);

                    auto found = CScraperManifest::mapManifest.find(iter.second);

                    if (found != CScraperManifest::mapManifest.end())
                    {
                        // This is a copy on purpose to minimize lock time.
                        CandidateManifest = *found->second;
                    }
                    else
                    {
                        continue;
                    }
                }

                ScraperStatsAndVerifiedBeacons stats_and_verified_beacons = GetScraperStatsFromSingleManifest(CandidateManifest);

                NN::Superblock superblock = NN::Superblock::FromStats(stats_and_verified_beacons);

                _log(logattribute::INFO, "ValidateSuperblock", "superblock.m_version = " + std::to_string(superblock.m_version));

                // This should really be done in the superblock class as an overload on NN::Superblock::FromConvergence.
                superblock.m_convergence_hint = CandidateManifest.nContentHash.GetUint64() >> 32;

                NN::QuorumHash nCandidateSuperblockHash = superblock.GetHash();

                _log(logattribute::INFO, "ValidateSuperblock", "Cached past convergence - nCandidateSuperblockHash = " + nCandidateSuperblockHash.ToString());
                _log(logattribute::INFO, "ValidateSuperblock", "Cached past convergence - nNewFormatSuperblockHash = " + nNewFormatSuperblockHash.ToString());

                if (nCandidateSuperblockHash == nNewFormatSuperblockHash) return scraperSBvalidationtype::ManifestLevelConvergence;
            }
        }
    }
    else
    {
        // We are in converged by project validation mode... the harder stuff...
        // borrowed from the ScraperConstructConvergedManifestByProject function...

        // Get the whitelist.
        const NN::WhitelistSnapshot projectWhitelist = NN::GetWhitelist().Snapshot();

        struct ReferencedConvergenceInfo
        {
            uint256 nConvergedConsensusBlock;
            int64_t nConvergedConsensusTime = 0;
            uint256 nManifestHashForConvergedBeaconList;
        };

        // ------- project - proj obj content hash - ReferencedConvergenceInfo
        std::map<std::string, std::map<uint256, ReferencedConvergenceInfo>> mCandidatePartsByProject;

        // ------- project ------ placeholder value - number of reference parts
        std::map<std::string, std::pair<unsigned int, unsigned int>> mProjectCandidatePartIndexHelper;

        unsigned int iCountSuccessfulConvergedProjects = 0;

        _log(logattribute::INFO, "ValidateSuperblock", "Number of Scrapers with manifests = " + std::to_string(nScraperCount));

        // TODO: Should this be based on the list of projects in the SB? (Equivalent to a cached whitelist state.)
        for (const auto& iWhitelistProject : projectWhitelist)
        {
            // Do a map by project object (content) hash, then scraperID, project, and referenced convergence info.
            std::multimap<uint256, std::tuple<ScraperID, std::string, ReferencedConvergenceInfo>> mProjectObjectsBinnedbyContent;

            int64_t nConvergedConsensusTime = 0;
            uint256 nContentHashPrev;

            // We initialize this for each project in the whitelist.
            bool fAtLeastOnePartInsertedForProject = false;

            _log(logattribute::INFO, "ValidateSuperblock", "by project uncached: project = " + iWhitelistProject.m_name);

            {
                LOCK(CScraperManifest::cs_mapManifest);
                _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

                // For the selected project in the whitelist, walk each scraper.
                for (const auto& iter : mMapCSManifestsBinnedByScraper)
                {
                    // iter.second is the mCSManifest. Walk each manifest in each scraper.
                    for (const auto& iter_inner : iter.second)
                    {
                        // This is the referenced CScraperManifest hash
                        uint256 nCSManifestHash = iter_inner.second.first;

                        // Select manifest based on provided hash.
                        auto pair = CScraperManifest::mapManifest.find(nCSManifestHash);
                        CScraperManifest& manifest = *pair->second;

                        // Find the part number in the manifest that corresponds to the whitelisted project.
                        // Once we find a part that corresponds to the selected project in the given manifest, then break,
                        // because there can only be one part in a manifest corresponding to a given project.
                        int nPart = -1;
                        uint256 nProjectObjectHash;
                        for (const auto& vectoriter : manifest.projects)
                        {
                            if (vectoriter.project == iWhitelistProject.m_name)
                            {
                                nPart = vectoriter.part1;
                                break;
                            }
                        }

                        // Part -1 means not found, Part 0 is the beacon list, so needs to be greater than zero.
                        if (nPart > 0)
                        {
                            // Get the hash of the part referenced in the manifest.
                            nProjectObjectHash = manifest.vParts[nPart]->hash;

                            // Even though this is a multimap on purpose because we are going to count occurances of the same key,
                            // We need to prevent the insertion of a second entry with the same content from the same scraper. This is
                            // even more true here at the part level than at the manifest level, because if both SCRAPER_CMANIFEST_RETAIN_NONCURRENT
                            // and SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES are true, then there can be many references
                            // to the same part by different manifests of the same scraper in addition to across scrapers.
                            auto range = mProjectObjectsBinnedbyContent.equal_range(nProjectObjectHash);
                            bool bAlreadyExists = false;
                            for (auto iter3 = range.first; iter3 != range.second; ++iter3)
                            {
                                // ---- ScraperID ------ Candidate scraperID to insert
                                if (std::get<0>(iter3->second) == iter.first)
                                    bAlreadyExists = true;
                            }

                            if (!bAlreadyExists)
                            {
                                ReferencedConvergenceInfo ConvergenceInfoRefByPart;

                                ConvergenceInfoRefByPart.nConvergedConsensusTime = manifest.nTime;
                                ConvergenceInfoRefByPart.nConvergedConsensusBlock = manifest.ConsensusBlock;
                                ConvergenceInfoRefByPart.nManifestHashForConvergedBeaconList = *manifest.phash;

                                mProjectObjectsBinnedbyContent.insert(std::make_pair(nProjectObjectHash,
                                                                                     std::make_tuple(iter.first,
                                                                                                     iWhitelistProject.m_name,
                                                                                                     ConvergenceInfoRefByPart)));
                            }
                        }
                    }
                }
                _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");
            } // Critical section scope for cs_mapManifest.

            _log(logattribute::INFO, "ValidateSuperblock", "by project uncached: mProjectObjectsBinnedbyContent size = "
                              + std::to_string(mProjectObjectsBinnedbyContent.size()));

            for (const auto& iter : mProjectObjectsBinnedbyContent)
            {
                uint32_t nReducedProjectObjectContentHash_prebitshift = iter.first.GetUint64() >> 32;
                uint32_t nReducedProjectObjectContentHash = iter.first.GetUint64() >> (32 + nAdditionalBitShift);

                unsigned int nIdenticalContentManifestCount = mProjectObjectsBinnedbyContent.count(iter.first);

                // Only process if there is a valid entry in the superblock.
                if (const auto SBProjectIter = NewFormatSuperblock.m_projects.Try(iWhitelistProject.m_name))
                {
                    if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY))
                    {
                        _log(logattribute::INFO,
                             "ValidateSuperblock",
                             "by project uncached:\n"
                             "nReducedProjectObjectContentHash_prebitshift = " + std::to_string(nReducedProjectObjectContentHash_prebitshift) + "\n"
                             + "           SBProjectIter->m_convergence_hint = " + std::to_string(SBProjectIter->m_convergence_hint) + "\n"
                             + "            nReducedProjectObjectContentHash = " + std::to_string(nReducedProjectObjectContentHash) + "\n"
                             + "     SBProjectIter->m_convergence_hint >> " + std::to_string(nAdditionalBitShift)
                             + " = " + std::to_string(SBProjectIter->m_convergence_hint >> nAdditionalBitShift) + "\n"
                             );
                    }

                    // Only process if the (reduced content hash) for the project object matches the hint.
                    if (nReducedProjectObjectContentHash == SBProjectIter->m_convergence_hint >> nAdditionalBitShift)
                    {
                        // Only process if there are actually parts.
                        if (nIdenticalContentManifestCount >= NumScrapersForSupermajority(nScraperCount)
                                && !mProjectObjectsBinnedbyContent.empty())
                        {
                            LOCK(CSplitBlob::cs_mapParts);

                            // Get the actual part ------------- by object hash.
                            auto iPart = CSplitBlob::mapParts.find(iter.first);

                            if (iPart != CSplitBlob::mapParts.end())
                            {
                                uint256 nContentHashCheck = Hash(iPart->second.data.begin(), iPart->second.data.end());

                                if (nContentHashCheck != iPart->first)
                                {
                                    _log(logattribute::ERR, "ScraperConstructConvergedManifestByProject", "Selected Converged Project Object content hash check failed! nContentHashCheck = "
                                         + nContentHashCheck.GetHex() + " and nContentHash = " + iPart->first.GetHex());
                                    continue;
                                }

                                ReferencedConvergenceInfo ConvergenceInfoRefByPart;

                                // On the first element of a repeated range, we update the ConvergenceInfoRefByPart, then 2nd and succeeding elements
                                // in the range, we only update if the nConvergedConsensusTime is greater. This will ensure the stored ConvergenceInfoRefByPart for
                                // the candidate part hash for the project is the latest one.

                                // We are on the first one of a range of the same part.
                                if (iter.first != nContentHashPrev)
                                {
                                    ConvergenceInfoRefByPart = std::get<2>(iter.second);
                                }
                                // We are on the second or succeeding parts of a range of the same part and the nConvergedConsensusTime is greater.
                                else if (iter.first == nContentHashPrev && ConvergenceInfoRefByPart.nConvergedConsensusTime > nConvergedConsensusTime)
                                {
                                    ConvergenceInfoRefByPart = std::get<2>(iter.second);
                                }

                                // This form of insert will override the previous insert with the same part hash, which is what we want.
                                // ------------------------- project name -------- part hash
                                mCandidatePartsByProject[iWhitelistProject.m_name][iter.first] = ConvergenceInfoRefByPart;

                                fAtLeastOnePartInsertedForProject = true;

                                // we are going to consider every part, so there is no break.
                            } // Test for successful find of part.
                        } // Test for supermajority (per project) and existence of part(s).
                    } // Test whether object's reduced content hash matches hint.
                } // Test whether candidate SB has a project that matches selected project in the whitelist.
            } // Iterate over mProjectObjectsBinnedbyContent.


            if (fAtLeastOnePartInsertedForProject)
            {
                // Initialize candidate part index helper
                // Note that "cumulative placevalue" is set to zero here and will be updated later.
                mProjectCandidatePartIndexHelper[iWhitelistProject.m_name] =
                        std::make_pair(0, (mCandidatePartsByProject.find(iWhitelistProject.m_name)->second).size());

                // Increment successful part counter.
                ++iCountSuccessfulConvergedProjects;
            }
        } // For loop over projectWhitelist.

        _log(logattribute::INFO, "ValidateSuperblock", "by project uncached: iCountSuccessfulConvergedProjects = "
                          + std::to_string(iCountSuccessfulConvergedProjects)
                          + ", NewFormatSuperblock.m_projects size = "
                          + std::to_string(NewFormatSuperblock.m_projects.size()));

        // If we have a matched project count match, then proceed with the construction of local candidate SB's
        // to validate, otherwise there is no validation.
        if (iCountSuccessfulConvergedProjects != NewFormatSuperblock.m_projects.size()) return scraperSBvalidationtype::Invalid;

        /*

        Compute number of permutations and cumulative placevalue. For instance, if there were 4 projects, and
        the number of candidate parts for each of the projects were 2, 3, 2, and 2, then the total number of
        permutations would be 2 * 3 * 2 * 2 = 24, and the cumulative placevalue would be
        -------------------- 12,  4,  2,  1. Notice we will use a reverse iterator to fill in the placevalue
        as we compute the total number of permutations. This is ridiculous of course because 99.99999% of the
        time it is going to be 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 (one permutation) or at most something like
        1 1 1 1 1 1 1 1 2 1 1 1 1 1 1 1 1 1 1, which will resolve to two permutations, but I am doing the full algorithm
        to be thorough.

        A further explanation...

        The easiest way to understand this algorithm is to compare it to a binary number....
        Each place has two possibilities, so the total possibilities are 2^n where you have n places.
        I.e if you had 4 places (think four projects) each with 2 candidate parts, you would have 0000 to 1111.
        This is permutation index 0 to 7 for a total of eight permutations. If the outer loop is the
        permutation index, you derive the place values by multiplying the combinations for the places
        up to but not including the place for the four digit binary number ... the place values are 8, 4, 2, 1.

        So, if you are at index 5 this is, starting at the most significant digit,
        5/8 = 0 remainder 5.
        5/4 = 1 remainder 1.
        1/2 = 0 remainder 1.
        1/1 = 1 remainder 0.
        (This of course is 0101 binary for 5.)

        If the number of candidate parts varies for each project (which it will in this problem), the place
        is a different base for each place, and then you track the number of possibilities in each place with
        the helper as I described in the comments. This allows the outer loop to simply run consecutively through
        the permutation index from 0 to n-1 permutations, and then for each permutation index value the repeated
        division in the inner loop  “decodes” the index into the selected part for each placeholder (project).
        Since you calculated the permutation index max n by muliplyjng all of the possibilities together for each
        place you know you have covered them all. I think it is a pretty elegant approach to the problem with iteration.

        */
        unsigned int nPermutations = 1;
        {
            std::map<std::string, std::pair<unsigned int, unsigned int>>::reverse_iterator iProject;
            for (iProject = mProjectCandidatePartIndexHelper.rbegin(); iProject != mProjectCandidatePartIndexHelper.rend(); ++iProject)
            {
                // pull the pair for the project which is currently (0, # of candidate parts) up to now.
                auto pair = iProject->second;

                // update ----------------------- project entry with -------- placeholder value - # of candidate parts
                mProjectCandidatePartIndexHelper[iProject->first] = std::make_pair(nPermutations, pair.second);

                nPermutations = nPermutations * iProject->second.second;
            }
        }

        _log(logattribute::INFO, "ValidateSuperblock", "uncached by project: nPermutations = " + std::to_string(nPermutations));

        // Iterate through the permutations. Most of the time there will only be one! :)
        for (unsigned int iPermutationIndex = 0; iPermutationIndex < nPermutations; ++iPermutationIndex)
        {
            // For each permutation we build a dummy converged manifest to use for validation.
            ConvergedManifest StructDummyConvergedManifest;

            // Locally scoped version for permutation loop.
            ReferencedConvergenceInfo ConvergenceInfo;

            CDataStream ss(SER_NETWORK,1);

            // We are going to do division to figure out the "place" values for each project, which correspond to the selected part for each project.
            // The "initial" remainder is the original value of the permutation index itself.
            unsigned int remainder = iPermutationIndex;

            for (const auto& iProject : mProjectCandidatePartIndexHelper)
            {
                unsigned int divisor = iProject.second.first;

                // Calculate quotient
                unsigned int iPartIndex = remainder / divisor;

                _log(logattribute::INFO, "ValidateSuperblock", "uncached by project: "
                                  + std::to_string(remainder) + "/" + std::to_string(divisor) + +" = iPartIndex = " + std::to_string(iPartIndex)
                                  + " index max = " + std::to_string(((mCandidatePartsByProject.find(iProject.first))->second).size() - 1));

                // Calculate remainder for next "place" (iteration)
                remainder -= iPartIndex * divisor;

                // Get an iterator to the first candidate part for the project.
                auto iCandidatePart = (mCandidatePartsByProject.find(iProject.first))->second.begin();
                // Advance to the part indicated by the index
                std::advance(iCandidatePart, iPartIndex);

                // Pull the actual part and do a hash check, then if it passes insert into trial StructDummyConvergedManifest.
                {
                    LOCK(CSplitBlob::cs_mapParts);

                    // Get the actual part ----------------- by object hash.
                    auto iPart = CSplitBlob::mapParts.find(iCandidatePart->first);

                    if (iPart == CSplitBlob::mapParts.end())
                    {
                        _log(logattribute::ERR, "ValidateSuperblock", "Selected Converged Project Object (part) not found in parts map.");
                        continue;
                    }

                    uint256 nContentHashCheck = Hash(iPart->second.data.begin(), iPart->second.data.end());

                    // If the selected part doesn't pass validation, then continue, under the small chance
                    // that another permutation could pass. This is unlikely though, because almost all of the time
                    // there will only be one candidate.
                    if (nContentHashCheck != iPart->first)
                    {
                        _log(logattribute::ERR, "ValidateSuperblock", "Selected Converged Project Object content hash check failed! nContentHashCheck = "
                             + nContentHashCheck.GetHex() + " and nContentHash = " + iPart->first.GetHex());
                        continue;
                    }

                    // Put Project Object (Part) in StructConvergedManifest keyed by project.
                    StructDummyConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair(iProject.first, iPart->second.data));

                    // Update consensus data.
                    ReferencedConvergenceInfo NewConvergenceInfo = iCandidatePart->second;

                    if (NewConvergenceInfo.nConvergedConsensusTime > ConvergenceInfo.nConvergedConsensusTime) ConvergenceInfo = NewConvergenceInfo;
                }
            }

            LOCK(CScraperManifest::cs_mapManifest);
            _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

            // Lets use the BeaconList from the manifest referred to by nManifestHashForConvergedBeaconList. Technically there is no exact answer to
            // the BeaconList that should be used in the convergence when putting it together at the individual part level, because each project part
            // could have used a different BeaconList (subject to the consensus ladder. It makes sense to use the "newest" one that is associated
            // with a manifest that has the newest part associated with a successful part (project) level convergence, and here we do this for each
            // permutation.

            // Select manifest based on convergence info hash based on part with last part time.
            auto pair = CScraperManifest::mapManifest.find(ConvergenceInfo.nManifestHashForConvergedBeaconList);
            CScraperManifest& manifest = *pair->second;

            // The vParts[0] is always the BeaconList.
            StructDummyConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair("BeaconList", manifest.vParts[0]->data));

            // Find the offset of the verified beacons project part. Typically
            // this exists at vParts offset 1 when a scraper verified at least
            // one pending beacon. If it doesn't exist, omit the part from the
            // reconstructed convergence:
            const auto verified_beacons_entry_iter = std::find_if(
                manifest.projects.begin(),
                manifest.projects.end(),
                [](const CScraperManifest::dentry& entry) {
                    return entry.project == "VerifiedBeacons";
                });

            if (verified_beacons_entry_iter != manifest.projects.end())
            {
                const size_t part_offset = verified_beacons_entry_iter->part1;

                if (part_offset == 0 || part_offset >= manifest.vParts.size())
                {
                    _log(logattribute::ERR, "ValidateSuperblock", "out-of-range verified beacon part.");
                }
                else
                {
                    StructDummyConvergedManifest.ConvergedManifestPartsMap.insert(std::make_pair("VerifiedBeacons", manifest.vParts[part_offset]->data));
                }
            }
            else
            {
                _log(logattribute::ERR, "ValidateSuperblock", "verified beacon project missing.");
            }

            StructDummyConvergedManifest.ConsensusBlock = ConvergenceInfo.nConvergedConsensusBlock;

            // The ConvergedManifest content hash is in the order of the map key and on the data.
            for (const auto& iter : StructDummyConvergedManifest.ConvergedManifestPartsMap)
                ss << iter.second;

            StructDummyConvergedManifest.nContentHash = Hash(ss.begin(), ss.end());
            StructDummyConvergedManifest.timestamp = ConvergenceInfo.nConvergedConsensusTime;
            StructDummyConvergedManifest.bByParts = true;

            _log(logattribute::INFO, "ValidateSuperblock", "Successful convergence by project: "
                 + std::to_string(iCountSuccessfulConvergedProjects) + " out of " + std::to_string(projectWhitelist.size())
                 + " projects at "
                 + DateTimeStrFormat("%x %H:%M:%S",  StructDummyConvergedManifest.timestamp));

            ScraperStatsAndVerifiedBeacons stats_and_verified_beacons = GetScraperStatsByConvergedManifest(StructDummyConvergedManifest);

            NN::Superblock superblock = NN::Superblock::FromStats(stats_and_verified_beacons);

            // This should really be done in the superblock class as an overload on NN::Superblock::FromConvergence.
            superblock.m_convergence_hint = StructDummyConvergedManifest.nContentHash.GetUint64() >> 32;

            if (StructDummyConvergedManifest.bByParts)
            {
                NN::Superblock::ProjectIndex& projects = superblock.m_projects;

                // Add hints created from the hashes of converged manifest parts
                for (const auto& part_pair : StructDummyConvergedManifest.ConvergedManifestPartsMap)
                {
                    const std::string& project_name = part_pair.first;
                    const CSerializeData& part_data = part_pair.second;

                    projects.SetHint(project_name, part_data);
                }
            }

            NN::QuorumHash nCandidateSuperblockHash = superblock.GetHash();

            _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

            if (nCandidateSuperblockHash == nNewFormatSuperblockHash) return scraperSBvalidationtype::ProjectLevelConvergence;
        } // Permutation loop
    } // Uncached project level validation

    // If we make it here, there is no validation.
    return scraperSBvalidationtype::Invalid;
}

/***********************
*    RPC Functions     *
************************/

UniValue sendscraperfilemanifest(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0 )
        throw std::runtime_error(
                "sendscraperfilemanifest\n"
                "Send a CScraperManifest object with the ScraperFileManifest.\n"
                );

    CBitcoinAddress AddressOut;
    CKey KeyOut;
    bool ret;
    if (IsScraperAuthorizedToBroadcastManifests(AddressOut, KeyOut))
    {
        ret = ScraperSendFileManifestContents(AddressOut, KeyOut);
        uiInterface.NotifyScraperEvent(scrapereventtypes::Manifest, CT_NEW, {});
    }
    else
        ret = false;

    return UniValue(ret);
}



UniValue savescraperfilemanifest(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1 )
        throw std::runtime_error(
                "savescraperfilemanifest <hash>\n"
                "Send a CScraperManifest object with the ScraperFileManifest.\n"
                );

    bool ret = ScraperSaveCScraperManifestToFiles(uint256S(params[0].get_str()));

    return UniValue(ret);
}


UniValue deletecscrapermanifest(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1 )
        throw std::runtime_error(
                "deletecscrapermanifest <hash>\n"
                "delete manifest object.\n"
                );

    LOCK(CScraperManifest::cs_mapManifest);
    _log(logattribute::INFO, "LOCK", "CScraperManifest::cs_mapManifest");

    bool ret = CScraperManifest::DeleteManifest(uint256S(params[0].get_str()), true);

    _log(logattribute::INFO, "ENDLOCK", "CScraperManifest::cs_mapManifest");

    return UniValue(ret);
}


UniValue archivelog(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1 )
        throw std::runtime_error(
                "archivelog <log>\n"
                "Immediately archives the specified log. Currently valid valus are debug and scraper.\n"
                );

    std::string sLogger = params[0].get_str();

    fs::path pfile_out;
    bool ret;

    if (sLogger == "debug")
    {
        ret = LogInstance().archive(true, pfile_out);
    }
    else if (sLogger == "scraper")
    {
        ret = ScraperLogInstance().archive(true, pfile_out);
    }
    else ret = false;

    return UniValue(ret);
}

UniValue convergencereport(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0 )
        throw std::runtime_error(
                "convergencereport\n"
                "Display local node report of scraper convergence.\n"
                );

    // See if converged stats/contract update needed...
    bool bConvergenceUpdateNeeded = true;
    {
        LOCK(cs_ConvergedScraperStatsCache);

        if (GetAdjustedTime() - ConvergedScraperStatsCache.nTime < (nScraperSleep / 1000) || ConvergedScraperStatsCache.bClean)
        {
            bConvergenceUpdateNeeded = false;
        }
    }

    if (bConvergenceUpdateNeeded)
    {
        // Don't need the output but will use the global cache, which will be updated.
        ScraperGetSuperblockContract(false, false);
    }

    UniValue result(UniValue::VOBJ);

    {
        LOCK(cs_ConvergedScraperStatsCache);

        if (!ConvergedScraperStatsCache.NewFormatSuperblock.WellFormed())
        {
            throw JSONRPCError(RPC_MISC_ERROR, "Error: A convergence cannot be formed at this time. Please review the scraper.log for details.");
        }

        int64_t nConvergenceTime = ConvergedScraperStatsCache.nTime;

        result.pushKV("convergence_timestamp", nConvergenceTime);
        result.pushKV("convergence_datetime", DateTimeStrFormat("%x %H:%M:%S UTC",  nConvergenceTime));

        UniValue ExcludedProjects(UniValue::VARR);

        for (const auto& entry : ConvergedScraperStatsCache.Convergence.vExcludedProjects)
        {
            ExcludedProjects.push_back(entry);
        }

        result.pushKV("excluded_projects", ExcludedProjects);


        UniValue IncludedScrapers(UniValue::VARR);

        for (const auto& entry : ConvergedScraperStatsCache.Convergence.vIncludedScrapers)
        {
            IncludedScrapers.push_back(entry);
        }

        result.pushKV("included_scrapers", IncludedScrapers);


        UniValue ExcludedScrapers(UniValue::VARR);

        for (const auto& entry : ConvergedScraperStatsCache.Convergence.vExcludedScrapers)
        {
            ExcludedScrapers.push_back(entry);
        }

        result.pushKV("excluded_scrapers", ExcludedScrapers);


        UniValue ScrapersNotPublishing(UniValue::VARR);

        for (const auto& entry : ConvergedScraperStatsCache.Convergence.vScrapersNotPublishing)
        {
            ScrapersNotPublishing.push_back(entry);
        }

        result.pushKV("scrapers_not_publishing", ScrapersNotPublishing);
    }

    return result;
}

UniValue testnewsb(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1 )
        throw std::runtime_error(
                "testnewsb [hint bits]\n"
                "Test the new Superblock class. Optional parameter of the number of bits for the reduced hash hint for uncached test.\n"
                "This is limited to a range of 4 to 32, with 32 as the default (which is the normal hint bits).\n"
                );

    unsigned int nReducedCacheBits = 32;

    if (params.size() == 1) nReducedCacheBits = params[0].get_int();

    {
        LOCK(cs_ConvergedScraperStatsCache);

		if (!ConvergedScraperStatsCache.NewFormatSuperblock.WellFormed())
		{
		    UniValue error(UniValue::VOBJ);
		    error.pushKV("Error:", "Wait until a convergence is formed.");

		    return error;
		}
    }

    UniValue res(UniValue::VOBJ);

    _log(logattribute::INFO, "testnewsb", "Size of the PastConvergences map = " + std::to_string(ConvergedScraperStatsCache.PastConvergences.size()));
    res.pushKV("Size of the PastConvergences map", std::to_string(ConvergedScraperStatsCache.PastConvergences.size()));

    // Contract binary pack/unpack check...
    _log(logattribute::INFO, "testnewsb", "Checking compatibility with binary SB pack/unpack by packing then unpacking, then comparing to the original");

    std::string legacy_packed_contract;
    {
        LOCK(cs_ConvergedScraperStatsCache);

        legacy_packed_contract = ConvergedScraperStatsCache.NewFormatSuperblock.PackLegacy();
        uint64_t legacy_packed_size = legacy_packed_contract.size();

        _log(logattribute::INFO, "testnewsb", "legacy packed size = " + std::to_string(legacy_packed_size));
        res.pushKV("legacy packed size", legacy_packed_size);
    }

    NN::SuperblockPtr NewFormatSuperblock = NN::SuperblockPtr::Empty();
    NN::Superblock NewFormatSuperblock_out;
    CDataStream ss(SER_NETWORK, 1);
    uint64_t nNewFormatSuperblockSerSize;
    uint64_t nNewFormatSuperblock_outSerSize;
    NN::QuorumHash nNewFormatSuperblockHash;
    NN::QuorumHash nNewFormatSuperblock_outHash;
    uint32_t nNewFormatSuperblockReducedContentHashFromConvergenceHint;
    uint32_t nNewFormatSuperblockReducedContentHashFromUnderlyingManifestHint;

    {
        LOCK(cs_ConvergedScraperStatsCache);

        NewFormatSuperblock = NN::SuperblockPtr::BindShared(
            NN::Superblock::FromConvergence(ConvergedScraperStatsCache),
            pindexBest);

        _log(logattribute::INFO, "testnewsb", "ConvergedScraperStatsCache.Convergence.bByParts = " + std::to_string(ConvergedScraperStatsCache.Convergence.bByParts));
    }

    _log(logattribute::INFO, "testnewsb", "m_projects size = " + std::to_string(NewFormatSuperblock->m_projects.size()));
    res.pushKV("m_projects size", (uint64_t) NewFormatSuperblock->m_projects.size());
    _log(logattribute::INFO, "testnewsb", "m_cpids size = " + std::to_string(NewFormatSuperblock->m_cpids.size()));
    res.pushKV("m_cpids size", (uint64_t) NewFormatSuperblock->m_cpids.size());
    _log(logattribute::INFO, "testnewsb", "zero-mag count = " + std::to_string(NewFormatSuperblock->m_cpids.Zeros()));
    res.pushKV("zero-mag count", (uint64_t) NewFormatSuperblock->m_cpids.Zeros());

    nNewFormatSuperblockSerSize = GetSerializeSize(*NewFormatSuperblock, SER_NETWORK, 1);
    nNewFormatSuperblockHash = NewFormatSuperblock->GetHash();

    _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock.m_version = " + std::to_string(NewFormatSuperblock->m_version));
    res.pushKV("NewFormatSuperblock.m_version", (uint64_t) NewFormatSuperblock->m_version);

    nNewFormatSuperblockReducedContentHashFromConvergenceHint = NewFormatSuperblock->m_convergence_hint;
    nNewFormatSuperblockReducedContentHashFromUnderlyingManifestHint = NewFormatSuperblock->m_manifest_content_hint;

    _log(logattribute::INFO, "testnewsb", "nNewFormatSuperblockSerSize = " + std::to_string(nNewFormatSuperblockSerSize));
    res.pushKV("nNewFormatSuperblockSerSize", nNewFormatSuperblockSerSize);

    ss << *NewFormatSuperblock;
    ss >> NewFormatSuperblock_out;

    nNewFormatSuperblock_outSerSize = GetSerializeSize(NewFormatSuperblock_out, SER_NETWORK, 1);
    nNewFormatSuperblock_outHash = NewFormatSuperblock_out.GetHash();

    _log(logattribute::INFO, "testnewsb", "nNewFormatSuperblock_outSerSize = " + std::to_string(nNewFormatSuperblock_outSerSize));
    res.pushKV("nNewFormatSuperblock_outSerSize", nNewFormatSuperblock_outSerSize);

    if (NewFormatSuperblock->GetHash() == nNewFormatSuperblock_outHash)
    {
        _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock serialization passed.");
        res.pushKV("NewFormatSuperblock serialization", "passed");
    }
    else
    {
        _log(logattribute::ERR, "testnewsb", "NewFormatSuperblock serialization FAILED.");
        res.pushKV("NewFormatSuperblock serialization", "FAILED");
    }

    NN::Superblock NewFormatSuperblockFromLegacy = NN::Superblock::UnpackLegacy(legacy_packed_contract);
    NN::QuorumHash new_legacy_hash = NN::QuorumHash::Hash(NewFormatSuperblockFromLegacy);

    res.pushKV("NewFormatSuperblockHash", nNewFormatSuperblockHash.ToString());
    _log(logattribute::INFO, "testnewsb", "NewFormatSuperblockHash = " + nNewFormatSuperblockHash.ToString());
    res.pushKV("new_legacy_hash", new_legacy_hash.ToString());
    _log(logattribute::INFO, "testnewsb", "new_legacy_hash = " + new_legacy_hash.ToString());
    res.pushKV("nNewFormatSuperblockReducedContentHashFromConvergenceHint", (uint64_t) nNewFormatSuperblockReducedContentHashFromConvergenceHint);
    _log(logattribute::INFO, "testnewsb", "nNewFormatSuperblockReducedContentHashFromConvergenceHint = " + std::to_string(nNewFormatSuperblockReducedContentHashFromConvergenceHint));
    res.pushKV("nNewFormatSuperblockReducedContentHashFromUnderlyingManifestHint", (uint64_t) nNewFormatSuperblockReducedContentHashFromUnderlyingManifestHint);
    _log(logattribute::INFO, "testnewsb", "nNewFormatSuperblockReducedContentHashFromUnderlyingManifestHint = " + std::to_string(nNewFormatSuperblockReducedContentHashFromUnderlyingManifestHint));

    _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock legacy unpack number of zero mags = " + std::to_string(NewFormatSuperblock->m_cpids.Zeros()));
    res.pushKV("NewFormatSuperblock legacy unpack number of zero mags", std::to_string(NewFormatSuperblock->m_cpids.Zeros()));

    // Log the number of bits used to force key collisions.
    _log(logattribute::INFO, "testnewsb", "nReducedCacheBits = " + std::to_string(nReducedCacheBits));
    res.pushKV("nReducedCacheBits", std::to_string(nReducedCacheBits));

    //
    // ValidateSuperblock() reference function tests (current convergence)
    //

    // nReducedCacheBits is only used for non-cached tests.
    scraperSBvalidationtype validity = ::ValidateSuperblock(*NewFormatSuperblock, true);

    if (validity != scraperSBvalidationtype::Invalid && validity != scraperSBvalidationtype::Unknown)
    {
        _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock validation against current (using cache) passed - " + GetTextForscraperSBvalidationtype(validity));
        res.pushKV("NewFormatSuperblock validation against current (using cache)", "passed - " + GetTextForscraperSBvalidationtype(validity));
    }
    else
    {
        _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock validation against current (using cache) failed - " + GetTextForscraperSBvalidationtype(validity));
        res.pushKV("NewFormatSuperblock validation against current (using cache)", "failed - " + GetTextForscraperSBvalidationtype(validity));
    }

    // nReducedCacheBits is only used here for non-cached tests.
    scraperSBvalidationtype validity2 = ::ValidateSuperblock(*NewFormatSuperblock, false, nReducedCacheBits);

    if (validity2 != scraperSBvalidationtype::Invalid && validity2 != scraperSBvalidationtype::Unknown)
    {
        _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock validation against current (without using cache) passed - " + GetTextForscraperSBvalidationtype(validity2));
        res.pushKV("NewFormatSuperblock validation against current (without using cache)", "passed - " + GetTextForscraperSBvalidationtype(validity2));
    }
    else
    {
        _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock validation against current (without using cache) failed - " + GetTextForscraperSBvalidationtype(validity2));
        res.pushKV("NewFormatSuperblock validation against current (without using cache)", "failed - " + GetTextForscraperSBvalidationtype(validity2));
    }

    //
    // SuperblockValidator class tests (current convergence)
    //

    if (NN::Quorum::ValidateSuperblock(NewFormatSuperblock))
    {
        _log(logattribute::INFO, "testnewsb", "NN::ValidateSuperblock validation against current (using cache) passed");
        res.pushKV("NN::ValidateSuperblock validation against current (using cache)", "passed");
    }
    else
    {
        _log(logattribute::INFO, "testnewsb", "NN::ValidateSuperblock validation against current (using cache) failed");
        res.pushKV("NN::ValidateSuperblock validation against current (using cache)", "failed");
    }

    if (NN::Quorum::ValidateSuperblock(NewFormatSuperblock, false, nReducedCacheBits))
    {
        _log(logattribute::INFO, "testnewsb", "NN::ValidateSuperblock validation against current (without using cache) passed");
        res.pushKV("NN::ValidateSuperblock validation against current (without using cache)", "passed");
    }
    else
    {
        _log(logattribute::INFO, "testnewsb", "NN::ValidateSuperblock validation against current (without using cache) failed");
        res.pushKV("NN::ValidateSuperblock validation against current (without using cache)", "failed");
    }

    ConvergedManifest RandomPastConvergedManifest;
    bool bPastConvergencesEmpty = true;

    {
        LOCK(cs_ConvergedScraperStatsCache);

        auto iPastSB = ConvergedScraperStatsCache.PastConvergences.begin();

        unsigned int PastConvergencesSize = ConvergedScraperStatsCache.PastConvergences.size();

        if (PastConvergencesSize > 1)
        {
            int i = GetRandInt(PastConvergencesSize - 1);

            _log(logattribute::INFO, "testnewsb", "NN::ValidateSuperblock random past RandomPastConvergedManifest index " + std::to_string(i) + " selected.");
            res.pushKV("NN::ValidateSuperblock random past RandomPastConvergedManifest index selected", i);

            std::advance(iPastSB, i);

            RandomPastConvergedManifest = iPastSB->second.second;

            bPastConvergencesEmpty = false;
        }
        else if (PastConvergencesSize == 1)
        {
            //Use the first and only element.
            RandomPastConvergedManifest = iPastSB->second.second;

            bPastConvergencesEmpty = false;
        }

    }

    if (!bPastConvergencesEmpty)
    {
        ScraperStatsAndVerifiedBeacons RandomPastSBStatsAndVerifiedBeacons = GetScraperStatsByConvergedManifest(RandomPastConvergedManifest);

        NN::Superblock RandomPastSB = NN::Superblock::FromStats(RandomPastSBStatsAndVerifiedBeacons);

        // This should really be done in the superblock class as an overload on NN::Superblock::FromConvergence.
        RandomPastSB.m_convergence_hint = RandomPastConvergedManifest.nContentHash.GetUint64() >> 32;

        if (RandomPastConvergedManifest.bByParts)
        {
            NN::Superblock::ProjectIndex& projects = RandomPastSB.m_projects;

            // Add hints created from the hashes of converged manifest parts to each
            // superblock project section to assist receiving nodes with validation:
            //
            for (const auto& part_pair : RandomPastConvergedManifest.ConvergedManifestPartsMap)
            {
                const std::string& project_name = part_pair.first;
                const CSerializeData& part_data = part_pair.second;

                projects.SetHint(project_name, part_data); // This also sets m_converged_by_project to true.
            }
        }
        else
        {
            RandomPastSB.m_manifest_content_hint = RandomPastConvergedManifest.nUnderlyingManifestContentHash.GetUint64() >> 32;
        }

        //
        // ValidateSuperblock() reference function tests (past convergence)
        //

        scraperSBvalidationtype validity3 = ::ValidateSuperblock(RandomPastSB, true);

        if (validity3 != scraperSBvalidationtype::Invalid && validity != scraperSBvalidationtype::Unknown)
        {
            _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock validation against random past (using cache) passed - " + GetTextForscraperSBvalidationtype(validity3));
            res.pushKV("NewFormatSuperblock validation against random past (using cache)", "passed - " + GetTextForscraperSBvalidationtype(validity3));
        }
        else
        {
            _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock validation against random past (using cache) failed - " + GetTextForscraperSBvalidationtype(validity3));
            res.pushKV("NewFormatSuperblock validation against random past (using cache)", "failed - " + GetTextForscraperSBvalidationtype(validity3));
        }

        scraperSBvalidationtype validity4 = ::ValidateSuperblock(RandomPastSB, false, nReducedCacheBits);

        if (validity4 != scraperSBvalidationtype::Invalid && validity != scraperSBvalidationtype::Unknown)
        {
            _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock validation against random past (without using cache) passed - " + GetTextForscraperSBvalidationtype(validity4));
            res.pushKV("NewFormatSuperblock validation against random past (without using cache)", "passed - " + GetTextForscraperSBvalidationtype(validity4));
        }
        else
        {
            _log(logattribute::INFO, "testnewsb", "NewFormatSuperblock validation against random past (without using cache) failed - " + GetTextForscraperSBvalidationtype(validity4));
            res.pushKV("NewFormatSuperblock validation against random past (without using cache)", "failed - " + GetTextForscraperSBvalidationtype(validity4));
        }

        //
        // SuperblockValidator class tests (past convergence)
        //

        NN::SuperblockPtr RandomPastSBPtr = NN::SuperblockPtr::BindShared(
            std::move(RandomPastSB),
            pindexBest);

        if (NN::Quorum::ValidateSuperblock(RandomPastSBPtr))
        {
            _log(logattribute::INFO, "testnewsb", "NN::ValidateSuperblock validation against random past (using cache) passed");
            res.pushKV("NN::ValidateSuperblock validation against random past (using cache)", "passed");
        }
        else
        {
            _log(logattribute::INFO, "testnewsb", "NN::ValidateSuperblock validation against random past (using cache) failed");
            res.pushKV("NN::ValidateSuperblock validation against random past (using cache)", "failed");
        }

        if (NN::Quorum::ValidateSuperblock(RandomPastSBPtr, false, nReducedCacheBits))
        {
            _log(logattribute::INFO, "testnewsb", "NN::ValidateSuperblock validation against random past (without using cache) passed");
            res.pushKV("NN::ValidateSuperblock validation against random past (without using cache)", "passed");
        }
        else
        {
            _log(logattribute::INFO, "testnewsb", "NN::ValidateSuperblock validation against random past (without using cache) failed");
            res.pushKV("NN::ValidateSuperblock validation against random past (without using cache)", "failed");
        }
    }

    return res;
}
