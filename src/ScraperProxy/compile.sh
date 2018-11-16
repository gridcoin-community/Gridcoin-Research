#!/bin/bash
g++ -o gridcoinscraper scraper-standalone.cpp scraper.cpp ../appcache.cpp -std=c++11 -I.. -lcurl -lboost_system -DBOOST_NO_CXX11_SCOPED_ENUMS -DSCRAPER_STANDALONE -lboost_filesystem -lboost_iostreams -lz -ljsoncpp
