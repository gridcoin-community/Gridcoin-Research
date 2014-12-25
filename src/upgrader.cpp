#include <stdio.h>      // for input output to terminal
#include <pthread.h>    // for threading
#include <unistd.h>     // for sleep
#include <upgrader.h>
#include <CkZip.h>
#include <CkZipEntry.h>

namespace bfs = boost::filesystem;
typedef std::vector<bfs::path> vec;

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

std::string geturl()
{
	std::string url = "http://download.gridcoin.us/download/";
	return url;
}

bfs::path getdatadir()
{
	bfs::path datadir = "/home/lederstrumpf/c++_playfield/GRCRestarter/data/";        // Naturally, this will be OS-dependent later
	return datadir;
}

bfs::path getprogramdir()
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
			path = getdatadir();
			break;

		case PROGRAM:
			path = getprogramdir();
			break;

		default:
			printf("Path not specified!\n");
	}

	return path;
}

void *download(void *curlhandle)
	{
	struct curlargs *curlarg = (struct curlargs *)curlhandle;
	curl_easy_perform(curlarg->handle);
	curlarg->done=true;
	}

#if !defined(QT_GUI)
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
		std::string source = "signed/gridcoin-qt";
		if(!upgrader.downloader(target, PROGRAM, source)) {return 0;}
	}
	else if (strcmp(argv[1], "downloadblocks")==0)
	{
		std::string target = "snapshot.zip";
		std::string source = "signed/snapshot.zip";
		if(!upgrader.downloader(target, DATA, source)) {return 0;}
		if(!upgrader.unzipper(target, DATA))             {return 0;}
		printf("Blocks downloaded and extracted successfully\n");
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
			printf("Copied file successfully\n");
		}
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
	target /= targetfile;

	printf("%s\n",url);
	printf("%s\n",target.c_str());

	pthread_t downloadThread; // downloading thread

	curlhandle.handle = curl_easy_init();
	curlhandle.done=0;

	file=fopen(target.c_str(), "wb");
	fileInitialized=true;

	curl_easy_setopt(curlhandle.handle, CURLOPT_URL, url);
	curl_easy_setopt(curlhandle.handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curlhandle.handle, CURLOPT_WRITEDATA, file);

	pthread_create(&downloadThread, NULL, download, (void *)&curlhandle);

	printf("downloading file...\n");

	filesize = -1;
	filesizeRetrieved = false;

	while (!curlhandle.done)
		{
			usleep(800*1000);
			int sz = getFileDone();
			printf("\r%li\tKB", sz/1024);
			printf("\t%li\%", getFilePerc(sz));
			fflush( stdout );
		}

	curl_easy_cleanup(curlhandle.handle);
	fclose(file);
	fileInitialized=false;

	printf("\nfile downloaded successfully\n");
	return true;
}

long int Upgrader::getFileDone()
{
	if (fileInitialized)
	{
		fseek(file, 0L, SEEK_END);
		return ftell(file);
	}

	return 0;
}

long int Upgrader::getFilePerc(long int sz)
{
	if(!filesizeRetrieved)
			{
				curl_easy_getinfo(curlhandle.handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &filesize);
				if(filesize>1) 
					{
						filesizeRetrieved=true;
						return 0;
					}
			}
	return sz*100/(long int)filesize;
}

bool Upgrader::unzipper(std::string targetfile, int pf)
{
	bfs::path target = path(pf);
	target /= "upgrade";
	target /= targetfile;

	CkZip zip;
	zip.UnlockComponent("unlockCode");

	if(zip.OpenZip(target.c_str())) 
	{
		printf("Extracting archive into %s\n", path(pf).c_str());
		if(!zip.Extract(path(pf).c_str()))
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
	return true;
}

bool Upgrader::juggler(int pf, bool recovery)			// for upgrade, backs up target and copies over upgraded file
{														// for recovery, copies over backup
	bfs::path backupdir(path(pf) / "backup");
	bfs::path sourcedir(path(pf));
	if (recovery)
		sourcedir /= "backup";
	else sourcedir /= "upgrade";

	vec iteraton = this->fvector(pf, recovery);

	for (vec::const_iterator mongo (iteraton.begin()); mongo != iteraton.end(); ++mongo)
	{

	bfs::path fongo = *mongo;

	if (!bfs::exists(sourcedir / fongo))
	{
		printf("Error, updated/recovery file does not exist\n");
		return false;
	}

	if (bfs::exists(path(pf) / fongo) && !recovery) // i.e. backup files before upgrading
		bfs::copy_file(path(pf) / fongo, backupdir / fongo, bfs::copy_option::overwrite_if_exists);

	if (bfs::exists(path(pf) / fongo)) // avoid overwriting
		bfs::remove(path(pf) / fongo);

	bfs::copy_file(sourcedir / fongo, path(pf) / fongo); // the actual upgrade/recovery

	printf("copied\n");
	
	}

	return true;
}

std::vector<bfs::path> Upgrader::fvector(int pf, bool recovery)
{
	bfs::path sourcedir(path(pf));
	if (recovery)
		sourcedir /= "backup/";
	else sourcedir /= "upgrade/";

	vec a;
 
	copy(bfs::directory_iterator(sourcedir), bfs::directory_iterator(), back_inserter(a));

	for (unsigned int i = 0; i < a.size(); ++i)
	{
		a[i]=a[i].filename();
	}

	return a;
}
