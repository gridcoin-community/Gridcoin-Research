#ifndef GLOBAL_OBJECTS_NOUI_HPP
#define GLOBAL_OBJECTS_NOUI_HPP

#include "fwd.h"
#include <string>
#include <map>
#include <set>

extern int nBoincUtilization;
extern std::string sRegVer;
extern bool bForceUpdate;
extern bool fQtActive;
extern bool bGridcoinCoreInitComplete;

// Timers
extern std::map<std::string, int> mvTimers; // Contains event timers that reset after max ms duration iterator is exceeded

#endif /* GLOBAL_OBJECTS_NOUI_HPP */
