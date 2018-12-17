#ifndef SCRAPER_H
#define SCRAPER_H

#include <string>
#include <vector>
#include <cmath>
#include <zlib.h>
#include <iostream>
#include <curl/curl.h>
#include <inttypes.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <map>
#include <unordered_map>
#include <boost/locale.hpp>
#include <codecvt>
#include <boost/exception/exception.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/filesystem.hpp>
#include <boost/serialization/binary_object.hpp>
//#include <expat.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <fstream>
#include <sstream>

#include "sync.h"
#include "appcache.h"
#include "beacon.h"
#include "wallet.h"

#include <memory>
#include "net.h"
// Do not include rpcserver.h yet, as it includes conflicting ExtractXML functions. To do: remove duplicates.
//#include "rpcserver.h"
#include "rpcprotocol.h"
#include "scraper_net.h"

#ifndef SCRAPER_STANDALONE 
#include "util.h"
#else
#include <json/json.h>
//#include <jsoncpp/json/json.h>
#include "uint256.h"
extern int64_t GetAdjustedTime();
extern bool Contains(const std::string& data, const std::string& instring);
#endif

/*********************
* Scraper Namepsace  *
*********************/

namespace fs = boost::filesystem;
namespace boostio = boost::iostreams;

/*********************
* Scraper ENUMS      *
*********************/

// Have to have undefine ERROR and redefine as zero in enum because stupid windows.h has it already defined as zero, but as a macro.
#ifdef WIN32
#undef ERROR
#endif

enum logattribute {
    ERROR,
    INFO,
    WARNING,
    CRITICAL
};

enum statsobjecttype {
    NetworkWide,
    byCPID,
    byProject,
    byCPIDbyProject
};

static std::vector<std::string> vstatsobjecttypestrings = { "NetWorkWide", "byCPID", "byProject", "byCPIDbyProject" };

const std::string GetTextForstatsobjecttype(int EnumValue)
{
  return vstatsobjecttypestrings[EnumValue];
}

/*********************
* Global Vars        *
*********************/

extern bool fDebug;
extern std::string msMasterMessagePrivateKey;
extern CWallet* pwalletMain;
std::vector<std::pair<std::string, std::string>> vwhitelist;
std::vector<std::pair<std::string, std::string>> vuserpass;
std::vector<std::pair<std::string, int64_t>> vprojectteamids;
std::vector<std::string> vauthenicationetags;
std::string rpcauth = "boinc:test";
std::string rpcip = "http://127.0.0.1:9334/";
int64_t ndownloadsize = 0;
int64_t nuploadsize = 0;
bool fScraperActive = false;

struct ScraperFileManifestEntry
{
    std::string filename; // Filename
    std::string project;
    uint256 hash; // hash of file
    int64_t timestamp;
    bool current;
};

typedef std::map<std::string, ScraperFileManifestEntry> ScraperFileManifestMap;

struct ScraperFileManifest
{
    ScraperFileManifestMap mScraperFileManifest;
    uint256 nFileManifestMapHash;
    uint256 nConsensusBlockHash;
    int64_t timestamp;
};

typedef std::string ScraperID;
// The inner map is sorted in descending order of time. The pair is manifest hash, content hash.
typedef std::multimap<int64_t, pair<uint256, uint256>, greater <int64_t>> mCSManifest;
// Right now this is using sCManifestName, but needs to be changed to pubkey once the pubkey part is finished.
// See the ScraperID typedef above.
typedef std::map<ScraperID, mCSManifest> mmCSManifestsBinnedByScraper;

struct ScraperObjectStatsKey
{
    statsobjecttype objecttype;
    std::string objectID;
};

struct ScraperObjectStatsValue
{
    double dTC;
    double dRAT;
    double dRAC;
    double dMag;
};

struct ScraperObjectStats
{
    ScraperObjectStatsKey statskey;
    ScraperObjectStatsValue statsvalue;
};

struct ScraperObjectStatsKeyComp
{
    bool operator() ( ScraperObjectStatsKey a, ScraperObjectStatsKey b ) const
    {
        return std::make_pair(a.objecttype, a.objectID) < std::make_pair(b.objecttype, b.objectID);
    }
};

typedef std::map<ScraperObjectStatsKey, ScraperObjectStats, ScraperObjectStatsKeyComp> ScraperStats;

/*********************
* Global Constants   *
*********************/

// We may want to turn some of these into administrative message appcache entries...

// Define 48 hour retention time for stats files, current or not.
static int64_t SCRAPER_FILE_RETENTION_TIME = 48 * 3600;
// Define whether prior CScraperManifests are kept.
static bool SCRAPER_CMANIFEST_RETAIN_NONCURRENT = true;
// Define CManifest scraper object retention time.
static int64_t SCRAPER_CMANIFEST_RETENTION_TIME = 3 * 3600;
static bool SCRAPER_CMANIFEST_INCLUDE_NONCURRENT_PROJ_FILES = false;
static const double MAG_ROUND = 0.01;
static const double NEURALNETWORKMULTIPLIER = 115000;
// This settings below are important. This sets the minimum number of scrapers
// that must be available to form a convergence. Above this minimum, the ratio
// is followed. For example, if there are 4 scrapers, a ratio of 2/3 would require
// CEILING(0.66666... * 4) = 3. See NumScrapersForSupermajority below.
// If there is only 1 scraper available, and the mininum is 2, then a convergence
// will not happen. Setting this below 2 will allow convergence to happen without
// cross checking, and is undesirable, because the scrapers are not supposed to be
// trusted entities.
static const unsigned int SCRAPER_SUPERMAJORITY_MINIMUM = 2;
// Two-thirds seems like a reasonable standard for agreement.
static const double SCRAPER_SUPERMAJORITY_RATIO = 2.0 / 3.0;

/*********************
* Functions          *
*********************/

void _log(logattribute eType, const std::string& sCall, const std::string& sMessage);
void _nntester(logattribute eType, const std::string& sCall, const std::string& sMessage);
bool ScraperDirectorySanity();
bool StoreBeaconList(const fs::path& file);
bool LoadBeaconList(const fs::path& file, BeaconMap& mBeaconMap);
std::vector<std::string> split(const std::string& s, const std::string& delim);
extern AppCacheSection ReadCacheSection(const std::string& section);
uint256 GetFileHash(const fs::path& inputfile);
uint256 GetmScraperFileManifestHash();
bool StoreScraperFileManifest(const fs::path& file);
bool LoadScraperFileManifest(const fs::path& file);
bool InsertScraperFileManifestEntry(ScraperFileManifestEntry& entry);
unsigned int DeleteScraperFileManifestEntry(ScraperFileManifestEntry& entry);
bool MarkScraperFileManifestEntryNonCurrent(ScraperFileManifestEntry& entry);
ScraperStats GetScraperStatsByConsensusBeaconList();
bool LoadProjectFileToStatsByCPID(const std::string& project, const fs::path& file, const double& projectmag, const BeaconMap& mBeaconMap, ScraperStats& mScraperStats);
bool StoreStats(const fs::path& file, const ScraperStats& mScraperStats);
bool ScraperSaveCScraperManifestToFiles(uint256 nManifestHash);
bool IsScraperAuthorizedToBroadcastManifests();
bool ScraperSendFileManifestContents(std::string CManifestName);
mmCSManifestsBinnedByScraper BinCScraperManifestsByScraper();
mmCSManifestsBinnedByScraper ScraperDeleteCScraperManifests();
bool ScraperDeleteCScraperManifest(uint256 nManifestHash);
bool ScraperConstructConvergedManifest();

double MagRound(double dMag)
{
    return round(dMag / MAG_ROUND) * MAG_ROUND;
}

unsigned int NumScrapersForSupermajority(unsigned int nScraperCount)
{
    unsigned int nRequired = std::max(SCRAPER_SUPERMAJORITY_MINIMUM, (unsigned int)std::ceil(SCRAPER_SUPERMAJORITY_RATIO * nScraperCount));
    
    return nRequired;
}

/*********************
* Scraper            *
*********************/

class scraper
{
public:
    scraper();
};

/********************
* Scraper Curl      *
********************/

class statscurl
{
private:

    CURL* curl;
    std::string buffer;
    std::string buffertwo;
    std::string header;
    long http_code;
    CURLcode res;

    static size_t writeheader(void* ptr, size_t size, size_t nmemb, void* userp)
    {
        ((std::string*)userp)->append((char*)ptr, size * nmemb);

        return size * nmemb;
    }

    static size_t writetofile(void* ptr, size_t size, size_t nmemb, FILE* fp)
    {
        return fwrite(ptr, size, nmemb, fp);
    }

    static size_t writestring(const char* in, size_t size, size_t nmemb, std::string* out)
    {
            const std::size_t totalBytes(size * nmemb);

            out->append(in, totalBytes);

            return totalBytes;
    }

public:

    statscurl()
        : curl(curl_easy_init())
    {
    }

    ~statscurl()
    {
        if (curl)
        {
            curl_easy_cleanup(curl);
            curl_global_cleanup();
        }
    }

    bool httpcode(const std::string& response, const std::string& url)
    {
        // Check code to make sure we have success as even a response is considered an OK
        // We check only on head requests since they give us the information we need
        // Codes we send back true and wait for other HTTP/ code is 301, 302, 307 and 308 since these are follows
        if (response.empty())
        {
            _log(ERROR, "httpcode", "Server returned an empty HTTP code <prjurl=" + url+ ">");

            return false;
        }

        if (response == "200")
            return true;

        else if (response == "400")
            _log(ERROR, "httpcode", "Server returned a http code of Bad Request <prjurl=" + url + ", code=" + response + ">");

        else if (response == "401")
            _log(ERROR, "httpcode", "Server returned a http code of Unauthorized <prjurl=" + url + ", code=" + response + ">");

        else if (response == "403")
            _log(ERROR, "httpcode", "Server returned a http code of Forbidden <prjurl=" + url + ", code=" + response + ">");

        else if (response == "404")
            _log(ERROR, "httpcode", "Server returned a http code of Not Found <prjurl=" + url + ", code=" + response + ">");

        else if (response == "301")
            return true;

        else if (response == "302")
            return true;

        else if (response == "307")
            return true;

        else if (response == "308")
            return true;

        else
            _log(ERROR, "httpcode", "Server returned a http code <prjurl=" + url + ", code=" + response + ">");

        return false;
    }

    bool rpccall(const std::string& type, const std::string& params, std::string& reply)
    {
        std::string strdata;

        if (params.empty())
            strdata = "{\"jsonrpc\" : \"1.0\", \"id\" : \"" + type + "\", \"method\" : \"" + type + "\", \"params\" : []}";

        else
            strdata = "{\"jsonrpc\" : \"1.0\", \"id\" : \"" + type + "\", \"method\" : \"" + type + "\", \"params\" : [\"project\"]}";

        const char* data = strdata.c_str();
        struct curl_slist* rpcheaders = NULL;

        rpcheaders = curl_slist_append(rpcheaders, "content-type: text/plain;");

        curl_easy_setopt(curl, CURLOPT_URL, rpcip.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writestring);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffertwo);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_PROXY, "");
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, rpcheaders);
        curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1L);
        curl_easy_setopt(curl, CURLOPT_USERPWD, rpcauth.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(data));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

        res = curl_easy_perform(curl);

        // Stop memory leak from headers
        curl_slist_free_all(rpcheaders);

        if (res > 0)
        {
            _log(ERROR, "curl_rpccall", "Failed to receive reply from Gridcoin RPC <type=" + type + "> (" + curl_easy_strerror(res) + ")");

            return false;
        }

        //curl_easy_reset(curl);

        std::istringstream ssinput(buffertwo);

        for (std::string line; std::getline(ssinput, line);)
            reply.append(line);

        _log(INFO, "curl_rpccall", "Successful RPC call <tpye=" + type + ">");

        return true;

    }

    bool http_download(const std::string& url, const std::string& destination, const std::string& userpass)
    {
        try {
            FILE* fp;

            fp = fopen(destination.c_str(), "wb");

            if(!fp)
            {
                _log(ERROR, "url_http_download", "Failed to open file to download project data into <destination=" + destination + ">");

                return false;
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writetofile);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_PROXY, "");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1L);
            curl_easy_setopt(curl, CURLOPT_USERPWD, userpass.c_str());

            res = curl_easy_perform(curl);
            fclose(fp);

            if (res > 0)
            {
                if (fp)
                    fclose(fp);

                _log(ERROR, "curl_http_download", "Failed to download file <urlfile=" + url + "> (" + curl_easy_strerror(res) + ")");

                return false;
            }

            return true;

        }

        catch (std::exception& ex)
        {
            _log(ERROR, "curl_http_download", "Std exception occured (" + std::string(ex.what()) + ")");

            return false;
        }
    }

    bool http_header(const std::string& url, std::string& etag, const std::string& userpass)
    {
        struct curl_slist* headers = NULL;

        headers = curl_slist_append(headers, "Accept: */*");
        headers = curl_slist_append(headers, "User-Agent: curl/7.58.0");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeheader);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
        curl_easy_setopt(curl, CURLOPT_PROXY, "");
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1L);
        curl_easy_setopt(curl, CURLOPT_USERPWD, userpass.c_str());

        res = curl_easy_perform(curl);

        // Stop memory leak from headers
        curl_slist_free_all(headers);

        if (res > 0)
        {
            _log(ERROR, "curl_http_header", "Failed to capture header of file <urlfile=" + url + ">");

            return false;
        }

        std::istringstream ssinput(header);

        for (std::string line; std::getline(ssinput, line);)
        {
            if (line.find("HTTP/") != std::string::npos)
            {
                // Check the code response in header to make sure everything is as it should be
                std::vector<std::string> codevector = split(line, " ");

                if (!httpcode(codevector[1], url))
                    return false;
            }

            if (line.find("ETag: ") != std::string::npos)
            {
                size_t pos;

                std::string modstring = line;

                pos = modstring.find("ETag: ");

                etag = modstring.substr(pos+6, modstring.size());

                etag = etag.substr(1, etag.size() - 3);
            }
        }

        if (etag.empty())
        {
            _log(ERROR, "curl_http_header", "No ETag response from project url <urlfile=" + url + ">");

            return false;
        }

        if (fDebug)
            _log(INFO, "curl_http_header", "Captured ETag for project url <urlfile=" + url + ", ETag=" + etag + ">");

        return true;
    }

    bool http_download(const std::string& url, const std::string& destination)
    {
        try {
            FILE* fp;

            fp = fopen(destination.c_str(), "wb");

            if(!fp)
            {
                _log(ERROR, "url_http_download", "Failed to open file to download project data into <destination=" + destination + ">");

                return false;
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writetofile);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_PROXY, "");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            res = curl_easy_perform(curl);
            fclose(fp);

            if (res > 0)
            {
                if (fp)
                    fclose(fp);

                _log(ERROR, "curl_http_download", "Failed to download file <urlfile=" + url + "> (" + curl_easy_strerror(res) + ")");

                return false;
            }

            return true;

        }

        catch (std::exception& ex)
        {
            _log(ERROR, "curl_http_download", "Std exception occured (" + std::string(ex.what()) + ")");

            return false;
        }
    }

    bool http_header(const std::string& url, std::string& etag)
    {
        struct curl_slist* headers = NULL;

        headers = curl_slist_append(headers, "Accept: */*");
        headers = curl_slist_append(headers, "User-Agent: curl/7.58.0");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeheader);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
        curl_easy_setopt(curl, CURLOPT_PROXY, "");
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);

        // Stop memory leak from headers
        curl_slist_free_all(headers);

        if (res > 0)
        {
            _log(ERROR, "curl_http_header", "Failed to capture header of file <urlfile=" + url + ">");

            return false;
        }

        std::istringstream ssinput(header);
        for (std::string line; std::getline(ssinput, line);)
        {
            if (line.find("HTTP/") != std::string::npos)
            {
                // Check the code response in header to make sure everything is as it should be
                std::vector<std::string> codevector = split(line, " ");

                if (!httpcode(codevector[1], url))
                    return false;
            }

            if (line.find("ETag: ") != std::string::npos)
            {
                size_t pos;

                std::string modstring = line;

                pos = modstring.find("ETag: ");

                etag = modstring.substr(pos+6, modstring.size());

                etag = etag.substr(1, etag.size() - 3);
            }
        }

        if (etag.empty())
        {
            _log(ERROR, "curl_http_header", "No ETag response from project url <urlfile=" + url + ">");

            return false;
        }

        if (fDebug)
            _log(INFO, "curl_http_header", "Captured ETag for project url <urlfile=" + url + ", ETag=" + etag + ">");

        return true;
    }

};

/**********************
* Network Node Logger *
**********************/

class logger
{
private:

    //std::ofstream logfile;
    fs::ofstream logfile;

public:

    logger()
    {
        fs::path plogfile = GetDataDir() / "scraper.log";

        logfile.open(plogfile.c_str(), std::ios_base::out | std::ios_base::app);

        if (!logfile.is_open())
            printf("Logging : Failed to open logging file\n");
    }

    ~logger()
    {
        if (logfile.is_open())
        {
            logfile.flush();
            logfile.close();
        }
    }

    void output(const std::string& tofile)
    {
        if (logfile.is_open())
            logfile << tofile << std::endl;

        return;
    }
};

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

    void cleanappend(const std::string& value)
    {
        builtstring << value.substr(1, value.size());
    }

    void nlcleanappend(const std::string& value)
    {
        builtstring << value.substr(1, value.size()) << "\n";
    }

    void nlappend(const std::string& value)
    {
        builtstring << value << "\n";
    }

    void xmlappend(const std::string& xmlkey, const std::string& value)
    {
        builtstring << "<" << xmlkey << ">" << value << "</" << xmlkey << ">\n";
    }

    void xmlappend(const std::string& xmlkey, int64_t value)
    {
        builtstring << "<" << xmlkey << ">" << value << "</" << xmlkey << ">\n";
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

class gridcoinrpc
{
private:

    std::string whitelistrpcdata = "";
    std::string superblockage = "";

public:

    gridcoinrpc()
    {}

    ~gridcoinrpc()
    {}

    bool wlimport()
    {
#ifdef SCRAPER_STANDALONE
        whitelistrpcdata = "";
        Json::Value jsonvalue;
        Json::Reader jsonreader;
        Json::StreamWriterBuilder valuestring;
        Json::Value::const_iterator jsondata;
        // Call RPC for the data
        statscurl wl;

        if (!wl.rpccall("listdata", "project", whitelistrpcdata))
        {
            _log(ERROR, "whitelist_data_import", "Failed to receive RPC reply for whitelist data");

            return false;
        }

        if (whitelistrpcdata.find("\"result\":null") != std::string::npos)
        {
            _log(ERROR, "whitelist_data_import", "RPC replied with error (" + whitelistrpcdata + ")");

            return false;
        }

        // Try to parse json data.

        bool successparse = jsonreader.parse(whitelistrpcdata, jsonvalue);

        if (!successparse)
        {
            printf("Failed to parse\n");
            return false;
        }

        const Json::Value whitelist = jsonvalue["result"];

        // Populate the beacon report data into scraper since we got a reply of whitelist
        vwhitelist.clear();

        for (jsondata = whitelist.begin() ; jsondata != whitelist.end(); jsondata++)
        {

            valuestring.settings_["indentation"] = "";
            std::string outurl = Json::writeString(valuestring, *jsondata);

            if (jsondata.key().asString() == "Key Type")
                continue;

            vwhitelist.push_back(std::make_pair(jsondata.key().asString(), outurl.substr(1, (outurl.size() - 2))));
        }

        return true;
#else
        vwhitelist.clear();

        for(const auto& item : ReadCacheSection(Section::PROJECT))
            vwhitelist.push_back(std::make_pair(item.first, item.second.value));

        return true;
#endif
    }

    int64_t sbage()
    {
#ifdef SCRAPER_STANDALONE
        Json::Value jsonvalue;
        Json::Reader jsonreader;
        Json::StreamWriterBuilder valuestring;
        Json::Value::const_iterator jsondata;
        superblockage = "";
        // Call RPC for the data
        statscurl age;

        if (!age.rpccall("superblockage", "", superblockage))
        {
            _log(ERROR, "superblockage_data_import", "Failed to receive RPC reply for superblock data");

            return -1;
        }

        if (superblockage.find("\"result\":null") != std::string::npos)
        {
            _log(ERROR, "superblockage_data_import", "RPC replied with error (" + superblockage + ")");

            return -2;
        }

        // Try to parse json data.

        bool successparse = jsonreader.parse(superblockage, jsonvalue);

        if (!successparse)
        {
            printf("Failed to parse\n");
            return -3;
        }

        try
        {
            const Json::Value superblockdata = jsonvalue["result"];

            for (jsondata = superblockdata.begin() ; jsondata != superblockdata.end(); jsondata++)
            {

                valuestring.settings_["indentation"] = "";
                std::string sSBage = Json::writeString(valuestring, *jsondata);

                if (jsondata.key().asString() == "Superblock Age")
                {
                    printf("Outage is %s\n", sSBage.c_str());

                    return std::stoll(sSBage);
                }
            }

            return -4;
        }

        catch (std::exception& ex)
        {
            _log(ERROR, "superblockage_data_import", "Exception occured (" + std::string(ex.what()) + ")");

            return -5;
        }

        return -6;
#else
        int64_t superblock_time = ReadCache(Section::SUPERBLOCK, "magnitudes").timestamp;
        int64_t nSBage = GetAdjustedTime() - superblock_time;

        return nSBage;
#endif
    }
};

/*********************
* Userpass Data      *
*********************/

class userpass
{
private:

    fs::ifstream userpassfile;

public:

    userpass()
    {
        vuserpass.clear();

        fs::path plistfile = GetDataDir() / "userpass.dat";

        userpassfile.open(plistfile.c_str(), std::ios_base::in);

        if (!userpassfile.is_open())
            _log(CRITICAL, "userpass_data", "Failed to open userpass file");
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

            _log(INFO, "userpass_data_import", "Userpass contains " + std::to_string(vuserpass.size()) + " projects");

            return true;
        }

        catch (std::exception& ex)
        {
            _log(CRITICAL, "userpass_data_import", "Failed to userpass import due to exception (" + std::string(ex.what()) + ")");

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

    fs::ofstream oauthdata;
    stringbuilder outdata;

public:

    authdata(const std::string& project)
    {
        std::string outfile = project + "_auth.dat";
        fs::path poutfile = GetDataDir() / outfile.c_str();

        oauthdata.open(poutfile.c_str(), std::ios_base::out | std::ios_base::app);

        if (!oauthdata.is_open())
            _log(CRITICAL, "auth_data", "Failed to open auth data file");
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
                _log(CRITICAL, "user_data_export", "No authentication etags to be exported!");

                return false;
            }

            oauthdata.write(outdata.value().c_str(), outdata.size());

            _log(INFO, "auth_data_export", "Exported");

            return true;
        }

        catch (std::exception& ex)
        {
            _log(CRITICAL, "auth_data_export", "Failed to export auth data due to exception (" + std::string(ex.what()) + ")");

            return false;
        }
    }
};

/**********************
* Network Node Tester *
**********************/

class nntester
{
private:

    fs::ofstream logfile;

public:

    nntester()
    {
        fs::path plogfile = GetDataDir() / "scraper.time";

        logfile.open(plogfile.c_str(), std::ios_base::out | std::ios_base::app);

        if (!logfile.is_open())
            printf("Logging : Failed to open logging file\n");
    }

    ~nntester()
    {
        if (logfile.is_open())
        {
            logfile.flush();
            logfile.close();
        }
    }

    void output(const std::string& tofile)
    {
        if (logfile.is_open())
            logfile << tofile << std::endl;

        return;
    }
};

#endif // SCRAPER_H
