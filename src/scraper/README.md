## Gridcoin Scraper

This scraper is designed to communicate with the wallet and gather whitelist data.  A wallet should be running locally with the scraper so if a whitelist entry is added or removed it can have up to date information.

This scraper runs a loop and sleeps for 300 seconds before performing another loop.

This scraper uses a userpass.dat file which contains credentials for Einstein, though it can contain future credentials for other projects as well.

This scraper will not perform any tasks when a superblock is not needed with the exception that it will perform its project file sync up to 300 seconds before a superblock needed.

This scraper keeps track of download bandwidth usage as well as the new upload size that neural network clients would be downloading.


How this works:

1) Request whitelist data from wallet
2) HEAD request each project server for ETag of current team or RAC file
3a) If ETag file already exists skip this project team or RAC file based on which it is HEAD requesting for
3b) If does not exist goto 4
4) Download new team or RAC file
5) If file size matches the file size reported in HEAD request we continue; else skip for now
5) Process the team or RAC file and make a gzip file stored as the ETag with only required data we need for neural network consensus. (reduces file sizes by ~99%)
6) Sleep for 300 seconds

This scraper has a pretty verbal log file and time file as well. All aspects of curl usage and RPC as well as stats processing have logging.

This scraper is actually quite fast with what it does.

CURRENT g++ compile flags:

-std=c++11 -lcurl -lboost_system -DBOOST_NO_CXX11_SCOPED_ENUMS -lboost_filesystem -lboost_iostreams -lz -ljsoncpp

NOTE: -g for testing and the valgrinding I've done.

OTHER:

As I've mentioned the shrinking of the files by removing unused data we dont use and also making us more if not fully compliant with stats data.

1535491750 [INFO] <MAIN> : download size so far: 302543285 upload size so far: 1666554

As seen here it has downloaded 302,543,285 in gzip files and produced 1,666,554 in gzip files for download by neural network. That is a 99.4% decrease in file size which also results in a faster neural network sync.
