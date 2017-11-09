#include "util.h"
#include <stdio.h>      // for input output to terminal
#include <signal.h>
#include "upgrader.h"
#include <zip.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <sstream>
#ifdef WIN32
#include <windows.h>
#include <winbase.h>
#else
#include <unistd.h>     // for sleep
#endif

extern void StartShutdown();

namespace bfs = boost::filesystem;
namespace bpt = boost::posix_time;
typedef std::vector<bfs::path> pathvec;

bool CANCEL_DOWNLOAD = false;

Upgrader upgrader;

/*
static int cancelDownloader(void *p,
                    curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow)
{
    if(CANCEL_DOWNLOAD) 
    {
        printf("\ncancelling download\n");
        return 1;
    }
    return 0;
}
*/

std::string geturl()
{
    std::string url = "https://download.gridcoin.us/download/downloadstake/signed/";
    // this will later be OS-dependent
    return url;
}

bfs::path Upgrader::path(int pathfork)
{
    bfs::path path;
    
    switch (pathfork)
    {
        case DATA:
            path = GetDataDir();
            break;

        case PROGRAM:
            path = GetProgramDir();
            break;

        default:
            printf("Path not specified!\n");
    }

    return path;
}

void Imker(void *kippel)
{
    RenameThread("grc-downloader");
    Upgrader *argon = (Upgrader*)kippel;
    printf("Starting download\n");
    argon->downloader(argon->getTarget());
    argon->unlockTarget();
}

void download(void *curlhandle)
{
    RenameThread("grc-curl");
    struct curlargs *curlarg = (struct curlargs *)curlhandle;
    CURLcode success = CURLE_OK;
    success = curl_easy_perform(curlarg->handle);
    if (success == CURLE_OK) { curlarg->success = true; }
    curlarg->downloading = false;
}

#if defined(UPGRADERFLAG)

bool waitForParent(int parent)
{   
    int delay = 0;
    int cutoff = 30;
    #ifdef WIN32
    printf("Parent: %i\n", parent);
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, parent);
    while (process != 0 && delay < cutoff)
        {
            printf("Waiting for client to exit...\n");
            Sleep(2000);
            CloseHandle(process);
            process = OpenProcess(SYNCHRONIZE, FALSE, parent);
            delay++;
        }
    
    #else
    while ((0 == kill(parent, 0)) && delay < cutoff)
        {
            printf("Waiting for client to exit...\n");
            usleep(1000*2000);
            delay++;
        }
    #endif
    printf((delay < cutoff)? "Client has exited\n" : "Client timed out\n" );
    return (delay < cutoff);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
#ifdef NO_UPGRADE
        printf("What am I supposed to do with this? \nOptions: \ndownloadblocks \nextractblocks \n");
#else
        printf("What am I supposed to do with this? \nOptions: \nqt \ndaemon \ndownloadblocks \nextractblocks \n");
#endif
        return 0;
    }

    Upgrader upgrader;
    ReadConfigFile(mapArgs, mapMultiArgs);

#ifndef NO_UPGRADE
    if (strcmp(argv[1], "qt")==0)
    {
        if(!upgrader.downloader(QT)) {return 0;}
        if (!upgrader.juggler(PROGRAM, false))          {return 0;}
        printf("Upgraded qt successfully\n");
    }
    else if (strcmp(argv[1], "daemon")==0)
    {
        if(!upgrader.downloader(DAEMON)) {return 0;}
        if (!upgrader.juggler(PROGRAM, false))          {return 0;}
    }
    else
#endif
    if (strcmp(argv[1], "downloadblocks")==0)
    {
        if(!upgrader.downloader(BLOCKS))    {return 0;}
        printf("Blocks downloaded successfully\n");
        if(!upgrader.unzipper(BLOCKS))            {return 0;}
        printf("Blocks extracted successfully\n");
        if (!upgrader.juggler(DATA, false))             {return 0;}
        printf("Blocks copied successfully\n");
    }
    else if (strcmp(argv[1], "extractblocks")==0)
    {
        if(!upgrader.unzipper(BLOCKS))             {return 0;}
        printf("Blocks extracted successfully\n");
        if (upgrader.juggler(DATA, false))
        {
            printf("Copied files successfully\n");
        }

    }

    // The following are for internal usage only

    else if (strcmp(argv[1], "gridcoinresearch")==0)
    {
        if (waitForParent(atoi(argv[2])))
        {
            if (upgrader.juggler(PROGRAM, false))
            {
                printf("Copied files successfully\n");
                upgrader.launcher(QT, -1);
            }
        }
    }
    else if (strcmp(argv[1], "gridcoinresearchd")==0)
    {
        if (waitForParent(atoi(argv[2])))
        {
            if(!upgrader.unzipper(BLOCKS))             {return 0;}
            printf("Blocks extracted successfully\n");
            if (upgrader.juggler(DATA, false))
            {
                printf("Copied files successfully\n");
                upgrader.launcher(DAEMON, -1);
            }
        }
    }
    else if (strcmp(argv[1], "snapshot.zip")==0)
    {
        if (waitForParent(atoi(argv[2])))
        {
            if(!upgrader.unzipper(BLOCKS))             {return 0;}
            printf("Blocks extracted successfully\n");
            if (upgrader.juggler(DATA, false))
            {
                printf("Copied files successfully\n");
                upgrader.launcher(atoi(argv[3]), -1);               
            }
        }       
    }

    //

    else 
    {
        printf("That's not an option!\n");
        return 0;
    }
    return 1;
}
#endif

bool Upgrader::setTarget(int target)
{
    if (targetmutex.try_lock())
    {
        globaltarget = target;
        curlhandle.downloading = true;
        return true;
    }
    return false;
}

void Upgrader::unlockTarget()
{
    targetmutex.unlock();
}

bool Upgrader::downloader(int targetfile)
{
    curlhandle.downloading = true;
    curlhandle.handle = curl_easy_init();
    curlhandle.success = false;

    std::string urlstring = geturl() + targetswitch(targetfile);
    const char *url = urlstring.c_str();

    bfs::path target = path(DATA) / "upgrade";
    // if user switches between upgrading client and bootstrapping blockchain, we don't want to pass around garbage
    if (bfs::exists(target)) {bfs::remove_all(target);} 
    
    if (!verifyPath(target, true)) {return false;}
    target /= targetswitch(targetfile);
    cancelDownload(false);
    if (bfs::exists(target))    {bfs::remove(target);}
    file = fopen(target.string().c_str(), "wb");
    fileInitialized=true;


    curl_easy_setopt(curlhandle.handle, CURLOPT_URL, url);
    curl_easy_setopt(curlhandle.handle, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curlhandle.handle, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curlhandle.handle, CURLOPT_WRITEDATA, file);
    //curl_easy_setopt(curlhandle.handle, CURLOPT_XFERINFOFUNCTION, cancelDownloader);
    curl_easy_setopt(curlhandle.handle, CURLOPT_NOPROGRESS, 0L);


    downloadThread = boost::thread(download, (void *)&curlhandle);

    printf("downloading file...\n");

    filesize = -1;
    filesizeRetrieved = false;

    while (curlhandle.downloading && !CANCEL_DOWNLOAD)
        {
            #ifdef WIN32
            Sleep(1000);
            #else
            usleep(1000*500);
            #endif
            #if defined(UPGRADERFLAG)
            int sz = getFileDone();
            printf("\r%i\tKB \t%i%%", sz/1024, getFilePerc(sz));
            fflush( stdout );
            #endif
        }

    curl_easy_cleanup(curlhandle.handle);
    fclose(file);
    fileInitialized=false;

    if(!curlhandle.success)
    {
        printf((CANCEL_DOWNLOAD)? "\ndownload interrupted\n" : "\ndownload failed\n");
        if (bfs::exists(target))    bfs::remove(target);
        cancelDownload(false);
        return false;
    }
    else
    {
        printf("\nfile downloaded successfully\n");
        return true;
    }
}

unsigned long int Upgrader::getFileDone()
{
    if (fileInitialized)
    {
        fseek(file, 0L, SEEK_END);
        return ftell(file);
    }

    return 0;
}

unsigned long int Upgrader::getFileSize()
{
    if (fileInitialized)
    {
        return filesize;
    }

    return 0;
}

int Upgrader::getFilePerc(long int sz)
{
    if(!filesizeRetrieved)
            {
                curl_easy_getinfo(curlhandle.handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &filesize);
                if(filesize > 0) 
                    {
                        filesizeRetrieved=true;
                        return 0;
                    }
            }
    
    return (filesize > 0)? (sz*100/(filesize)) : 0;
}

bool Upgrader::unzipper(int targetfile)
{
    bfs::path target = path(DATA) / "upgrade";
    if (!verifyPath(target.c_str(), true)) {return false;}

    const char *targetzip = (target / targetswitch(targetfile)).string().c_str();

    struct zip *archive;
    struct zip_file *zipfile;
    struct zip_stat filestat;
    char buffer[1024*1024];
    FILE *file;
    int bufferlength, err;
    unsigned long long sum;

    printf("Extracting %s\n", targetzip);

    if ((archive = zip_open(targetzip, 0, &err)) == NULL) {
        printf("Failed to open archive %s\n", targetzip);
        return false;
    }
 
    for (unsigned int i = 0; i < zip_get_num_entries(archive, 0); i++) 
    {
        if (zip_stat_index(archive, i, 0, &filestat) == 0)
        {
            verifyPath((target / filestat.name).parent_path(), true);
            if (!is_directory(target / filestat.name))
            {
                zipfile = zip_fopen_index(archive, i, 0);
                if (!zipfile) {
                    printf("Could not open %s in archive\n", filestat.name);
                    continue;
                }

                file = fopen((target / filestat.name).string().c_str(), "w");
 
                sum = 0;
                while (sum != filestat.size) {
                    bufferlength = zip_fread(zipfile, buffer, 1024*1024);
                    fwrite(buffer, sizeof(char), bufferlength, file);
                    sum += bufferlength;
                }
                printf("Finished extracting %s\n", filestat.name);
                fclose(file);
                zip_fclose(zipfile);
            }
        }
    }
    if (zip_close(archive) == -1) 
    {
        printf("Can't close zip archive %s\n", targetzip);
        return false;
    }

    bfs::remove(target / targetswitch(targetfile));
    return true;
}

bool Upgrader::juggler(int pf, bool recovery)           // for upgrade, backs up target and copies over upgraded file
{                                                       // for recovery, copies over backup
    static std::locale loc(std::cout.getloc(), new bpt::time_facet("%Y-%b-%d %H%M%S"));
    std::stringstream datestream;
    datestream.imbue(loc);
    datestream << bpt::second_clock::local_time();
    std::string subdir = (pf == BLOCKS)? "Blockchain " : "Client " + datestream.str();
    bfs::path backupdir(path(DATA) / "backup" / subdir);
    bfs::path sourcedir(path(DATA));
    if (recovery)
        sourcedir = backupdir;
    else sourcedir /= "upgrade";

    if (!verifyPath(sourcedir, true)) {return false;}

    if ((pf == PROGRAM) && (path(pf) == ""))
    {
        return false;
    }

    //printf("Copying %s into %s\n", path(pf).c_str(), backupdir.c_str());

    if ((pf == PROGRAM) && (safeProgramDir()))
    {
        pathvec iteraton = this->fvector(sourcedir);

        for (pathvec::const_iterator mongo (iteraton.begin()); mongo != iteraton.end(); ++mongo)
        {

            bfs::path fongo = *mongo;

            if (!bfs::exists(sourcedir / fongo) || bfs::is_directory(sourcedir / fongo))
            {
                continue;
            }

            if (bfs::exists(path(pf) / fongo) && !recovery) // i.e. backup files before upgrading
            {   if (!verifyPath(backupdir, true)) {return false;}
                bfs::copy_file(path(pf) / fongo, backupdir / fongo, bfs::copy_option::overwrite_if_exists);
            }

            if (bfs::exists(path(pf) / fongo)) // avoid overwriting
            {bfs::remove(path(pf) / fongo);}

            bfs::copy_file(sourcedir / fongo, path(pf) / fongo); // the actual upgrade/recovery

        }
    }
    else
    {
        copyDir(path(pf), backupdir, true);
        copyDir(sourcedir, path(pf), true);

    }

    if (!recovery)
    {
        bfs::remove_all(sourcedir); // include removing directory to avoid having users tempted to store files here
    }

    return true;
}

bool Upgrader::safeProgramDir()
{
    #ifndef WIN32
    return true;
    #endif
    return false;
}

bool Upgrader::copyDir(bfs::path source, bfs::path target, bool recursive)
{
    if (!verifyPath(source, false) || !verifyPath(target, true))    {return false;}
    
    pathvec iteraton = this->fvector(source);
    for (pathvec::const_iterator mongo (iteraton.begin()); mongo != iteraton.end(); ++mongo)
    {

    bfs::path fongo = *mongo;

    if (!bfs::exists(source / fongo))
    {
        printf("Error, file/directory does not exist\n");
        return false;
    }

    if (bfs::is_directory(source / fongo))
    {
        if ((fongo == "upgrade") || (fongo == "backup") || (!recursive))
        {
            continue;
        }
        verifyPath(target / fongo, true);
        copyDir(source / fongo, target / fongo, recursive);
    }
        else
        {
            if (bfs::exists(target / fongo))
            {
                bfs::remove(target / fongo);
            } // avoid overwriting      

            bfs::copy_file(source / fongo, target / fongo); // the actual upgrade/recovery

        }   
    }
    return true;
}

pathvec Upgrader::fvector(bfs::path path)
{
    pathvec a;
 
    copy(bfs::directory_iterator(path), bfs::directory_iterator(), back_inserter(a));

    for (unsigned int i = 0; i < a.size(); ++i)
    {
        a[i]=a[i].filename();   // We just want the filenames, not the whole paths as we are addressing three different directories with the content of concern
    }

    return a;
}

#if defined QT_GUI && defined WIN32
wchar_t *convertCharArrayToLPCWSTR(const char* charArray)
    {
        wchar_t* wString=new wchar_t[4096];
        MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, 4096);
        return wString;
    }
#endif


void Upgrader::launcher(int launchtarget, int launcharg)
{
    if (bfs::exists(GetProgramDir() / targetswitch(launchtarget)))
    {
        #ifndef WIN32
        std::stringstream pidstream;
        pidstream << getpid();
        std::string pid = pidstream.str();
        printf("Parent: %s\n", pid.c_str());
        if(!fork())
        {
            if(launchtarget!=UPGRADER)
            {
                execl((GetProgramDir() / targetswitch(launchtarget)).c_str(), targetswitch(launchtarget).c_str(), NULL);
            }
            else
            {
                std::stringstream launcher;
                #ifdef QT_GUI
                launcher << QT;
                #else
                launcher << DAEMON;
                #endif
                execl((GetProgramDir() / targetswitch(launchtarget)).c_str(), targetswitch(launchtarget).c_str(), targetswitch(launcharg).c_str() , pid.c_str(), launcher.str().c_str(), NULL);
            }
        }
        #else
        PROCESS_INFORMATION ProcessInfo;
        STARTUPINFO StartupInfo;
        ZeroMemory(&StartupInfo, sizeof(StartupInfo));
        StartupInfo.cb = sizeof StartupInfo;

        std::string argumentstring = targetswitch(launchtarget);
        argumentstring.append(" ");
        argumentstring.append(targetswitch(launcharg));
        argumentstring.append(" ");
        long unsigned int pid = GetCurrentProcessId();
        argumentstring.append(boost::lexical_cast<std::string>(pid));
        argumentstring.append(" ");
        #ifdef QT_GUI
        argumentstring.append(boost::lexical_cast<std::string>(QT));
        #else
        argumentstring.append(boost::lexical_cast<std::string>(DAEMON));
        #endif
        argumentstring.append(" ");

        char * argument = new char[argumentstring.length() + sizeof(char)];
        strcpy(argument, argumentstring.c_str());

        std::string programstring = (GetProgramDir() / targetswitch(launchtarget)).string();
        char * program =  new char[programstring.length()];
        strcpy(program, programstring.c_str());

        #if defined QT_GUI && defined WIN32
                wchar_t*  wcProgram = convertCharArrayToLPCWSTR(program);
                wchar_t* wcArgument = convertCharArrayToLPCWSTR(argument);
                CreateProcess(wcProgram, wcArgument, NULL, NULL, FALSE, 0, NULL, NULL, &StartupInfo, &ProcessInfo);
        #else
                CreateProcess(program, argument, NULL, NULL, FALSE, 0, NULL, NULL, &StartupInfo, &ProcessInfo);
        #endif

        delete argument;
        delete program;
        #endif
    }
    else
    {
        printf("Could not find %s\n", targetswitch(launchtarget).c_str());
    }
        #ifndef UPGRADERFLAG
        StartShutdown();
        #endif
}

bool Upgrader::verifyPath(bfs::path path, bool create)
{
    if (bfs::is_directory(path))
        {
            return true;
        }
    else if (create)
        {
            if (bfs::create_directories(path))
            {
            //printf("%s successfully created\n", path.c_str());
            return true;
            }
        }
    else
        {
            //printf("%s does not exist and could not be created!\n", path.c_str());
            return false;
        }
    return false;

}

std::string Upgrader::targetswitch(int target)
{
    switch(target)
    {
        case BLOCKS:
        return "snapshot.zip";

        case QT:
        return "gridcoinresearch";

        case DAEMON:
        return "gridcoinresearchd";

        case UPGRADER:
        return "gridcoinupgrader";

        default:
        return "";
    }
}

void Upgrader::cancelDownload(bool cancel)
{
    cancelmutex.lock();
    CANCEL_DOWNLOAD = cancel;
    cancelmutex.unlock();
}

Upgrader::~Upgrader()
{
    cancelDownload(true);
    setTarget(getTarget());
    unlockTarget();
    downloadThread.interrupt();
    downloadThread.join();
}
