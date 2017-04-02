#pragma once

#include "uint256.h"

#include <boost/flyweight/flyweight_fwd.hpp>
#include <boost/flyweight/no_tracking_fwd.hpp>
#include <string>
#include <deque>

// An untracked, flyweight string.
typedef boost::flyweight<std::string, boost::flyweights::no_tracking> flyweight_string;

// Typedeffed CPID string to make the intention obvious when used in containers.
typedef flyweight_string cpid_string;

typedef std::deque<uint256> HashCollection;
