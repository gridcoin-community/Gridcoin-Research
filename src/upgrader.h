#ifndef GRC_UPGRADER
#define GRC_UPGRADER

#include <curl/curl.h> // for downloading
#include <string>
#include <string.h>
#include <boost/filesystem.hpp>
#include "qt/upgradedialog.h"

// CONVERT INDENTATION TO SPACES!!!

struct curlargs
{ 
	CURL *handle;
	bool done;
};

enum TARGET
{
	DATA,
	PROGRAM,
};

class Upgrader
{

private:

	struct curlargs curlhandle;
	FILE *file;
	double filesize;
	bool filesizeRetrieved;
	std::vector<boost::filesystem::path> fvector(int pathfork, bool recovery);
	// //launched in a separate thread that performs the downloading:
	// // data writing function for curl:
	// size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream); 
	// // arguments for curl:
	// struct curlargs;
	
public:

	bool downloader(std::string target, int pathfork, std::string source, UpgradeDialog *kuppak);
	bool unzipper(std::string target, int pathfork);

//	switches around upgrade, target and backup:
	bool juggler(int pathfork, bool recovery);

// 	return info about file being downloaded:
	int getFileDone();
	int getFilePerc(int sz);
};

#endif