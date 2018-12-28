#include <string>
#include <map>
#include <vector>
//#include <boost/thread.hpp>
//#include <boost/chrono.hpp>
#include <thread>
#include <chrono>

#include <boost/filesystem.hpp>


bool fShutdown = false;
bool fDebug = true;

extern void Scraper(bool fStandalone);

int main()
{
    Scraper(true);
    
    return 0;
}

/****************************
* Sanity for standlone mode *
*****************************/

std::vector<std::string> split(const std::string& s, const std::string& delim)
{
    size_t pos = 0;
    size_t end = 0;
    std::vector<std::string> elems;

    while((end = s.find(delim, pos)) != std::string::npos)
    {
        elems.push_back(s.substr(pos, end - pos));
        pos = end + delim.size();
    }

    elems.push_back(s.substr(pos, end - pos));

    return elems;
}

std::string ExtractXML(const std::string XMLdata, const std::string key, const std::string key_end)
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

void MilliSleep(int64_t n)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(n));
}

int64_t GetAdjustedTime()
{
    return time(NULL);
}

bool Contains(const std::string& data, const std::string& instring)
{
    return data.find(instring) != std::string::npos;
}

boost::filesystem::path GetDataDir()
{
    return boost::filesystem::current_path();
}