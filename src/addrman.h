// Copyright (c) 2012 Pieter Wuille
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ADDRMAN_H
#define BITCOIN_ADDRMAN_H

#include "netbase.h"
#include "protocol.h"
#include "random.h"
#include "util.h"
#include "sync.h"

#include <map>
#include <memory>
#include <vector>

class AddrInfo;
class AddrManImpl;

//! The maximum number of addresses returned by a getaddr call. Public because
//! the getaddr RPC help text references it (the bucket-sizing constants stay
//! private in addrman_impl.h).
#define ADDRMAN_GETADDR_MAX 2500

//! The maximum percentage of nodes to return in a getaddr call.
#define ADDRMAN_GETADDR_MAX_PCT 23

/**
 * Stochastic address manager (issue #2558 PR 6b).
 *
 * Thin pimpl wrapper: all state and logic live in AddrManImpl (addrman_impl.h
 * + addrman.cpp); this forwards the public interface to it. peers.dat
 * serialization is byte-identical to the pre-split implementation.
 */
class AddrMan
{
public:
    //! Construct an address manager. When \p deterministic is set (tests only),
    //! bucket placement and selection are reproducible.
    explicit AddrMan(bool deterministic = false);
    ~AddrMan();

    template <typename Stream>
    void Serialize(Stream& s) const;

    template <typename Stream>
    void Unserialize(Stream& s);

    //! Return the number of (unique) addresses in all tables.
    size_t size() const;

    //! Add a single address.
    bool Add(const CAddress& addr, const CNetAddr& source, int64_t nTimePenalty = 0);

    //! Add multiple addresses.
    bool Add(const std::vector<CAddress>& vAddr, const CNetAddr& source, int64_t nTimePenalty = 0);

    //! Mark an entry as accessible.
    void Good(const CService& addr, int64_t nTime = GetAdjustedTime());

    //! Mark an entry as connection attempted to.
    void Attempt(const CService& addr, int64_t nTime = GetAdjustedTime());

    //! Choose an address to connect to.
    CAddress Select(bool newOnly = false);

    //! Return a bunch of addresses, selected at random.
    std::vector<CAddress> GetAddr();

    //! Mark an entry as currently-connected-to.
    void Connected(const CService& addr, int64_t nTime = GetAdjustedTime());

    //! Update an entry's service bits.
    void SetServices(const CService& addr, ServiceFlags nServices);

    //! Empty the address tables.
    void Clear();

    //! Test-only hooks, exposed for the unit tests (which include
    //! addrman_impl.h to use the returned AddrInfo). Not used by production code.
    AddrInfo* Find(const CNetAddr& addr, int* pnId = nullptr);
    AddrInfo* Create(const CAddress& addr, const CNetAddr& addrSource, int* pnId = nullptr);
    void Delete(int nId);

private:
    const std::unique_ptr<AddrManImpl> m_impl;
};

#endif // BITCOIN_ADDRMAN_H
