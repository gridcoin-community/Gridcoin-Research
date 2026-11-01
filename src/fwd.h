#ifndef BITCOIN_FWD_H
#define BITCOIN_FWD_H

#include <memory>

// Block data structures.
class CBlock;
class CBlockIndex;
class CNetAddr;
class CTransaction;
class CWallet;

// Transaction reference types
typedef std::shared_ptr<const CTransaction> CTransactionRef;

class ThreadHandler;
typedef std::shared_ptr<ThreadHandler> ThreadHandlerPtr;

class CKey;

#endif // BITCOIN_FWD_H
