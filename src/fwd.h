#pragma once

#include <memory>

// Block data structures.
class CBlock;
class CBlockIndex;
class CNetAddr;
class CTransaction;
class CWallet;

class ThreadHandler;
typedef std::shared_ptr<ThreadHandler> ThreadHandlerPtr;

class CKey;
