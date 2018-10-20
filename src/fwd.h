#pragma once

#include <memory>

// Block data structures.
class CBlock;
class CBlockIndex;
class CNetAddr;
class CTransaction;
class CWallet;

// Gridcoin
struct MiningCPID;
struct StructCPID;

class ThreadHandler;
typedef std::shared_ptr<ThreadHandler> ThreadHandlerPtr;

class CKey;
