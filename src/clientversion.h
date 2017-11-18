#ifndef CLIENTVERSION_H
#define CLIENTVERSION_H

//
// client versioning
//

#if defined(HAVE_CONFIG_H)
#include "config/gridcoin-config.h"
#else

//! Copyright string used in Windows .rc files
#define COPYRIGHT_STR "2009-"

// These need to be macros, as version.cpp's and bitcoin-qt.rc's voodoo requires it
#define CLIENT_VERSION_MAJOR       3
#define CLIENT_VERSION_MINOR       6
#define CLIENT_VERSION_REVISION    3
#define CLIENT_VERSION_BUILD       17

#define CLIENT_VERSION_IS_RELEASE false
#define COPYRIGHT_YEAR 2017

#endif //HAVE_CONFIG_H

// Converts the parameter X to a string after macro replacement on X has been performed.
// Don't merge these into one macro!
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X

#define COPYRIGHT_STR "2009-" STRINGIZE(COPYRIGHT_YEAR) " The Bitcoin Core Developers"

#if !defined(WINDRES_PREPROC)

#include <string>
#include <vector>

static const int CLIENT_VERSION =
                           1000000 * CLIENT_VERSION_MAJOR
                         +   10000 * CLIENT_VERSION_MINOR
                         +     100 * CLIENT_VERSION_REVISION
                         +       1 * CLIENT_VERSION_BUILD;

extern const std::string CLIENT_NAME;
extern const std::string CLIENT_BUILD;
extern const std::string CLIENT_DATE;


std::string FormatFullVersion();
std::string FormatSubVersion(const std::string& name, int nClientVersion, const std::vector<std::string>& comments);

#endif // WINDRES_PREPROC


#endif // CLIENTVERSION_H
 
