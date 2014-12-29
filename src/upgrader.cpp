#include <stdio.h>      // for input output to terminal
#include <unistd.h>     // for sleep
#include <signal.h>
#include "upgrader.h"
#include "util.h"
#include "chilkat/include/CkZip.h"
#include "chilkat/include/CkZipEntry.h"
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

extern void StartShutdown();

namespace bfs = boost::filesystem;
namespace bpt = boost::posix_time;
typedef std::vector<bfs::path> vec;

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

std::string geturl()
{
	std::string url = "http://download.gridcoin.us/download/signed/";
	return url;
}

bfs::path GetProgramDir()
{
	bfs::path programdir = "/home/lederstrumpf/c++_playfield/GRCRestarter/program/";  // Naturally, this will be OS-dependent later
	return programdir;
}

bfs::path path(int pathfork)
{
	bfs::path path;
	
	switch (pathfork)
	{
		case DATA:
			path = GetDefaultDataDir();
			break;

		case PROGRAM:
			path = GetProgramDir();
			break;

		default:
			printf("Path not specified!\n");
	}

	return path;
}

void download(void *curlhandle)
	{
	struct curlargs *curlarg = (struct curlargs *)curlhandle;
	curl_easy_perform(curlarg->handle);
	curlarg->done=true;
	}

#if defined(UPGRADER)
int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("The fuck am I supposed to do with this? \nOptions: \nupgrade \ndownloadblocks \nextractblocks \njuggler \n");
		return 0;
	}

	Upgrader upgrader;
	
	if (strcmp(argv[1], "upgrade")==0)
	{
		std::string target = "gridcoin"; // Consider adding reference to directory so that GRCupgrade can be called from anywhere on linux!
		std::string source = "gridcoin-qt";
		if(!upgrader.downloader(target, PROGRAM, source)) {return 0;}
		if (!upgrader.juggler(PROGRAM, false)) 			{return 0;}
	}
	else if (strcmp(argv[1], "downloadblocks")==0)
	{
		std::string target = "snapshot.zip";
		std::string source = "snapshot.zip";
		if(!upgrader.downloader(target, DATA, source)) 	{return 0;}
		printf("Blocks downloaded successfully\n");
		if(!upgrader.unzipper(target, DATA))            {return 0;}
		printf("Blocks extracted successfully\n");
		if (!upgrader.juggler(DATA, false)) 			{return 0;}
		printf("Blocks copied successfully\n");
	}
	else if (strcmp(argv[1], "extractblocks")==0)
	{
		std::string target = "snapshot.zip";
		if(!upgrader.unzipper(target, DATA))             {return 0;}
		printf("Blocks extracted successfully\n");
	}
	else if (strcmp(argv[1], "juggler")==0)
	{
		if (upgrader.juggler(DATA, false))
		{
			printf("Copied file(s) successfully\n");
		}
	}
	else if (strcmp(argv[1], "path")==0)
	{
		printf("%s\n", GetDataDir().c_str());
	}
	else if (strcmp(argv[1], "justupgrade")==0)
	{
		#ifndef WIN32
		int parent = atoi(argv[2]);
		while (0 == kill(parent, 0))
		{
			printf("Parent still kicking\n");
			usleep(1000*800);
		}
		printf("\nParent dead\n");
		if (upgrader.juggler(PROGRAM, false))
		{
			printf("Copied files successfully\n");
		}
		#endif
	}
	else if (strcmp(argv[1], "justblocks")==0)
	{
		#ifndef WIN32
		int parent = atoi(argv[2]);
		while (0 == kill(parent, 0))
		{
			printf("Parent still kicking\n");
			usleep(1000*800);
		}
		printf("\nParent dead\n");
		std::string target = "snapshot.zip";
		if(!upgrader.unzipper(target, DATA))             {return 0;}
		printf("Blocks extracted successfully\n");
		if (upgrader.juggler(DATA, false))
		{
			printf("Copied files successfully\n");
		}
		#endif
	}
	else 
	{
		printf("That's not an option!\n");
		return 0;
	}
	return 1;
}
#endif

bool Upgrader::downloader(std::string targetfile, int pf, std::string source)
{
	std::string urlstring = geturl() + source;
	const char *url = urlstring.c_str();

	bfs::path target = path(pf) / "upgrade";
	if (!verifyPath(target, true)) {return false;}
	target /= targetfile;

	printf("%s\n",url);
	printf("%s\n",target.c_str());

	curlhandle.handle = curl_easy_init();
	curlhandle.done=0;

	file=fopen(target.c_str(), "wb");
	fileInitialized=true;

	curl_easy_setopt(curlhandle.handle, CURLOPT_URL, url);
	curl_easy_setopt(curlhandle.handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curlhandle.handle, CURLOPT_WRITEDATA, file);

	boost::thread(download, (void *)&curlhandle);

	printf("downloading file...\n");

	filesize = -1;
	filesizeRetrieved = false;

	while (!curlhandle.done)
		{
			usleep(800*1000);
			#if defined(UPGRADER)
			int sz = getFileDone();
			printf("\r%li\tKB", sz/1024);
			printf("\t%li%%", getFilePerc(sz));
			fflush( stdout );
			#endif
		}

	curl_easy_cleanup(curlhandle.handle);
	fclose(file);
	fileInitialized=false;

	printf("\nfile downloaded successfully\n");
	return true;
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
	if(filesize > 0)
	{
		return sz*100/(filesize);
	}
	return 0;
}

bool Upgrader::unzipper(std::string targetfile, int pf)
{
	bfs::path target = path(pf);
	target /= "upgrade";
	if (!verifyPath(target, true)) {return false;}
	
	CkZip zip;
	zip.UnlockComponent("unlockCode");

	if(zip.OpenZip((target / targetfile).c_str())) 
	{
		printf("Extracting archive into %s\n", target.c_str());
		if(!zip.Extract(target.c_str()))
		{
			printf("Extracting archive failed\n");
			return false;
		}
	}
	else 
	{
		printf("Could not open archive\n");
		return false;
	}
	zip.CloseZip();
	bfs::remove(target / targetfile);
	return true;
}

bool Upgrader::juggler(int pf, bool recovery)			// for upgrade, backs up target and copies over upgraded file
{														// for recovery, copies over backup
	bfs::path backupdir(path(DATA) / "backup");
	bfs::path sourcedir(path(DATA));
	if (recovery)
		sourcedir = backupdir;
	else sourcedir /= "upgrade";

	if (!verifyPath(sourcedir, true)) {return false;}

	printf("Copying %s into %s\n", path(pf).c_str(), (backupdir / bpt::to_simple_string(bpt::second_clock::local_time())).c_str());

	copyDir(path(pf), backupdir / bpt::to_simple_string(bpt::second_clock::local_time()));

	printf("Copying %s into %s\n", sourcedir.c_str(), path(pf).c_str());

	copyDir(sourcedir, path(pf));

	return true;
}

bool Upgrader::copyDir(bfs::path source, bfs::path target)
{
	if (!verifyPath(source, false))	{return false;}
	if (!verifyPath(target, true))	{return false;}
	
	vec iteraton = this->fvector(source);
	for (vec::const_iterator mongo (iteraton.begin()); mongo != iteraton.end(); ++mongo)
	{

	bfs::path fongo = *mongo;

	// if (!bfs::exists(sourcedir / fongo))
	// {
	// 	printf("Error, file does not exist\n");
	// 	return false;
	// }

	if (bfs::is_directory(source / fongo))
	{
		// printf("%s is a directory - time for recursion!\n", (source / fongo).c_str());
		// if (blacklistDirectory(fongo)) {continue;}
		if ((fongo == "upgrade") || (fongo == "backup") || (fongo == "now"))
		{
			// printf("Skip this shit\n");
			continue;
		}
		copyDir(source / fongo, target / fongo);
	}
	else
	{
		if (bfs::exists(target / fongo))
		{
			bfs::remove(target / fongo);
		} // avoid overwriting		

		bfs::copy_file(source / fongo, target / fongo); // the actual upgrade/recovery

		// printf("copied\n");
	}	
	}
	return true;
}

// bool Upgrader::CopyBlacklistDirectories(bfs::path path)
// {
// 	const char *blacklist[] = {
// 		"upgrade",
// 		"backup"
// 	}
// 	for 
// }

std::vector<bfs::path> Upgrader::fvector(bfs::path path)
{
	vec a;
 
	copy(bfs::directory_iterator(path), bfs::directory_iterator(), back_inserter(a));

	for (unsigned int i = 0; i < a.size(); ++i)
	{
		// printf("Member of directory to be copied: %s\n", a[i].c_str());
		a[i]=a[i].filename();	// We just want the filenames, not the whole paths as we are addressing three different directories with the content of concern
	}

	return a;
}

#ifndef UPGRADER
void Upgrader::upgrade()
{
	#ifndef WIN32
	std::stringstream pidstream;
	pidstream << getpid();
	std::string pid = pidstream.str();
	printf("Parent: %s\n", pid.c_str());
	if(!fork())
	{
		printf("Parent: %s\n", pid.c_str());
		execl("/home/lederstrumpf/Gridcoin-Research/src/upgrader", "upgrader", "justblocks", pid.c_str(), NULL);
		printf("Failed!");
	}
	StartShutdown();
	#endif
}
#endif

bool Upgrader::verifyPath(bfs::path path, bool create)
{
	if (bfs::exists(path))
		{
			return true;
		}
	else if (bfs::create_directories(path))
		{
			printf("%s successfully created\n", path.c_str());
			return true;
		}
	else 
		{
			printf("%s does not exist and could not be created!\n");
			return false;
		}
}
