// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_CHAIN_SETUP_H
#define GRIDCOIN_TEST_CHAIN_SETUP_H

#include "amount.h"
#include "key.h"
#include "keystore.h"
#include "primitives/transaction.h"

#include <string>
#include <vector>

class CBlock;

namespace grc_test {

//!
//! \brief A real regtest chain, with spendable coins, inside the unit-test binary.
//!
//! The global TestingSetup (test_gridcoin.cpp) already supplies ECC, a mocked
//! Berkeley DB wallet and an in-memory LevelDB behind the process-global txdb
//! handle. What it does not supply is a chain: mapBlockIndex is empty, there is
//! no genesis, no block file, and no transaction index. Anything that resolves
//! an input through CTxDB is therefore untestable, which is why the mempool
//! selection loop in CreateRestOfTheBlock has never executed an iteration under
//! a unit test (see issue #3290).
//!
//! This fixture selects regtest, creates the datadir, and calls the production
//! LoadBlockIndex(), which writes the deterministic regtest genesis to a real
//! blk0001.dat and its coinbase to the transaction index. That coinbase pays ten
//! spendable outputs to the premine key, and regtest skips ConnectInputs'
//! coinbase maturity walk (validation.cpp), so those outputs are spendable at
//! height 1 with no mining at all.
//!
//! LIFETIME. Use it as a SUITE-level fixture:
//!
//!     BOOST_AUTO_TEST_SUITE(my_tests, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())
//!
//! and NOT with BOOST_FIXTURE_TEST_SUITE, which constructs a fixture once per
//! test case. A per-case rebuild does not work here: the genesis block index,
//! hashBestChain and the transaction index all persist in the LevelDB memenv,
//! which is opened once for the whole binary and must never be closed (doing so
//! destroys the global fixture's handle), so a second LoadBlockIndex() takes the
//! disk-reload path instead of creating genesis. Constructing a second one while
//! the first is alive asserts.
//!
//! Because it is not a base class under that decorator, the helpers below are
//! free functions. They are valid only while a RegtestChainSetup is alive.
//!
//! GLOBAL STATE. Construction flips the whole process to regtest and mutates
//! consensus globals shared with every other suite in this binary. Every
//! restore lives in an RAII sub-object constructed before the mutating work, so
//! a throw out of the constructor still unwinds it -- no destructor runs for a
//! partially constructed object. One residue is irreversible and accepted: the
//! genesis block-index record, hashBestChain and the tx-index entries stay in
//! the memenv LevelDB. They are inert for other suites (nothing else calls
//! LoadBlockIndex, and lookups are by hash), which is precisely why the lifetime
//! is once per suite.
//!
struct RegtestChainSetup
{
    RegtestChainSetup();
    ~RegtestChainSetup();

    RegtestChainSetup(const RegtestChainSetup&) = delete;
    RegtestChainSetup& operator=(const RegtestChainSetup&) = delete;
};

//! \brief The key the regtest genesis premine pays to.
const CKey& PremineKey();

//! \brief A keystore holding PremineKey(), for SignSignature().
const CBasicKeyStore& PremineKeystore();

//! \brief The regtest genesis coinbase. Its outputs are the fixture's coins.
const CTransaction& PremineCoinbase();

//! \brief Premine outputs the transaction index still records as unspent.
std::vector<COutPoint> SpendablePremineOutputs();

//! \brief A P2PKH scriptPubKey paying the premine key.
CScript PremineScript();

//! \brief Timestamp shared by every fixture-built transaction.
//!
//! Fixed rather than wall-clock so tests are reproducible. It is after the
//! genesis timestamp, which ConnectInputs requires (txPrev.nTime <= tx.nTime).
int64_t FixtureTxTime();

//!
//! \brief Sign a spend of \p txFrom output \p n, paying \p fee.
//!
//! \p n_outputs is the size knob: a 1-in/N-out P2PKH transaction is roughly
//! 163 + 34N bytes, which is how a test varies fee RATE independently of
//! absolute fee. The signature is real -- ConnectInputs verifies it on the
//! miner path.
//!
//!
//! \p tx_time overrides the transaction timestamp; 0 means FixtureTxTime().
//! ConnectInputs rejects a transaction older than the input it spends, so a
//! value below the genesis timestamp will not connect.
//!
CTransaction CreateSpend(const CTransaction& txFrom, uint32_t n, CAmount fee, int n_outputs = 1,
                         int64_t tx_time = 0);

//! \brief As CreateSpend, but every output carries \p script_pub_key.
CTransaction CreateSpendToScript(const CTransaction& txFrom, uint32_t n, CAmount fee,
                                 const CScript& script_pub_key, int n_outputs = 1,
                                 int64_t tx_time = 0);

//!
//! \brief A signed transaction that satisfies CTransaction::IsCoinStake().
//!
//! Empty vout[0], a real signed input, and vout[1] paying the remainder, so the
//! transaction carries a POSITIVE fee. That matters: a realistic minting
//! coinstake pays a negative fee and is excluded by the nTxFees < nMinFee
//! handler whether or not the loop's IsCoinStake() guard is there, so it would
//! prove nothing about the guard.
//!
CTransaction CreateCoinstakeShaped(const CTransaction& txFrom, uint32_t n, CAmount fee);

//! \brief Put \p tx in the global mempool with \p fee, bypassing all validation.
void AddToMempool(const CTransaction& tx, CAmount fee);

//!
//! \brief Fill in CreateRestOfTheBlock's preconditions.
//!
//! Sets a block version that reaches the nBlockSize reserve arithmetic, sizes
//! block.vtx and the coinstake outputs (the prologue indexes vout[1]), and puts
//! block.nTime after FixtureTxTime() so the timestamp guard does not silently
//! drop every candidate.
//!
void MakeBlockAndCoinstake(CBlock& block, CMutableTransaction& coinbase,
                           CMutableTransaction& coinstake);

//!
//! \brief Mine one real regtest block, retrying across stake time slots.
//!
//! Mirrors the retry loop in generatetoaddress (rpc/mining.cpp): the kernel is
//! evaluated at a single 16-second-masked timestamp, so each attempt steps to a
//! fresh slot. Rescans the wallet afterwards -- RegisterValidationInterface is a
//! silent no-op in this binary, so the wallet has no other way to learn about the
//! block. Returns false and sets \p err on failure.
//!
//! Only tests that genuinely need a mined output (a confirmed P2SH funding
//! output, say) should call this; the premine alone needs no mining.
//!
bool CreateAndProcessBlock(CBlock& block_out, std::string& err);

} // namespace grc_test

#endif // GRIDCOIN_TEST_CHAIN_SETUP_H
