#include "scraper.h"

extern bool fShutdown;

bool find(const std::string& s, const std::string& find);
std::string urlsanity(const std::string& s, const std::string& type);
std::string lowercase(std::string s);
std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);
std::vector<std::string> vXMLData(const std::string& xmldata, int64_t teamid);
int64_t teamid(const std::string& xmldata);

bool WhitelistPopulated();
bool UserpassPopulated();
bool DownloadProjectTeamFiles();
int64_t ProcessProjectTeamFile(const fs::path& file, const std::string& etag);
bool DownloadProjectRacFiles();
bool ProcessProjectRacFile(const fs::path& file, const std::string& etag, int64_t teamid);
bool AuthenticationETagUpdate(const std::string& project, const std::string& etag);
void AuthenticationETagClear();

// No header storage by day number gz
bool xDownloadProjectTeamFiles();
int64_t xProcessProjectTeamFile(const fs::path& file);
bool xDownloadProjectRacFiles();
bool xProcessProjectRacFile(const fs::path& file, int64_t teamid);
int GetDayOfYear(int64_t timestamp);

void testdata(const std::string& etag);

// Note these two are initialized here for standalone mode compile, but will be overwritten by
// the values from GetArg in init.cpp when compiled as part of the wallet.
unsigned int nScraperSleep = 60000;
boost::filesystem::path pathScraper = fs::current_path() / "Scraper";

extern void MilliSleep(int64_t n);

void Scraper(bool fScraperStandalone)
{
    while (fScraperStandalone || !fShutdown)
    {
        // Refresh the whitelist if its available

        gridcoinrpc data;

        int64_t sbage = data.sbage();

        // Give 300 seconds before superblock needed before we sync
        if (sbage <= 86100 && sbage >= 0)
            _log(INFO, "main", "Superblock not needed. age=" + std::to_string(sbage));

        else if (sbage <= -1)
            _log(ERROR, "main", "RPC error occured, check logs");

        else
        {
            if (!data.wlimport())
                _log(WARNING, "main", "Refreshing of whitelist failed.. using old data");

            else
                _log(INFO, "main", "Refreshing of whitelist completed");

            AuthenticationETagClear();

            DownloadProjectTeamFiles();

            DownloadProjectRacFiles();

        }

        _nntester(INFO, "MAIN", "download size so far: " + std::to_string(ndownloadsize) + " upload size so far: " + std::to_string(nuploadsize));

        _log(INFO, "MAIN", "Sleeping for " + std::to_string(nScraperSleep) +" milliseconds");

        MilliSleep(nScraperSleep);
    }
}

/**********************
* Sanity              *
**********************/

bool find(const std::string& s, const std::string& find)
{
    size_t pos = s.find(find);

    if (pos == std::string::npos)
        return false;

    else
        return true;
}

std::string urlsanity(const std::string& s, const std::string& type)
{
    stringbuilder url;

    if (find(s, "einstein"))
    {
        url.append(s.substr(0,4));
        url.append("s");
        url.append(s.substr(4, s.size()));
    }

    else
        url.append(s);

    url.append("stats/");
    url.append(type);

    if (find(s, "gorlaeus") || find(s, "worldcommunitygrid"))
        url.append(".xml.gz");

    else
        url.append(".gz");

    return url.value();
}

std::string lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    return s;
}

int64_t teamid(const std::string& xmldata)
{
    std::string sXML = xmldata; // copy so we can iterate this.

    while (!sXML.empty())
    {
        size_t loca = sXML.find("<team>");
        size_t locb = sXML.find("</team>");

        std::string substr = sXML.substr(loca, locb + 7);

        if (lowercase(ExtractXML(substr, "<name>", "</name")) == "gridcoin")
        {
            std::string teamidstr = ExtractXML(substr, "<id>", "</id>");

            return std::stol(teamidstr);
        }

        sXML = sXML.substr(locb + 7, sXML.size());
    }

    return 0;
}

std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end)
{

    std::string extraction = "";
    std::string::size_type loc = XMLdata.find( key, 0 );

    if(loc != std::string::npos)
    {
        std::string::size_type loc_end = XMLdata.find(key_end, loc+3);

        if (loc_end != std::string::npos)
            extraction = XMLdata.substr(loc+(key.length()),loc_end-loc-(key.length()));
    }

    return extraction;
}

/**********************
* Scraper Logger      *
**********************/

void _log(logattribute eType, const std::string& sCall, const std::string& sMessage)
{
    std::string sType;
    std::string sOut;

    try
    {
        switch (eType)
        {
            case INFO:        sType = "INFO";        break;
            case WARNING:     sType = "WARNING";     break;
            case ERROR:       sType = "ERROR";       break;
            case CRITICAL:    sType = "CRITICAL";    break;
        }
    }

    catch (std::exception& ex)
    {
        printf("Logger : exception occured in _log function (%s)\n", ex.what());

        return;
    }

    stringbuilder string;

    string.append(std::to_string(time(NULL)));
    string.append(" [");
    string.append(sType);
    string.append("] <");
    string.append(sCall);
    string.append("> : ");
    string.append(sMessage);
    sOut = string.value();

    logger log;

    log.output(sOut);

    return;
}

void _nntester(logattribute eType, const std::string& sCall, const std::string& sMessage)
{
    std::string sType;
    std::string sOut;

    try
    {
        switch (eType)
        {
            case INFO:        sType = "INFO";        break;
            case WARNING:     sType = "WARNING";     break;
            case ERROR:       sType = "ERROR";       break;
            case CRITICAL:    sType = "CRITICAL";    break;
        }
    }

    catch (std::exception& ex)
    {
        printf("Logger : exception occured in _log function (%s)\n", ex.what());

        return;
    }

    stringbuilder string;

    string.append(std::to_string(time(NULL)));
    string.append(" [");
    string.append(sType);
    string.append("] <");
    string.append(sCall);
    string.append("> : ");
    string.append(sMessage);
    sOut = string.value();

    nntester log;

    log.output(sOut);

    return;
}

/**********************
* Populate Whitelist  *
**********************/

bool WhitelistPopulated()
{
    if (vwhitelist.empty())
    {
        _log(INFO, "WhitelistPopulated", "Whitelist vector currently empty; populating");

        gridcoinrpc data;

        if (data.wlimport())
            _log(INFO, "WhitelistPopulated", "Successfully populated whitelist vector");

        else
        {
            _log(CRITICAL, "WhitelistPopulated", "Failed to populate whitelist vector; keeping old whitelist data for time being");

            return false;
        }
    }

    _log(INFO, "WhitelistPopulated", "Whitelist is populated; Contains " + std::to_string(vwhitelist.size()) + " projects");

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
        _log(INFO, "UserpassPopulated", "Userpass vector currently empty; populating");

        userpass up;

        if (up.import())
            _log(INFO, "UserPassPopulated", "Successfully populated userpass vector");

        else
        {
            _log(CRITICAL, "UserPassPopulated", "Failed to populate userpass vector");

            return false;
        }
    }

    _log(INFO, "UserPassPopulated", "Userpass is populated; Contains " + std::to_string(vuserpass.size()) + " projects");

    return true;
}

/**********************
* Project Team Files  *
**********************/

bool DownloadProjectTeamFiles()
{
    vprojectteamids.clear();

        if (!WhitelistPopulated())
        {
            _log(CRITICAL, "DownloadProjectTeamFiles", "Whitelist is not populated");

            return false;
        }

        if (!UserpassPopulated())
        {
            _log(CRITICAL, "DownloadProjectTeamFiles", "Userpass is not populated");

            return false;
        }

        for (const auto& prjs : vwhitelist)
        {
            _log(INFO, "DownloadProjectTeamFiles", "Downloading project file for " + prjs.first);

            std::vector<std::string> vPrjUrl = split(prjs.second, "@");

            std::string sUrl = urlsanity(vPrjUrl[0], "team");

            std::string team_file_name = prjs.first + "-team.gz";

            fs::path team_file = pathScraper / team_file_name.c_str();

 //           if (fs::exists(team_file))
//                fs::remove(team_file);

            // Grab ETag of team file
            statscurl teametagcurl;
            std::string sTeamETag;

            bool buserpass = false;
            std::string userpass;

            for (const auto& up : vuserpass)
            {
                if (up.first == prjs.first)
                {
                    buserpass = true;

                    userpass = up.second;

                    break;
                }
            }

            if (buserpass)
            {
                if (!teametagcurl.http_header(sUrl, sTeamETag, userpass))
                {
                    _log(ERROR, "DownloadProjectTeamFiles", "Failed to pull team header file for " + prjs.first);

                    continue;
                }
            }

            else
                if (!teametagcurl.http_header(sUrl, sTeamETag))
                {
                    _log(ERROR, "DownloadProjectTeamFiles", "Failed to pull team header file for " + prjs.first);

                    // todo remove these files
                    continue;
                }

            _log(INFO, "DownloadProjectTeamFiles", "Successfully pulled team header file for " + prjs.first);


            if (buserpass)
            {
                authdata ad(lowercase(prjs.first));

                ad.setoutputdata("team", prjs.first, sTeamETag);

                if (!ad.xport())
                    _log(CRITICAL, "DownloadProjectTeamFiles", "Failed to export etag for " + prjs.first + " to authentication file");
            }

            std::string chketagfile = sTeamETag + ".gz";
            fs::path chkfile = pathScraper / chketagfile.c_str();

            if (fs::exists(chkfile))
            {
                _log(INFO, "DownloadProjectTeamFiles", "Etag file for " + prjs.first + " already exists");

                _nntester(INFO, "DownloadProjectTeamFiles", "Etag file for " + prjs.first + " already exists");

                continue;
            }

            else
                fs::remove(team_file);

            _nntester(INFO, "DownloadProjectTeamFiles", "Etag file for " + prjs.first + " does not exists");

            statscurl teamcurl;

            if (buserpass)
            {
                if (!teamcurl.http_download(sUrl, team_file.string(), userpass))
                {
                    _log(ERROR, "DownloadProjectTeamFiles", "Failed to download project team file for " + prjs.first);

                    continue;
                }
            }

            else
                if (!teamcurl.http_download(sUrl, team_file.string()))
                {
                    _log(ERROR, "DownloadProjectTeamFiles", "Failed to download project team file for " + prjs.first);

                    continue;
                }

            int64_t teamid = ProcessProjectTeamFile(team_file.string(), sTeamETag);

            vprojectteamids.push_back(std::make_pair(prjs.first, teamid));
        }

    return true;
}

int64_t ProcessProjectTeamFile(const fs::path& file, const std::string& etag)
{
    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(ERROR, "ProcessProjectTeamFile", "Failed to open team gzip file (" + file.string() + ")");

        return 0;
    }

    _log(INFO, "ProcessProjectTeamFile", "Opening team file (" + file.string() + ")");

    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string gzetagfile = "";

    // If einstein we store different
    if (file.string().find("einstein") != std::string::npos)
        gzetagfile = "einstein_team.gz";

    else
        gzetagfile = etag + ".gz";

    gzetagfile = ((fs::path)(pathScraper / gzetagfile)).c_str();

    std::ofstream outgzfile(gzetagfile, std::ios_base::out | std::ios_base::binary);

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(INFO, "ProccessProjectTeamFile", "Started processing " + file.string());

    stringbuilder outdata;
    outdata.nlappend("<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>");
    outdata.nlappend("<teams>");
    stream << outdata.value();

    bool bcomplete = false;
    bool bfileerror = false;
    std::vector<std::string> vXML;

    while (!bcomplete && !bfileerror)
    {
        // Find teams block
        std::string line;

        while (std::getline(in, line))
        {
            if (line != "<teams>")
                continue;

            else
                break;
        }
        // <teams> block found
        // Lets vector the  <user> blocks

        while (std::getline(in, line))
        {
            if (bcomplete)
                break;

            if (line == "</teams>")
            {
                bcomplete = true;

                break;
            }

            if (line == "<team>")
            {
                stringbuilder teamdata;

                teamdata.nlappend(line);

                while (std::getline(in, line))
                {
                    if (line.empty())
                    {
                        _log(CRITICAL, "ProcessProjectTeamFile", "An empty line has been found in " + file.string() + ". This usually means file was incomplete when downloaded; Moving on");

                        bfileerror = true;

                        break;
                    }

                    if (line == "</team>")
                    {
                        teamdata.nlappend(line);

                        if (lowercase(ExtractXML(teamdata.value(), "<name>", "</name>")) == "gridcoin")
                        {
                            vXML.push_back(teamdata.value());

                            bcomplete = true;

                            break;
                        }

                        else
                            break;
                    }

                    if (bcomplete)
                        break;

                    teamdata.nlcleanappend(line);
                }
            }
        }

        // If the file is complete/incomplete we will reach here. however bcomplete would be true to break this outter most loop.
        // In case where we reach here but the bool for bcomplete is not true then the file was incomplete!
        if (!bcomplete)
            bfileerror = true;
    }

    if (bfileerror)
    {
        _log(CRITICAL, "ProcessProjectTeamFile", "Error in data processing of " + file.string()  + "; Aborted processing");

        std::string efile = etag + ".gz";
        fs::path fsepfile = pathScraper / efile;
        ingzfile.close();
        outgzfile.flush();
        outgzfile.close();

        if (fs::exists(fsepfile))
            fs::remove(fsepfile);

        if (fs::exists(file))
            fs::remove(file);

        return false;
    }

    _log(INFO, "ProccessProjectTeamFile", "Finished processing " + file.string());
    _log(INFO, "ProccessProjectTeamFile", "Started processing team data and stripping");

    // Strip the team xml data to contain only what we need to be compressed from vector
    // We Need:
    // id, name
    // We Don't need:
    // type, userid, total_credit, expavg_credit, expavg_time, founder_name, url, name_html, description
    // In reality we do not need those since NN does those calculations and does not use the extra data
    // I've also opted to save 1 byte a line by not doing newlines since this a scraper
    std::string steamid;

    for (auto const& vv : vXML)
    {
        std::string sid = ExtractXML(vv, "<id>", "</id>");
        std::string sname = ExtractXML(vv, "<name>", "</name>");
        stringbuilder xmlout;

        xmlout.nlappend("<team>");
        xmlout.xmlappend("id", sid);
        xmlout.xmlappend("name", sname);
        xmlout.nlappend("</team>");
        xmlout.append("</teams>");

        stream << xmlout.value();

        steamid = sid;
    }

    _log(INFO, "ProccessProjectTeamFile", "Finished processing team rac data; stripping complete");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    ingzfile.close();
    outgzfile.flush();
    outgzfile.close();

    _log(INFO, "ProccessProjectTeamFile", "Complete Process");

    try
    {
        size_t filea = fs::file_size(file);
        fs::path temp = gzetagfile.c_str();
        size_t fileb = fs::file_size(temp);

        _nntester(INFO, "ProcessProjectTeamFile", "Processing new team file " + file.string() + "(" + std::to_string(filea) + " -> " + std::to_string(fileb) + ")");

        ndownloadsize += (int64_t)filea;
        nuploadsize += (int64_t)fileb;
    }

    catch (fs::filesystem_error& e)
    {
        _log(INFO, "ProcessProjectTeamFile", "FS Error -> " + std::string(e.what()));
    }

    if (fs::exists(file))
        fs::remove(file);

    if (!steamid.empty())
        return std::stol(steamid);

    _log(ERROR, "ProcessProjectTeamFile", "Unable to determine team ID");

    return 0;
}

/**********************
* Project RAC Files   *
**********************/

bool DownloadProjectRacFiles()
{
        if (!WhitelistPopulated())
        {
            _log(CRITICAL, "DownloadProjectRacFiles", "Whitelist is not populated");

            return false;
        }

        if (!UserpassPopulated())
        {
            _log(CRITICAL, "DownloadProjectRacFiles", "Userpass is not populated");

            return false;
        }

        for (const auto& prjs : vwhitelist)
        {
            _log(INFO, "DownloadProjectRacFiles", "Downloading project file for " + prjs.first);

            std::vector<std::string> vPrjUrl = split(prjs.second, "@");

            std::string sUrl = urlsanity(vPrjUrl[0], "user");

            std::string rac_file_name = prjs.first + +"-user.gz";

            fs::path rac_file = pathScraper / rac_file_name.c_str();

//            if (fs::exists(rac_file))
//                fs::remove(rac_file);

            // Grab ETag of team file
            statscurl racetagcurl;
            std::string sRacETag;

            bool buserpass = false;
            std::string userpass;

            for (const auto& up : vuserpass)
            {
                if (up.first == prjs.first)
                {
                    buserpass = true;

                    userpass = up.second;

                    break;
                }
            }

            if (buserpass)
            {
                if (!racetagcurl.http_header(sUrl, sRacETag, userpass))
                {
                    _log(ERROR, "DownloadProjectRacFiles", "Failed to pull rac header file for " + prjs.first);

                    continue;
                }
            }

            else
                if (!racetagcurl.http_header(sUrl, sRacETag))
                {
                    _log(ERROR, "DownloadProjectRacFiles", "Failed to pull rac header file for " + prjs.first);

                    continue;
                }

            if (sRacETag.empty())
            {
                _log(ERROR, "DownloadProjectRacFiles", "ETag for project is empty" + prjs.first);

                continue;
            }

            else
            _log(INFO, "DownloadProjectRacFiles", "Successfully pulled rac header file for " + prjs.first);

            if (buserpass)
            {
                authdata ad(lowercase(prjs.first));

                ad.setoutputdata("user", prjs.first, sRacETag);

                if (!ad.xport())
                    _log(CRITICAL, "DownloadProjectRacFiles", "Failed to export etag for " + prjs.first + " to authentication file");
            }

            std::string chketagfile = sRacETag + ".gz";
            fs::path chkfile = pathScraper / chketagfile.c_str();

            if (fs::exists(chkfile))
            {
                _log(INFO, "DownloadProjectRacFiles", "Etag file for " + prjs.first + " already exists");

                _nntester(INFO, "DownloadProjectRacFiles", "Etag file for " + prjs.first + " already exists");

                continue;
            }

            else
                fs::remove(chkfile);

            _nntester(INFO, "DownloadProjectRacFiles", "Etag file for " + prjs.first + " already exists");

            statscurl raccurl;

            if (buserpass)
            {
                if (!raccurl.http_download(sUrl, rac_file.string(), userpass))
                {
                    _log(ERROR, "DownloadProjectRacFiles", "Failed to download project rac file for " + prjs.first);

                    continue;
                }
            }

            else
                if (!raccurl.http_download(sUrl, rac_file.string()))
                {
                    _log(ERROR, "DownloadProjectRacFiles", "Failed to download project rac file for " + prjs.first);

                    continue;
                }

            int64_t team_id = 0;

            for (auto const& vprj : vprojectteamids)
            {
                if (vprj.first != prjs.first)
                    continue;

                else
                    team_id = vprj.second;
            }

            if (team_id == 0)
            {
                _log(ERROR, "DownloadProjectRacFiles", "TEAM ID was not found for project " + prjs.first);

                continue;
            }
            ProcessProjectRacFile(rac_file.string(), sRacETag, team_id);
        }

    return true;
}

bool ProcessProjectRacFile(const fs::path& file, const std::string& etag, int64_t teamid)
{
    std::ifstream ingzfile(file.string().c_str(), std::ios_base::in | std::ios_base::binary);

    if (!ingzfile)
    {
        _log(ERROR, "ProcessProjectRacFile", "Failed to open rac gzip file (" + file.string() + ")");

        return false;
    }

    _log(INFO, "ProcessProjectRacFile", "Opening rac file (" + file.string() + ")");

    boostio::filtering_istream in;

    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string gzetagfile = "";

    // If einstein we store different
    if (file.string().find("einstein") != std::string::npos)
        gzetagfile = "einstein_user.gz";

    else
        gzetagfile = etag + ".gz";

    gzetagfile = ((fs::path)(pathScraper / gzetagfile)).c_str();

    std::ofstream outgzfile(gzetagfile, std::ios_base::out | std::ios_base::binary);

    boostio::filtering_istream out;
    out.push(boostio::gzip_compressor());
    std::stringstream stream;

    _log(INFO, "ProccessProjectRacFile", "Started processing " + file.string());

    stringbuilder outdata;
    outdata.nlappend("<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>");
    stream << outdata.value();

    bool bcomplete = false;
    bool bfileerror = false;
    std::vector<std::string> vXML;

    while (!bcomplete && !bfileerror)
    {
        // Find users block
        std::string line;

        while (std::getline(in, line))
        {
            if (line != "<users>")
                continue;

            else
                break;
        }
        // <users> block found
        // Lets vector the  <user> blocks

        while (std::getline(in, line))
        {
            if (bcomplete)
                break;

            if (line == "</users>")
            {
                bcomplete = true;

                break;
            }

            if (line == "<user>")
            {
                stringbuilder userdata;

                userdata.nlappend(line);

                while (std::getline(in, line))
                {
                    if (line == "</user>")
                    {
                        userdata.nlappend(line);

                        if (ExtractXML(userdata.value(), "<teamid>", "</teamid>") == std::to_string(teamid))
                            vXML.push_back(userdata.value());

                        break;
                    }

                    userdata.nlcleanappend(line);
                }
            }
        }

        // If the file is complete/incomplete we will reach here. however bcomplete would be true to break this outter most loop.
        // In case where we reach here but the bool for bcomplete is not true then the file was incomplete!
        if (!bcomplete)
            bfileerror = true;
    }

    if (bfileerror)
    {
        _log(CRITICAL, "ProcessProjectRacFile", "Error in data processing of " + file.string() + "; Aborted processing");

        std::string efile = etag + ".gz";
        fs::path fsepfile = pathScraper/ efile;
        ingzfile.close();
        outgzfile.flush();
        outgzfile.close();

        if (fs::exists(fsepfile))
            fs::remove(fsepfile);

        if (fs::exists(file))
            fs::remove(file);

        return false;
    }

    _log(INFO, "ProccessProjectRacFile", "Finished processing " + file.string());
    _log(INFO, "ProccessProjectRacFile", "Started processing team rac data and stripping");

    // Strip the team rac xml data to contain only why we need to be compressed from vector
    // We Need:
    // total_credit, expavg_time, expavg_credit, teamid, cpid
    // We Don't need:
    // id, country, name, url
    // In reality we DO NOT need total_credit till TCD
    // In reality we DO NOT need expavg_time but this will make nn compatible
    // Also since we know teamid in this function we do not have to extract that data with ExtractXML
    // I've also opted to save 1 byte a line by not doing newlines since this a scraper

    stringbuilder xmlout;

    xmlout.nlappend("<users>");

    for (auto const& vv : vXML)
    {
        std::string stotal_credit = ExtractXML(vv, "<total_credit>", "</total_credit>");
        std::string sexpavg_time = ExtractXML(vv, "<expavg_time>", "</expavg_time>");
        std::string sexpavg_credit = ExtractXML(vv, "<expavg_credit>", "</expavg_credit>");
        std::string scpid = ExtractXML(vv, "<cpid>", "</cpid>");

        xmlout.nlappend("<user>");
        xmlout.xmlappend("total_credit", stotal_credit);
        xmlout.xmlappend("expavg_time", sexpavg_time);
        xmlout.xmlappend("expavg_credit", sexpavg_credit);
        xmlout.xmlappend("cpid", scpid);
        xmlout.xmlappend("teamid", teamid);
        xmlout.nlappend("</user>");
    }

    xmlout.nlappend("</users>");

    stream << xmlout.value();

    _log(INFO, "ProccessProjectRacFile", "Finished processing team rac data; stripping complete");

    out.push(stream);
    boost::iostreams::copy(out, outgzfile);
    ingzfile.close();
    outgzfile.flush();
    outgzfile.close();

    try
    {
    size_t filea = fs::file_size(file);
    fs::path temp = gzetagfile.c_str();
    size_t fileb = fs::file_size(temp);

    _nntester(INFO, "ProcessProjectRacFile", "Processing new rac file " + file.string() + "(" + std::to_string(filea) + " -> " + std::to_string(fileb) + ")");

    ndownloadsize += (int64_t)filea;
    nuploadsize += (int64_t)fileb;

    }

    catch (fs::filesystem_error& e)
    {
        _log(INFO, "ProcessProjectRacFile", "FS Error -> " + std::string(e.what()));
    }

    fs::remove(file);

    _log(INFO, "ProccessProjectRacFile", "Complete Process");

    return true;
}

void testdata(const std::string& etag)
{
    std::ifstream ingzfile( (pathScraper / (fs::path)(etag + ".gz")).c_str(), std::ios_base::in | std::ios_base::binary);


    boostio::filtering_istream in;
    in.push(boostio::gzip_decompressor());
    in.push(ingzfile);

    std::string line;
    printf("size: %lu\n", in.size());
    while (std::getline(in, line))
    {
        printf("data: %s\n", line.c_str());
    }
}
