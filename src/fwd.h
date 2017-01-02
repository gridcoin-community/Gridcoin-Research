#pragma once

#include <boost/flyweight/flyweight_fwd.hpp>
#include <boost/flyweight/no_tracking_fwd.hpp>
#include <string>

// An untracked, flyweight string.
typedef boost::flyweight<std::string, boost::flyweights::no_tracking> flyweight_string;
