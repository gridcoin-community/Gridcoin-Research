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

bool CANCEL_DOWNLOAD = false;
bool DOWNLOAD_SUCCESS = false;

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
	bfs::path programdir = bfs::current_path();  // Naturally, this will be OS-dependent later
	return programdir;
}

bfs::path Upgrader::path(int pathfork)
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
	CURLcode success = CURLE_OK;
	printf("Starting the downz\n");
	success = curl_easy_perform(curlarg->handle);
	if (success == CURLE_OK) { curlarg->success = true; }
	printf("Finished downloading\n");
	curlarg->done = true;
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
		std::string target = "gridcoin-qt"; // Consider adding reference to directory so that GRCupgrade can be called from anywhere on linux!
		std::string source = "gridcoin-qt";
		if(!upgrader.downloader(QT)) {return 0;}
		if (!upgrader.juggler(PROGRAM, false)) 			{return 0;}
	}
	else if (strcmp(argv[1], "downloadblocks")==0)
	{
		if(!upgrader.downloader(BLOCKS)) 	{return 0;}
		printf("Blocks downloaded successfully\n");
		if(!upgrader.unzipper(BLOCKS))            {return 0;}
		printf("Blocks extracted successfully\n");
		if (!upgrader.juggler(DATA, true)) 			{return 0;}
		printf("Blocks copied successfully\n");
	}
	else if (strcmp(argv[1], "extractblocks")==0)
	{
		if(!upgrader.unzipper(BLOCKS))             {return 0;}
		printf("Blocks extracted successfully\n");
		if (upgrader.juggler(DATA, true))
		{
			printf("Copied files successfully\n");
		}

	}
	else if (strcmp(argv[1], "path")==0)
	{
		printf("%s\n", GetDataDir().c_str());
	}
	else if (strcmp(argv[1], "gridcoin-qt")==0)
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
	else if (strcmp(argv[1], "gridcoind")==0)
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
	else if (strcmp(argv[1], "snapshot.zip")==0)
	{
		#ifndef WIN32
		int parent = atoi(argv[2]);
		while (0 == kill(parent, 0))
		{
			printf("Parent still kicking\n");
			usleep(1000*800);
		}
		printf("\nParent dead\n");
		if(!upgrader.unzipper(BLOCKS))             {return 0;}
		printf("Blocks extracted successfully\n");
		if (upgrader.juggler(DATA, true))
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

bool Upgrader::downloader(int targetfile)
{
	std::string urlstring = geturl() + targetswitch(targetfile);
	const char *url = urlstring.c_str();

	bfs::path target = path(DATA) / "upgrade";
	if (!verifyPath(target, true)) {return false;}
	target /= targetswitch(targetfile);

	printf("%s\n",url);
	printf("%s\n",target.c_str());

	curlhandle.handle = curl_easy_init();
	curlhandle.done = false;
	curlhandle.success = false;

	if (bfs::exists(target))	{bfs::remove(target);}
	file=fopen(target.c_str(), "wb");
	fileInitialized=true;


	curl_easy_setopt(curlhandle.handle, CURLOPT_URL, url);
	curl_easy_setopt(curlhandle.handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curlhandle.handle, CURLOPT_WRITEDATA, file);
	curl_easy_setopt(curlhandle.handle, CURLOPT_WRITEDATA, file);
	curl_easy_setopt(curlhandle.handle, CURLOPT_XFERINFOFUNCTION, cancelDownloader);
	curl_easy_setopt(curlhandle.handle, CURLOPT_NOPROGRESS, 0L);


	boost::thread(download, (void *)&curlhandle);

	printf("downloading file...\n");

	filesize = -1;
	filesizeRetrieved = false;
	int c = 0;

	while (!curlhandle.done && !CANCEL_DOWNLOAD)
		{
			usleep(800*1000);
			printf("looping\n");
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

	// printf((CANCEL_DOWNLOAD)? "\ndownload interrupted\n" : "\nfile downloaded successfully\n");
	if(!curlhandle.success)
	{
		printf((CANCEL_DOWNLOAD)? "\ndownload interrupted\n" : "\ndownload failed\n");
		if (bfs::exists(target))	bfs::remove(target);
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
	bfs::path target = path(DATA);
	target /= "upgrade";
	if (!verifyPath(target, true)) {return false;}
	
	CkZip zip;
	zip.UnlockComponent("unlockCode");

	if(zip.OpenZip((target / targetswitch(targetfile)).c_str())) 
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
	bfs::remove(target / targetswitch(targetfile));
	return true;
}

bool Upgrader::juggler(int pf, bool recovery)			// for upgrade, backs up target and copies over upgraded file
{														// for recovery, copies over backup
	std::string subdir = (pf == BLOCKS)? "Blockchain " : "Client " + bpt::to_simple_string(bpt::second_clock::local_time());
	bfs::path backupdir(path(DATA) / "backup" / subdir);
	bfs::path sourcedir(path(DATA));
	if (recovery)
		sourcedir = backupdir;
	else sourcedir /= "upgrade";

	if (!verifyPath(sourcedir, true)) {return false;}

	printf("Copying %s into %s\n", path(pf).c_str(), (backupdir / bpt::to_simple_string(bpt::second_clock::local_time())).c_str());

	if ((pf == PROGRAM) && (safeProgramDir()))
	{
		vec iteraton = this->fvector(sourcedir);

		for (vec::const_iterator mongo (iteraton.begin()); mongo != iteraton.end(); ++mongo)
		{

			bfs::path fongo = *mongo;

			if (!bfs::exists(sourcedir / fongo) || bfs::is_directory(sourcedir / fongo))
			{
				continue;
			}

			if (bfs::exists(path(pf) / fongo) && !recovery) // i.e. backup files before upgrading
			{	if (!verifyPath(backupdir, true)) {return false;}
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

		printf("Copying %s into %s\n", sourcedir.c_str(), path(pf).c_str());

		copyDir(sourcedir, path(pf), true);

	return true;
	}
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
	if (!verifyPath(source, false) || !verifyPath(target, true))	{return false;}
	
	vec iteraton = this->fvector(source);
	for (vec::const_iterator mongo (iteraton.begin()); mongo != iteraton.end(); ++mongo)
	{

	bfs::path fongo = *mongo;

	if (!bfs::exists(source / fongo))
	{
		printf("Error, file/directory does not exist\n");
		return false;
	}

	if (bfs::is_directory(source / fongo))
	{
		// printf("%s is a directory - time for recursion!\n", (source / fongo).c_str());
		// if (blacklistDirectory(fongo)) {continue;}
		if ((fongo == "upgrade") || (fongo == "backup") || (fongo == "now") || (!recursive))
		{
			// printf("Skip this shit\n");
			continue;
		}
		copyDir(source / fongo, target / fongo, recursive);
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

vec Upgrader::fvector(bfs::path path)
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
void Upgrader::upgrade(int target)
{
	#ifndef WIN32
	std::stringstream pidstream;
	pidstream << getpid();
	std::string pid = pidstream.str();
	printf("Parent: %s\n", pid.c_str());
	if (bfs::exists(GetProgramDir() / "upgrader"))
	{
	if(!fork())
	{
		printf("Parent: %s\n", pid.c_str());
		if (bfs::exists(bfs::current_path() / "upgrader"))
		{execl((GetProgramDir() / "upgrader").c_str(), "upgrader", targetswitch(target).c_str() , pid.c_str(), NULL);}
		printf("%s does not exist!", (bfs::current_path() / "upgrader").c_str());
		usleep(1000*10000);
	}
	else
	{
		StartShutdown();	
	}
	}
	printf("Could not find upgrader - please verify that this is in the folder gridcoin-qt/gridcoind was called from\n");
	#else
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

std::string Upgrader::targetswitch(int target)
{
	switch(target)
	{
		case BLOCKS:
		return "snapshot.zip";

		case QT:
		return "gridcoin-qt";

		case DAEMON:
		return "gridcoind";
	}
}

void Upgrader::cancelDownload(bool cancel)
{
	CANCEL_DOWNLOAD = cancel;
}