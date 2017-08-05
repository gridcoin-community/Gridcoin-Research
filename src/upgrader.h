#ifndef GRC_UPGRADER
#define GRC_UPGRADER

#include <curl/curl.h> // for downloading
#include <string>
#include <string.h>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

struct curlargs
{ 
    CURL *handle;
    bool downloading;
    bool success;
};

enum TARGET
{
    QT,
    DAEMON,
    UPGRADER,

    DATA,
    PROGRAM,
    
    BLOCKS
};

class Upgrader
{

private:
    boost::thread downloadThread;
    boost::mutex targetmutex;
    boost::mutex cancelmutex;
    bool fileInitialized;
    struct curlargs curlhandle;
    FILE *file;
    double filesize;
    bool filesizeRetrieved;
    std::vector<boost::filesystem::path> fvector(boost::filesystem::path path);
    std::string targetswitch(int target);
    bool verifyPath(boost::filesystem::path path, bool create);
    bool copyDir(boost::filesystem::path source, boost::filesystem::path target, bool recursive);
    bool safeProgramDir();
    boost::filesystem::path path(int pathfork);
    int globaltarget;

public:

    ~Upgrader();

    bool downloader (int target);
    bool unzipper   (int target);
    void launcher   (int launchtarget, int target);

//  switches around upgrade, target and backup:
    bool juggler(int pathfork, bool recovery);

//  return info about file being downloaded:
    unsigned long int getFileDone();
    unsigned long int getFileSize();
    int getFilePerc(long int sz);
    bool downloading() {return curlhandle.downloading;}
    bool downloadSuccess() {return curlhandle.success;}
    void cancelDownload(bool cancel);
    void initChecker();

//  set the upgrader's global target
    bool setTarget(int target);
    void unlockTarget();
    int getTarget() {return globaltarget;};

};

#endif
