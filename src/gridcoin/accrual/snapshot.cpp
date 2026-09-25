// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/accrual/snapshot.h"

#include "consensus/consensus.h"
#include "gridcoin/beacon.h"
#include "node/blockstorage.h"
#include "tinyformat.h"
#include "util/strencodings.h"
#include "util/system.h"

#include <algorithm>

namespace GRC {

using LogFlags = BCLog::LogFlags;

fs::path SnapshotDirectory()
{
    return GetDataDir() / "accrual";
}

fs::path SnapshotPath(const uint64_t height)
{
    return SnapshotDirectory() / strprintf("%" PRIu64 ".dat", height);
}

CAmount SnapshotAccrualComputer::MaxReward() const
{
    // The maximum accrual that a CPID can claim in one block is limited to
    // the amount of accrual that a CPID can collect over two days when the
    // CPID achieves the maximum magnitude value supported in a superblock.
    //
    // Where... (for block versions below V13)
    //
    //   max_magnitude = 32767
    //   magnitude_unit = 0.25
    //
    // ...then...
    //
    //   max_magnitude * magnitude_unit * 2 = max_accrual = 16383.5
    //
    // ...rounded-up to 16384.
    //
    // For V13+, the magnitude unit can be set by protocol entry.
    return (GetMagnitudeUnit() * 32768 * 2 * COIN).ToCAmount();
}

double SnapshotAccrualComputer::MagnitudeUnit() const
{
    // Superblock-based accrual calculations do not rely on the rolling
    // two-week network payment average. Instead, we calculate research
    // rewards using the magnitude unit that represents the equilibrium
    // quantity of the formula used to determine the magnitude unit for
    // the legacy research age accrual calculations.
    //
    // Where (prior to block v13) ...
    //
    //   blocks_per_day = 960
    //   grc_per_block = 50
    //   total_magnitude = 115000
    //
    //   max_daily_emission = blocks_per_day * grc_per_block
    //   daily_emission = (5.0 / 9) * max_daily_emission;
    //
    // ...then...
    //
    //   daily_emission / total_magnitude = magnitude_unit = 0.23188405...
    //
    // ...rounded-up to 0.25:
    //
    // V13+, the magnitude unit can be set by protocol entry.
    return GetMagnitudeUnit().ToDouble();
}

int64_t SnapshotAccrualComputer::AccrualAge() const
{
    // For the CPIDs that never staked a block, report the accrual age as
    // the time since the CPID advertised a beacon. This is not perfectly
    // accurate since newbie accrual begins when a new CPID first appears
    // in a superblock, but we don't store or look-up that superblock for
    // performance. The accrual age requested here is informational since
    // the SnapshotCalculator performs the real accrual calculation.
    //
    // TODO: Update this to base age on timestamp when beacon verifies in
    // a superblock after contract improvements for more accurate age.
    //
    if (m_account.IsNew()) {
        if (const BeaconOption beacon = GetBeaconRegistry().Try(m_cpid)) {
            const int64_t beacon_time = beacon->m_timestamp;

            if (beacon_time > 0) {
                return m_payment_time - beacon_time;
            }
        }

        return 0;
    }

    return SnapshotCalculator::AccrualAge(m_account);
}

double SnapshotAccrualComputer::AccrualDays() const
{
    // Since this informational value is not consensus-critical, we use
    // floating-point arithmetic for readability:
    //
    return AccrualAge() / 86400.0;
}

int64_t SnapshotAccrualComputer::AccrualBlockSpan() const
{
    // TODO: we can use the height of a beacon verification in a superblock
    // to report accurate block spans after contract improvements. For now,
    // this is informational, so we just report zero for newbies:
    //
    if (m_account.IsNew()) {
        return 0;
    }

    return m_last_height - m_account.LastRewardHeight();
}

CAmount SnapshotAccrualComputer::PaymentPerDay() const
{
    if (m_account.IsNew()) {
        return 0;
    }

    const int64_t elapsed = m_payment_time - m_account.FirstRewardTime();
    const double lifetime_days = elapsed / 86400.0;

    if (lifetime_days <= 0) {
        return 0;
    }

    return m_account.m_total_research_subsidy / lifetime_days;
}

CAmount SnapshotAccrualComputer::PaymentPerDayLimit() const
{
    return MaxReward();
}

CAmount SnapshotAccrualComputer::NearRewardLimit() const
{
    // This returns MaxReward() - 2 * ExpectedDaily() or 1/2 of MaxReward(), whichever
    // is greater

    CAmount threshold = std::max(MaxReward() / 2, MaxReward() - 2 * ExpectedDaily());

    return threshold;
}

bool SnapshotAccrualComputer::ExceededRecentPayments() const
{
    return RawAccrual() > PaymentPerDayLimit();
}

CAmount SnapshotAccrualComputer::ExpectedDaily() const
{
    // Since this informational value is not consensus-critical, we use
    // floating-point arithmetic for readability:
    //
    return CurrentMagnitude(m_cpid).Floating() * MagnitudeUnit() * COIN;
}

CAmount SnapshotAccrualComputer::RawAccrual() const
{
    if (m_account.LastRewardHeight() >= m_superblock.m_height) {
        return AccrualDelta(m_cpid, m_account);
    }

    LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: CPID = %s, m_account.m_accrual = %" PRId64 ", ",
             __func__, m_cpid.ToString(), m_account.m_accrual);

    return m_account.m_accrual + AccrualDelta(m_cpid, m_account);
}

CAmount SnapshotAccrualComputer::Accrual() const
{
    const CAmount accrual = RawAccrual();

    if (accrual > MaxReward()) {
        return MaxReward();
    }

    return accrual;
}

AccrualSnapshot::AccrualSnapshot()
    : m_version(CURRENT_VERSION)
    , m_height(0)
{
}

AccrualSnapshot::AccrualSnapshot(deserialize_type, CAutoHasherFile& file)
{
    m_records.clear();

    file >> m_version;
    file >> m_height;

    while (true) {
        Cpid cpid;
        int64_t accrual;

        try {
            file >> cpid;
            file >> accrual;
        } catch (const std::ios_base::failure& e) {
            if (feof(file.Get())) {
                break;
            }

            throw;
        }

        if (!(file.GetType() & SER_GETHASH)) {
            m_records.emplace(cpid, accrual);
        }
    }
}

CAmount AccrualSnapshot::GetAccrual(const Cpid cpid) const
{
    auto iter = m_records.find(cpid);

    if (iter == m_records.end()) {
        return 0;
    }

    return iter->second;
}

AccrualSnapshotFile::AccrualSnapshotFile(FILE* file, const int ser_type)
    : m_file(file, ser_type, AccrualSnapshot::CURRENT_VERSION)
{
}

AccrualSnapshotFile::AccrualSnapshotFile(FILE* file)
    : AccrualSnapshotFile(file, SER_DISK)
{
}

uint64_t AccrualSnapshotFile::ParseHeight(const fs::path& snapshot_path)
{
    uint64_t height = 0;

    if (!ParseUInt64(snapshot_path.stem().string(), &height)) {
        LogPrint(BCLog::LogFlags::SB, "WARN: %s: Filename in snapshot path does not contain a valid height number.",
                 __func__);
    }

    return height;
}

void AccrualSnapshotFile::Remove(const fs::path& snapshot_path)
{
    try {
        fs::remove(snapshot_path);
    } catch (const std::exception& e) {
        // Failing to remove the snapshot file is not a critical error as
        // long as we can remove it from the registry.
        //
        LogPrintf("WARNING: %s: %s", __func__, e.what());
    }
}

void AccrualSnapshotFile::Remove(const uint64_t height)
{
    Remove(SnapshotPath(height));
}

bool AccrualSnapshotFile::IsNull() const
{
    return m_file.IsNull();
}

uint256 AccrualSnapshotFile::GetHash()
{
    return m_file.GetHash();
}

AccrualSnapshotReader::AccrualSnapshotReader(const fs::path& snapshot_path, const int ser_type)
    : AccrualSnapshotFile(fsbridge::fopen(snapshot_path, "rb"), ser_type)
{
}

AccrualSnapshotReader::AccrualSnapshotReader(const fs::path& snapshot_path)
    : AccrualSnapshotReader(snapshot_path, SER_DISK)
{
}

uint256 AccrualSnapshotReader::Hash(const fs::path& snapshot_path)
{
    AccrualSnapshotReader reader(snapshot_path, SER_GETHASH);

    if (reader.IsNull()) {
        return uint256();
    }

    try {
        reader.Read();
    } catch (const std::exception& e) {
        error("%s: %s", __func__, e.what());
        return uint256();
    }

    return reader.GetHash();
}

AccrualSnapshot AccrualSnapshotReader::Read()
{
    return AccrualSnapshot(deserialize, m_file);
}

AccrualSnapshotWriter::AccrualSnapshotWriter(const fs::path& snapshot_path)
    : AccrualSnapshotFile(fsbridge::fopen(snapshot_path, "wb"))
{
}

void AccrualSnapshotWriter::WriteHeader(const uint64_t height)
{
    m_file << AccrualSnapshot::CURRENT_VERSION;
    m_file << height;
}

void AccrualSnapshotWriter::WriteRecord(const Cpid cpid, const int64_t accrual)
{
    m_file << cpid << accrual;
}

bool AccrualSnapshotRegistry::Initialize()
{
    LogPrintf("Initializing accrual snapshot registry...");

    if (!Close()) {
        return false;
    }

    CAutoFile registry_file(
        fsbridge::fopen(RegistryPath(), "rb"),
        SER_DISK,
        CURRENT_VERSION);

    if (!registry_file.IsNull()) {
        try {
            Unserialize(registry_file);
        } catch (const std::ios_base::failure& e) {
            if (feof(registry_file.Get())) {
                throw SnapshotStateError("unexpected eof while loading the registry.");
            }

            throw;
        }
    } else {
        m_entries.clear();
    }

    LogPrintf("Accrual snapshot registry loaded. Compacting...");

    return Rewrite();
}

bool AccrualSnapshotRegistry::Close()
{
    if (m_file && fclose(m_file) != 0) {
        return error("%s: failed to close snapshot registry", __func__);
    }

    m_file = nullptr;

    return true;
}

bool AccrualSnapshotRegistry::ResetBaseline(const uint64_t height)
{
    if (!WriteEntry(Action::BASELINE, Entry(height, uint256()))) {
        return error("%s: failed to record baseline snapshot", __func__);
    }

    LogPrint(LogFlags::TALLY,
        "Tally: reset new accrual snapshot baseline: %" PRIu64, height);

    m_entries.clear();

    return true;
}

uint64_t AccrualSnapshotRegistry::BaselineHeight() const
{
    if (!m_entries.empty()) {
        return m_entries.front().m_height;
    }

    return 0;
}

uint64_t AccrualSnapshotRegistry::LatestHeight() const
{
    if (!m_entries.empty()) {
        return m_entries.back().m_height;
    }

    return 0;
}

const AccrualSnapshotRegistry::Entry* AccrualSnapshotRegistry::TryHeight(const uint64_t height) const
{
    const auto iter = std::lower_bound(m_entries.begin(), m_entries.end(), height);

    if (iter == m_entries.end() || iter->m_height != height) {
        return nullptr;
    }

    return &*iter;
}

void AccrualSnapshotRegistry::AssertHashMatches(const uint64_t height, const uint256 hash) const
{
    if (const Entry* entry = TryHeight(height)) {
        entry->AssertHash(hash);
    }
}

bool AccrualSnapshotRegistry::Register(const uint64_t height, const uint256 snapshot_hash)
{
    assert(m_entries.empty() || height > m_entries.back().m_height); // LINT-OK-ASSERT: relocated unchanged

    const Entry entry(height, snapshot_hash);

    if (!WriteEntry(Action::REGISTER, entry)) {
        return error("%s: failed to add %" PRIu64, __func__, height);
    }

    LogPrint(LogFlags::TALLY,
        "Tally: recorded new accrual snapshot %" PRIu64, height);

    m_entries.emplace_back(entry);

    return true;
}

bool AccrualSnapshotRegistry::Deregister(const uint64_t height)
{
    assert(!m_entries.empty() && height == m_entries.back().m_height); // LINT-OK-ASSERT: relocated unchanged

    const Entry* entry = TryHeight(height);

    if (!entry) {
        return true;
    }

    if (!WriteEntry(Action::DEREGISTER, *entry)) {
        return error("%s: failed to remove %" PRIu64, __func__, height);
    }

    LogPrint(LogFlags::TALLY,
        "Tally: recorded accrual snapshot removal %" PRIu64, height);

    m_entries.pop_back();

    return true;
}

void AccrualSnapshotRegistry::write(Span<const std::byte> src)
{
    if (!m_file) {
        throw std::ios_base::failure(
            strprintf("%s: file handle is nullptr", __func__));
    }

    if (fwrite(src.data(), 1, src.size(), m_file) != src.size()) {
        throw std::ios_base::failure(
            strprintf("%s: write failed", __func__));
    }
}

fs::path AccrualSnapshotRegistry::RegistryPath()
{
    return SnapshotDirectory() / "registry.dat";
}

bool AccrualSnapshotRegistry::ReopenForWrite()
{
    if (!Close()) {
        return false;
    }

    m_file = fsbridge::fopen(RegistryPath(), "wb");

    return m_file || error(
        "%s: failed to open snapshot registry for writing: %s",
        __func__,
        RegistryPath().string());
}

bool AccrualSnapshotRegistry::Rewrite()
{
    if (!ReopenForWrite()) {
        return false;
    }

    try {
        ::Serialize(*this, CURRENT_VERSION);
    } catch (const std::exception& e) {
        return error("%s: %s", __func__, e.what());
    }

    if (m_entries.empty()) {
        return true;
    }

    if (!WriteEntry(Action::BASELINE, m_entries.front())) {
        return false;
    }

    for (const auto& entry : m_entries) {
        if (!WriteEntry(Action::REGISTER, entry)) {
            return false;
        }
    }

    return true;
}

bool AccrualSnapshotRegistry::WriteEntry(const Action action, const Entry entry)
{
    try {
        ::Serialize(*this, DiskEntry(action, entry));
    } catch (const std::exception& e) {
        return error("%s: %s", __func__, e.what());
    }

    return fflush(m_file) == 0;
}

void AccrualSnapshotRegistry::Unserialize(CAutoFile& file)
{
    m_entries.clear();

    uint32_t version;
    file >> version;

    if (version != CURRENT_VERSION) {
        // When this is executed by the tally's initialization routine, the
        // exception will cause the application to rebuild the snapshots:
        throw SnapshotRegistryVersionMismatchError(version, CURRENT_VERSION);
    }

    while (true) {
        try {
            const DiskEntry entry(deserialize, file);

            switch (entry.m_action) {
                case Action::BASELINE:
                    LogPrint(LogFlags::ACCRUAL,
                        "  Baseline: %" PRIu64, entry.m_height);

                    m_entries.clear();
                    break;
                case Action::REGISTER:
                    LogPrint(LogFlags::ACCRUAL,
                        "  Added: %" PRIu64 " (%s)",
                        entry.m_height,
                        entry.m_snapshot_hash.ToString());

                    m_entries.emplace_back(entry);
                    break;
                case Action::DEREGISTER:
                    LogPrint(LogFlags::ACCRUAL,
                        "  Removed: %" PRIu64 " (%s)",
                        entry.m_height,
                        entry.m_snapshot_hash.ToString());

                    m_entries.pop_back();
            }
        } catch (const std::ios_base::failure& e) {
            if (feof(file.Get())) {
                break;
            }

            throw;
        }
    }
}

bool AccrualSnapshotRepository::Initialize()
{
    try {
        fs::create_directory(SnapshotDirectory());
    } catch (const std::exception& e) {
        return error(
            "%s: failed to create the accrual snapshot directory %s: %s",
            __func__,
            SnapshotDirectory().string(),
            e.what());
    }

    return m_registry.Initialize();
}

bool AccrualSnapshotRepository::EraseAll()
{
    if (!m_registry.Close()) {
        return false;
    }

    try {
        fs::remove_all(SnapshotDirectory());
    } catch (const std::exception& e) {
        return error(
            "%s: failed to erase all accrual snapshots in %s: %s",
            __func__,
            SnapshotDirectory().string(),
            e.what());
    }

    return Initialize();
}

void AccrualSnapshotRepository::AssertMatch(const uint64_t height) const
{
    if (const auto* entry = m_registry.TryHeight(height)) {
        entry->AssertHash(AccrualSnapshotReader::Hash(SnapshotPath(height)));
    } else {
        throw SnapshotMissingError(height);
    }
}

void AccrualSnapshotRepository::PruneSnapshotFiles() const
{
    for (const auto& file : fs::directory_iterator(SnapshotDirectory())) {
        const fs::path& file_path = file.path();

        if (file_path.filename() == "registry.dat") {
            continue;
        }

        if (const uint64_t height = AccrualSnapshotFile::ParseHeight(file_path)) {
            if (m_registry.TryHeight(height)) {
                continue;
            }
        }

        LogPrint(LogFlags::TALLY,
            "%s: removing extraneous accrual snapshot file %s",
            __func__,
            file_path.filename().string());

        AccrualSnapshotFile::Remove(file_path);
    }
}

bool AccrualSnapshotRepository::HasBaseline() const
{
    return m_registry.BaselineHeight() > 0;
}

bool AccrualSnapshotRepository::StoreBaseline(const uint64_t height, const ResearchAccountMap& accounts)
{
    return m_registry.ResetBaseline(height) && Store(height, accounts);
}

bool AccrualSnapshotRepository::Store(const uint64_t height, const ResearchAccountMap& accounts)
{
    LogPrint(LogFlags::TALLY,
        "Tally: storing new accrual snapshot %" PRIu64 "...", height);

    AccrualSnapshotWriter writer(SnapshotPath(height));

    if (writer.IsNull()) {
        return error("%s: failed to open %" PRIu64, __func__, height);
    }

    try {
        writer.WriteHeader(height);

        for (const auto& account_pair : accounts) {
            if (account_pair.second.m_accrual > 0) {
                writer.WriteRecord(
                    account_pair.first, // CPID
                    account_pair.second.m_accrual);
            }
        }
    } catch (const std::exception& e) {
        return error("%s: %s", __func__, e.what());
    }

    return m_registry.Register(height, writer.GetHash());
}

bool AccrualSnapshotRepository::ApplyLatest(ResearchAccountMap& accounts) const
{
    // No snapshot left to apply. That is the state of a fresh chain whose
    // first superblock was just disconnected: nothing was ever stored
    // before it, and there is no baseline to fall back to. The
    // pre-superblock state is no snapshot accrual for anyone, so restore
    // that rather than trying to open a snapshot at height 0.
    if (m_registry.LatestHeight() == 0) {
        LogPrint(LogFlags::TALLY, "Tally: no accrual snapshot to apply; clearing snapshot accrual.");
        for (auto& account_pair : accounts) {
            account_pair.second.m_accrual = 0;
        }
        return true;
    }
    return Apply(m_registry.LatestHeight(), accounts);
}

bool AccrualSnapshotRepository::Apply(const uint64_t height, ResearchAccountMap& accounts) const
{
    LogPrint(LogFlags::TALLY,
        "Tally: applying accrual snapshot %" PRIu64 "...", height);

    AccrualSnapshotReader reader(SnapshotPath(height));

    if (reader.IsNull()) {
        return error("%s: failed to open %" PRIu64, __func__, height);
    }

    AccrualSnapshot snapshot;

    try {
        snapshot = reader.Read();
    } catch (const std::exception& e) {
        return error("%s: %s", __func__, e.what());
    }

    m_registry.AssertHashMatches(height, reader.GetHash());

    for (auto& account_pair : accounts) {
        const Cpid& cpid = account_pair.first;
        ResearchAccount& account = account_pair.second;

        account.m_accrual = snapshot.GetAccrual(cpid);
    }

    // Apply snapshot accrual for any CPIDs with no accounting record as
    // of the last superblock:
    //
    for (const auto& cpid_pair : snapshot.m_records) {
        if (accounts.find(cpid_pair.first) == accounts.end()) {
            accounts[cpid_pair.first].m_accrual = cpid_pair.second;
        }
    }

    return true;
}

bool AccrualSnapshotRepository::Drop(const uint64_t height)
{
    LogPrint(LogFlags::TALLY,
        "Tally: dropping accrual snapshot %" PRIu64 "...", height);

    AccrualSnapshotFile::Remove(height);

    return m_registry.Deregister(height);
}

int AccrualSnapshotRepository::DropAboveHeight(const uint64_t tip_height)
{
    int dropped = 0;
    while (m_registry.LatestHeight() > tip_height) {
        const uint64_t latest = m_registry.LatestHeight();
        if (!Drop(latest)) {
            return -1;
        }
        ++dropped;
    }
    return dropped;
}

bool AccrualSnapshotRepository::CloseRegistryFile()
{
    return m_registry.Close();
}

SnapshotBaselineBuilder::SnapshotBaselineBuilder(ResearchAccountMap& researchers)
    : m_researchers(researchers)
    , m_superblock(SuperblockPtr::Empty())
{
}

bool SnapshotBaselineBuilder::Run(const CBlockIndex* pindex, const SuperblockPtr current_superblock)
{
    LogPrint(LogFlags::TALLY, "Tally: Building baseline snapshot...");

    // Although research accounts initialize with zero snapshot accrual,
    // we'll zero-out these again in case something changed those values
    // (like a testing RPC call):
    //
    for (auto& account_pair : m_researchers) {
        account_pair.second.m_accrual = 0;
    }

    // The maximum depth to consider for rewards corresponds to the legacy
    // rule that limits unclaimed accrual validity to roughly six months.
    //
    // We establish the baseline when connecting the block below the first
    // version 11 block, so we add 1 to the maximum depth:
    //
    const int64_t max_depth = pindex->nHeight + 1 - BLOCKS_PER_DAY * 30 * 6;

    LogPrint(LogFlags::TALLY, "  Snapshot max depth: %" PRId64, max_depth);

    // Seek to the block before the current superblock.
    //
    // We don't include the current superblock in the delta accrual baseline
    // because an accrual snapshot is active until the next superblock.
    //
    for (;
        pindex && pindex->nHeight >= current_superblock.m_height;
        pindex = pindex->pprev);

    // Calculate the pending accrual for active CPIDs from each historical
    // superblock.
    //
    // We begin tallying research reward accrual from the superblock prior
    // to the current superblock and sum the reward for each CPID by using
    // the magnitudes stored in each of the superblocks to compute accrual
    // earned during the period that a superblock was active.
    //
    int64_t payment_time = current_superblock.m_timestamp;

    for (; pindex && pindex->nHeight > max_depth; pindex = pindex->pprev) {
        if (!pindex->IsSuperblock()) {
            continue;
        }

        if (!LoadSuperblock(pindex)) {
            return false;
        }

        TallyAccrual(payment_time);

        payment_time = pindex->nTime;
    }

    // If the maximum depth is a superblock, we're done.
    //
    if (pindex->IsSuperblock()) {
        return true;
    }

    // Otherwise, we need to credit the remaining accrual between the last
    // superblock and the maximum depth for any CPIDs left.
    //
    // To accomplish this, we slide back one more superblock to engage the
    // magnitudes for this window that we then apply to the period between
    // the maximum depth and the superblock above it.
    //
    const CBlockIndex* const pindex_max = pindex;

    for (; pindex; pindex = pindex->pprev) {
        if (!pindex->IsSuperblock()) {
            continue;
        }

        // We intentionally bind the superblock to the wrong block index
        // to force accrual calculation at the time of the maximum depth
        // rather than at the time of the superblock's containing block:
        //
        if (!LoadSuperblock(pindex, pindex_max)) {
            return false;
        }

        TallyAccrual(payment_time);

        break;
    }

    return true;
}

bool SnapshotBaselineBuilder::LoadSuperblock(
    const CBlockIndex* const pindex,
    const CBlockIndex* const pindex_bind)
{
    assert(pindex->IsSuperblock()); // LINT-OK-ASSERT: relocated unchanged

    LogPrint(LogFlags::TALLY, "  Superblock: %" PRId64, pindex->nHeight);

    CBlock block;

    if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
        return error(
            "SnapshotBaselineBuilder: failed to load superblock %" PRIu64,
            pindex->nHeight);
    }

    m_superblock = block.GetSuperblock(pindex_bind ? pindex_bind : pindex);

    return true;
}

void SnapshotBaselineBuilder::TallyAccrual(const int64_t payment_time)
{
    const SnapshotCalculator calc(payment_time, m_superblock);

    for (const auto& cpid_pair : m_superblock->m_cpids) {
        ResearchAccount& account = m_researchers[cpid_pair.Cpid()];
        account.m_accrual += calc.AccrualDelta(cpid_pair.Cpid(), account);
    }
}

} // namespace GRC
