#pragma once

#include "fs.h"
#include "neuralnet/account.h"
#include "neuralnet/accrual/computer.h"
#include "neuralnet/beacon.h"
#include "neuralnet/cpid.h"
#include "neuralnet/superblock.h"
#include "serialize.h"
#include "streams.h"
#include "tinyformat.h"

#include <unordered_map>

class CBlockIndex;

namespace {
using namespace NN;
using LogFlags = BCLog::LogFlags;

//!
//! \brief Calculates the current accrual for a CPID by adding the snapshot of
//! accrued research rewards of the CPID's research account to rewards accrued
//! since the active superblock.
//!
class SnapshotCalculator
{
public:
    //!
    //! \brief Initialize a new snapshot calculator.
    //!
    //! \param payment_time Time of payment to calculate rewards at.
    //! \param superblock   Determines CPID magnitude and delta accrual age.
    //!
    SnapshotCalculator(const int64_t payment_time, SuperblockPtr superblock)
        : m_payment_time(payment_time)
        , m_superblock(std::move(superblock))
    {
    }

    //!
    //! \brief Get the magnitude unit factored into the reward calculation.
    //!
    //! \return Amount paid per unit of magnitude per day in units of GRC.
    //!
    static double MagnitudeUnit()
    {
        // Superblock-based accrual calculations do not rely on the rolling
        // two-week network payment average. Instead, we calculate research
        // rewards using the magnitude unit that represents the equilibrium
        // quantity of the formula used to determine the magnitude unit for
        // the legacy research age accrual calculations.
        //
        // Where...
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
        // ...rounded-up to:
        //
        return 0.25;
    }

    //!
    //! \brief Get the accrual earned since the start of an account's accrual
    //! period.
    //!
    //! \param cpid    CPID to calculate accrual for.
    //! \param account Provides historical accrual context.
    //!
    //! \return Accrual earned in units of 1/100000000 GRC.
    //!
    int64_t AccrualDelta(const Cpid& cpid, const ResearchAccount& account) const
    {
        double accrual_days;

        // If the CPID earned a reward on or after the current superblock, we
        // calculate the reward using plain research age. The CPID carries no
        // outstanding snapshot accrual.
        //
        // For CPIDs that did not stake a block after the current superblock,
        // we calculate accrual earned since the superblock arrived and apply
        // that on top of any rewards carried by their snapshot accrual. This
        // includes newbie accounts which begin to accrue rewards after their
        // CPIDs first appear in a superblock.
        //
        if (account.LastRewardHeight() >= m_superblock.m_height) {
            accrual_days = AccrualDays(account);
        } else {
            accrual_days = SuperblockAgeDays();
        }

        return accrual_days * CurrentMagnitude(cpid) * MagnitudeUnit() * COIN;
    }

protected:
    const int64_t m_payment_time;     //!< Payment time to calculate rewards at.
    const SuperblockPtr m_superblock; //!< Supplies CPID magnitudes.

    //!
    //! \brief Get the magnitude of the CPID in the active superblock.
    //!
    //! \param cpid CPID to fetch magnitude for.
    //!
    //! \return Magnitude of the CPID in the superblock or zero if the CPID
    //! does not exist in the superblock.
    //!
    double CurrentMagnitude(const Cpid& cpid) const
    {
        return m_superblock->m_cpids.MagnitudeOf(cpid).Floating();
    }

    //!
    //! \brief Get the time elapsed since the account's last research reward.
    //!
    //! \return Elapsed time in seconds.
    //!
    int64_t AccrualAge(const ResearchAccount& account) const
    {
        if (const BlockPtrOption pindex_option = account.LastRewardBlock()) {
            const CBlockIndex* const pindex = *pindex_option;

            if (m_payment_time > pindex->nTime) {
                return m_payment_time - pindex->nTime;
            }
        }

        return 0;
    }

    //!
    //! \brief Get the number of days since the account's last research reward.
    //!
    //! \return Elapsed time in days.
    //!
    double AccrualDays(const ResearchAccount& account) const
    {
        return AccrualAge(account) / 86400.0;
    }

    //!
    //! \brief Calculate the age of the active superblock to determine the
    //! duration of the accrual period.
    //!
    //! \return Superblock age as days in until to the payment time.
    //!
    double SuperblockAgeDays() const
    {
        const int64_t timespan = m_payment_time - m_superblock.m_timestamp;

        if (timespan <= 0) {
            return 0;
        }

        return timespan / 86400.0;
    }
}; // SnapshotCalculator

//!
//! \brief A calculator that computes the accrued research rewards for a
//! research account using delta snapshot rules.
//!
class SnapshotAccrualComputer : public IAccrualComputer, SnapshotCalculator
{
    // See IAccrualComputer for inherited API documentation.

public:
    //!
    //! \brief Initialze a delta snapshot accrual calculator.
    //!
    //! \param cpid         CPID to calculate research accrual for.
    //! \param account      CPID's historical accrual context.
    //! \param payment_time Time of payment to calculate rewards at.
    //! \param last_height  Height of the block for the reward.
    //! \param superblock   Determines CPID magnitude and delta accrual age.
    //!
    SnapshotAccrualComputer(
        const Cpid cpid,
        const ResearchAccount& account,
        const int64_t payment_time,
        const uint32_t last_height,
        SuperblockPtr superblock)
        : SnapshotCalculator(payment_time, std::move(superblock))
        , m_cpid(cpid)
        , m_account(account)
        , m_last_height(last_height)
    {
    }

    int64_t MaxReward() const override
    {
        // The maximum accrual that a CPID can claim in one block is limited to
        // the amount of accrual that a CPID can collect over two days when the
        // CPID acheives the maximum magnitude value supported in a superblock.
        //
        // Where...
        //
        //   max_magnitude = 32767
        //   magnitude_unit = 0.25
        //
        // ...then...
        //
        //   max_magnitude * magnitude_unit * 2 = max_accrual = 16383.5
        //
        // ...rounded-up to:
        //
        return 16384 * COIN;
    }

    double MagnitudeUnit() const override
    {
        return SnapshotCalculator::MagnitudeUnit();
    }

    int64_t AccrualAge() const override
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
            const int64_t beacon_time = GetBeaconRegistry().Try(m_cpid)->m_timestamp;

            if (beacon_time <= 0) {
                return 0;
            }

            return m_payment_time - beacon_time;
        }

        return SnapshotCalculator::AccrualAge(m_account);
    }

    double AccrualDays() const override
    {
        return AccrualAge() / 86400.0;
    }

    int64_t AccrualBlockSpan() const override
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

    int64_t PaymentPerDay() const override
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

    int64_t PaymentPerDayLimit() const override
    {
        return MaxReward();
    }

    bool ExceededRecentPayments() const override
    {
        return RawAccrual() > PaymentPerDayLimit();
    }

    int64_t ExpectedDaily() const override
    {
        return CurrentMagnitude(m_cpid) * MagnitudeUnit() * COIN;
    }

    int64_t RawAccrual() const override
    {
        if (m_account.LastRewardHeight() >= m_superblock.m_height) {
            return AccrualDelta(m_cpid, m_account);
        }

        return m_account.m_accrual + AccrualDelta(m_cpid, m_account);
    }

    int64_t Accrual() const override
    {
        const int64_t accrual = RawAccrual();

        if (accrual > MaxReward()) {
            return MaxReward();
        }

        return accrual;
    }

private:
    const Cpid m_cpid;                //!< CPID to calculate accrual for.
    const ResearchAccount& m_account; //!< CPID's historical accrual context.
    const uint32_t m_last_height;     //!< Height of the block for the reward.
}; // SnapshotAccrualComputer

//!
//! \brief Get the path to the accrual snapshot storage directory.
//!
fs::path SnapshotDirectory()
{
    return GetDataDir() / "accrual";
}

//!
//! \brief Get the path to a snapshot file.
//!
//! \param height Block height of the snapshot data.
//!
//! \return Path to the snapshot file in the snapshot directory.
//!
fs::path SnapshotPath(const uint64_t height)
{
    return SnapshotDirectory() / strprintf("%" PRIu64 ".dat", height);
}

//!
//! \brief Contains a snapshot of pending research reward accrual for CPIDs in
//! the network at a point in time.
//!
//! Except for the first baseline, the wallet creates accrual snapshots when it
//! receives blocks that contain a superblock. It stores these to disk to avoid
//! reading superblocks from disk to recalculate accrual upon start-up and when
//! reorganizing the chain.
//!
class AccrualSnapshot
{
public:
    //!
    //! \brief Version number of the current format for a serialized snapshot.
    //!
    static constexpr uint32_t CURRENT_VERSION = 1;

    //!
    //! \brief Initialize an empty accrual snapshot.
    //!
    AccrualSnapshot()
        : m_version(CURRENT_VERSION)
        , m_height(0)
    {
    }

    //!
    //! \brief Initialize an accrual snapshot by deserializing it from the
    //! provided file.
    //!
    //! \param s The input stream.
    //!
    AccrualSnapshot(deserialize_type, CAutoFile& file)
    {
        m_records.clear();

        file >> m_version;
        file >> m_height;

        while (true) {
            std::pair<Cpid, int64_t> record;

            try {
                file >> record;
            } catch (const std::ios_base::failure& e) {
                if (feof(file.Get())) {
                    break;
                }

                throw;
            }

            m_records.emplace(record.first, record.second);
        }
    }

    //!
    //! \brief Get the accrual at the time of the snapshot for the specified
    //! CPID.
    //!
    //! \param cpid CPID to fetch accrual for.
    //!
    //! \return Accrued research rewards at the time of the snapshot in units
    //! of 1/100000000 GRC or zero if the CPID does not exist in the snapshot.
    //!
    int64_t GetAccrual(const Cpid cpid) const
    {
        auto iter = m_records.find(cpid);

        if (iter == m_records.end()) {
            return 0;
        }

        return iter->second;
    }

private:
    uint32_t m_version; //!< Version of the serialized snapshot format.
    uint64_t m_height;  //!< Block height of the snapshot.

    //!
    //! \brief Maps CPIDs to rewards accrued at the time of the snapshot.
    //!
    //! Accrual values stored in units of of 1/100000000 GRC.
    //!
    std::unordered_map<Cpid, int64_t> m_records;
}; // AccrualSnapshot

constexpr uint32_t AccrualSnapshot::CURRENT_VERSION; // for clang

//!
//! \brief Base class for types that read and write accrual snapshot files.
//!
class AccrualSnapshotFile
{
public:
    //!
    //! \brief Initialize an accrual snapshot file.
    //!
    //! \param file Handle of the snapshot file to manage.
    //!
    AccrualSnapshotFile(FILE* file)
        : m_file(file, SER_DISK, AccrualSnapshot::CURRENT_VERSION)
    {
    }

    //!
    //! \brief Determine whether the wrapped file handle is \c nullptr .
    //!
    //! \return \c true if initialized with a null file handle. This may occur
    //! when the operating system filesystem API failed to open the file.
    //!
    bool IsNull() const
    {
        return m_file.IsNull();
    }

protected:
    CAutoFile m_file; //!< Abstracts snapshot file operations.
}; // AccrualSnapshotFile

//!
//! \brief Reads an accrual snapshot file from disk.
//!
class AccrualSnapshotReader : public AccrualSnapshotFile
{
public:
    //!
    //! \brief Initialize an accrual snapshot file reader.
    //!
    //! \param snapshot_path Path to the snapshot file to read.
    //!
    AccrualSnapshotReader(const fs::path& snapshot_path)
        : AccrualSnapshotFile(fsbridge::fopen(snapshot_path, "rb"))
    {
    }

    //!
    //! \brief Deserialize the snapshot file from disk.
    //!
    //! \return The contents of the snapshot file.
    //!
    AccrualSnapshot Read()
    {
        return AccrualSnapshot(deserialize, m_file);
    }
}; // AccrualSnapshotReader

//!
//! \brief Writes an accrual snapshot to a file on disk.
//!
class AccrualSnapshotWriter : public AccrualSnapshotFile
{
public:
    //!
    //! \brief Initialize an accrual snapshot file writer.
    //!
    //! \param snapshot_path Path to the snapshot file to write.
    //!
    AccrualSnapshotWriter(const fs::path& snapshot_path)
        : AccrualSnapshotFile(fsbridge::fopen(snapshot_path, "wb"))
    {
    }

    //!
    //! \brief Write the header of an accrual snapshot.
    //!
    //! \param height Block height of the snapshot. Usually a superblock.
    //!
    void WriteHeader(const uint64_t height)
    {
        m_file << AccrualSnapshot::CURRENT_VERSION;
        m_file << height;
    }

    //!
    //! \brief Write a CPID to accrual mapping to the snapshot file.
    //!
    //! \param cpid    Identifies the owner of the accrual.
    //! \param accrual Accrued research rewards in units of 1/100000000 GRC.
    //!
    void WriteRecord(const Cpid cpid, const int64_t accrual)
    {
        m_file << cpid << accrual;
    }
}; // AccrualSnapshotWriter

//!
//! \brief Maintains context for the set of active accrual snapshots.
//!
class AccrualSnapshotRegistry
{
public:
    //!
    //! \brief Version number of the current format for a serialized registry
    //! file.
    //!
    static constexpr uint32_t CURRENT_VERSION = 1;

    //!
    //! \brief Load the snapshot registry from disk and prepare it for use.
    //!
    //! \return \c false if the registry failed to initialize because of an IO
    //! error.
    //!
    bool Initialize()
    {
        LogPrintf("Initializing accrual snapshot registry...");

        try {
            CAutoFile registry_file(
                fsbridge::fopen(RegistryPath(), "rb"),
                SER_DISK,
                CURRENT_VERSION);

            if (!registry_file.IsNull()) {
                Unserialize(registry_file);
            }
        } catch (const std::exception& e) {
            return error("%s: %s", __func__, e.what());
        }

        LogPrintf("Accrual snapshot registry loaded. Compacting...");

        return Rewrite();
    }

    //!
    //! \brief Set the height of the block for the accrual snapshot baseline.
    //!
    //! The wallet stores the baseline snapshot at the block before the switch
    //! to version 11 blocks. Testing RPCs may create this baseline at another
    //! height.
    //!
    //! \return \c false if the registry failed to store the baseline because
    //! of an IO error.
    //!
    bool ResetBaseline(const uint64_t height)
    {
        if (!WriteEntry(Action::BASELINE, height) && Register(height)) {
            return error("%s: failed to record baseline snapshot", __func__);
        }

        LogPrint(LogFlags::TALLY,
            "Tally: reset new accrual snapshot baseline: %" PRIu64, height);

        m_heights.clear();

        return true;
    }

    //!
    //! \brief Get the height of the baseline accrual snapshot.
    //!
    //! \return Zero if no baseline snapshot exists yet.
    //!
    uint64_t BaselineHeight() const
    {
        if (!m_heights.empty()) {
            return m_heights.front();
        }

        return 0;
    }

    //!
    //! \brief Get the height of the most recent accrual snapshot.
    //!
    //! \return Zero if no baseline snapshot exists yet.
    //!
    uint64_t LatestHeight() const
    {
        if (!m_heights.empty()) {
            return m_heights.back();
        }

        return 0;
    }

    //!
    //! \brief Add the height of a new accrual snapshot to the registry.
    //!
    //! \param height Block height of the new snapshot. Usually a superblock.
    //!
    //! \return \c false if the registry failed to store the snapshot context
    //! because of an IO error.
    //!
    bool Register(const uint64_t height)
    {
        assert(m_heights.empty() || height > m_heights.back());

        if (!WriteEntry(Action::REGISTER, height)) {
            return error("%s: failed to add %" PRIu64, __func__, height);
        }

        LogPrint(LogFlags::TALLY,
            "Tally: recorded new accrual snapshot %" PRIu64, height);

        m_heights.emplace_back(height);

        return true;
    }

    //!
    //! \brief Remove the height of a defunct accrual snapshot from the registry.
    //!
    //! \param height Block height of the snapshot. Usually a superblock.
    //!
    //! \return \c false if the registry failed to store the snapshot context
    //! because of an IO error.
    //!
    bool Deregister(const uint64_t height)
    {
        assert(!m_heights.empty() && height == m_heights.back());

        if (!WriteEntry(Action::DEREGISTER, height)) {
            return error("%s: failed to remove %" PRIu64, __func__, height);
        }

        LogPrint(LogFlags::TALLY,
            "Tally: recorded accrual snapshot removal %" PRIu64, height);

        m_heights.pop_back();

        return true;
    }

    //!
    //! \brief Serialize the provided data to the registry file.
    //!
    //! \param pch  The bytes to serialize.
    //! \param size Byte length of the data.
    //!
    //! TODO: encapsulate this
    //!
    void write(const char* pch, size_t size)
    {
        if (!m_file) {
            throw std::ios_base::failure(
                strprintf("%s: file handle is nullptr", __func__));
        }

        if (fwrite(pch, 1, size, m_file) != size) {
            throw std::ios_base::failure(
                strprintf("%s: write failed", __func__));
        }
    }
private:
    //!
    //! \brief
    //!
    enum class Action
    {
        BASELINE,    //!< Set the height of the baseline accrual snapshot.
        REGISTER,    //!< Add a new accrual snapshot.
        DEREGISTER,  //!< Remove an existing accrual snapshot.
    };

    //!
    //! \brief
    //!
    struct Entry
    {
        Action m_action;
        uint64_t m_height;

        Entry(const Action action, const uint64_t height)
            : m_action(action), m_height(height)
        {
        }

        Entry(deserialize_type, CAutoFile& file)
        {
            Unserialize(file);
        }

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            uint8_t action = static_cast<uint8_t>(m_action);
            READWRITE(action);
            m_action = static_cast<Action>(action);

            READWRITE(m_height);
        }
    };

    FILE* m_file;                    //!< Handle of the registry file.
    std::vector<uint64_t> m_heights; //!< Ordered heights of each snapshot.

    //!
    //! \brief
    //!
    static fs::path RegistryPath()
    {
        return SnapshotDirectory() / "registry.dat";
    }

    //!
    //! \brief Truncate the registry file and reopen it for writing.
    //!
    //! \return \c false if an IO error occurred.
    //!
    bool ReopenForWrite()
    {
        if (m_file && fclose(m_file) != 0) {
            return error("%s: failed to close registry", __func__);
        }

        m_file = fsbridge::fopen(RegistryPath(), "wb");

        return m_file || error(
            "%s: failed to open snapshot registry for writing: %s",
            __func__,
            RegistryPath().string());
    }

    //!
    //! \brief Prunes the registry file of any non-current entries and opens it
    //! for writing.
    //!
    //! \return \c false if an IO error occurred.
    //!
    bool Rewrite()
    {
        if (!ReopenForWrite()) {
            return false;
        }

        try {
            ::Serialize(*this, CURRENT_VERSION);
        } catch (const std::exception& e) {
            return error("%s: %s", __func__, e.what());
        }

        if (m_heights.empty()) {
            return true;
        }

        if (!WriteEntry(Action::BASELINE, m_heights.front())) {
            return false;
        }

        for (const auto& height : m_heights) {
            if (!WriteEntry(Action::REGISTER, height)) {
                return false;
            }
        }

        return true;
    }

    //!
    //! \brief Write a registry entry to disk.
    //!
    //! \param action Type of registry entry to write.
    //! \param height Height of the accrual snapshot associated with the entry.
    //!
    //! \return \c false if an IO error occurred.
    //!
    bool WriteEntry(const Action action, const uint64_t height)
    {
        try {
            ::Serialize(*this, Entry(action, height));
        } catch (const std::exception& e) {
            return error("%s: %s", __func__, e.what());
        }

        return fflush(m_file) == 0;
    }

    //!
    //! \brief Read the snapshot registry file from disk.
    //!
    //! \param file Wraps the registry file to deserialize.
    //!
    void Unserialize(CAutoFile& file)
    {
        m_heights.clear();

        uint32_t version;
        file >> version;

        while (true) {
            try {
                const Entry entry(deserialize, file);

                switch (entry.m_action) {
                    case Action::BASELINE:
                        LogPrint(LogFlags::ACCRUAL,
                            "  Baseline: %" PRIu64, entry.m_height);

                        m_heights.clear();
                        break;
                    case Action::REGISTER:
                        LogPrint(LogFlags::ACCRUAL,
                            "  Added: %" PRIu64, entry.m_height);

                        m_heights.emplace_back(entry.m_height);
                        break;
                    case Action::DEREGISTER:
                        LogPrint(LogFlags::ACCRUAL,
                            "  Removed: %" PRIu64, entry.m_height);

                        m_heights.pop_back();
                }
            } catch (const std::ios_base::failure& e) {
                if (feof(file.Get())) {
                    break;
                }

                throw;
            }
        }
    }
}; // AccrualSnapshotRegistry

//!
//! \brief Manages storage of accrual snapshots.
//!
//! TODO: Add snapshot pruning to reclaim disk space for very old snapshots.
//! TODO: Add a way to rebuild the snapshots in case of file corruption, etc.
//!
class AccrualSnapshotRepository
{
public:
    //!
    //! \brief Initialize the accrual snapshot system.
    //!
    //! \return \c false if the snapshot system failed to initialize because of
    //! an error.
    //!
    bool Initialize()
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

    //!
    //! \brief Determine whether the node already stored a baseline accrual
    //! snapshot.
    //!
    //! \return \c true if the node previously activated the accrual snapshot
    //! system.
    //!
    bool HasBaseline() const
    {
        return m_registry.BaselineHeight() > 0;
    }

    //!
    //! \brief Store a snapshot of accrual for each account as the baseline.
    //!
    //! \param height   Height of the block to associate with the snapshot.
    //! \param accounts Research accounts to record accrual from.
    //!
    //! \return \c false when an error occurs while creating a snapshot.
    //!
    bool StoreBaseline(const uint64_t height, const ResearchAccountMap& accounts)
    {
        return m_registry.ResetBaseline(height) && Store(height, accounts);
    }

    //!
    //! \brief Store a snapshot of accrual for each account to disk.
    //!
    //! \param height   Height of the block to associate with the snapshot.
    //! \param accounts Research accounts to record accrual from.
    //!
    //! \return \c false when an error occurs while creating a snapshot.
    //!
    bool Store(const uint64_t height, const ResearchAccountMap& accounts)
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

        return m_registry.Register(height);
    }

    //!
    //! \brief Load the most recent accrual snapshot for each account.
    //!
    //! \param accounts Research accounts to apply snapshot accrual to.
    //!
    //! \return \c false when an error occurs while loading a snapshot.
    //!
    bool ApplyLatest(ResearchAccountMap& accounts) const
    {
        return Apply(m_registry.LatestHeight(), accounts);
    }

    //!
    //! \brief Load the specified accrual snapshot for each account.
    //!
    //! \param height   Block height of the snapshot. Usually a superblock.
    //! \param accounts Research accounts to apply snapshot accrual to.
    //!
    //! \return \c false when an error occurs while loading a snapshot.
    //!
    bool Apply(const uint64_t height, ResearchAccountMap& accounts) const
    {
        LogPrint(LogFlags::TALLY,
            "Tally: applying accrual snapshot %" PRIu64 "...", height);

        AccrualSnapshotReader reader(SnapshotPath(height));

        if (reader.IsNull()) {
            return error("%s: failed to open %" PRIu64, __func__, height);
        }

        try {
            const AccrualSnapshot snapshot = reader.Read();

            for (auto& account_pair : accounts) {
                const Cpid& cpid = account_pair.first;
                ResearchAccount& account = account_pair.second;

                account.m_accrual = snapshot.GetAccrual(cpid);
            }
        } catch (const std::exception& e) {
            return error("%s: %s", __func__, e.what());
        }

        return true;
    }

    //!
    //! \brief Erase the specified accrual snapshot.
    //!
    //! \param height Block height of the snapshot. Usually a superblock.
    //!
    //! \return \c false when an error occurs while removing a snapshot.
    //!
    bool Drop(const uint64_t height)
    {
        LogPrint(LogFlags::TALLY,
            "Tally: dropping accrual snapshot %" PRIu64 "...", height);

        try {
            fs::remove(SnapshotPath(height));
        } catch (const std::exception& e) {
            // Failing to remove the snapshot file is not a critical error as
            // long as we can remove it from the registry.
            //
            LogPrintf("WARNING: %s: %s", __func__, e.what());
        }

        return m_registry.Deregister(height);
    }

private:
    AccrualSnapshotRegistry m_registry; //!< Tracks snapshot files state.
}; // AccrualSnapshotRepository

//!
//! \brief Establishes the baseline accrual for each CPID in the network for
//! the transition to snapshot accrual calculations.
//!
class SnapshotBaselineBuilder
{
public:
    //!
    //! \brief Initialize a new baseline builder.
    //!
    //! \param researchers The current set of research accounts for known CPIDs.
    //!
    SnapshotBaselineBuilder(ResearchAccountMap& researchers)
        : m_researchers(researchers)
        , m_superblock(SuperblockPtr::Empty())
    {
    }

    //!
    //! \brief Scan the chain to establish the baseline delta snapshot accrual
    //! for each CPID in the network and apply it to the research accounts.
    //!
    //! \param pindex             Block to establish the accrual baseline from.
    //! \param current_superblock Baseline starts from before this superblock.
    //!
    //! \return \c false if an error occurs while processing historical accrual.
    //!
    bool Run(const CBlockIndex* pindex, const SuperblockPtr current_superblock)
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
            if (pindex->nIsSuperBlock != 1) {
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
        if (pindex->nIsSuperBlock == 1) {
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
            if (pindex->nIsSuperBlock != 1) {
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

private:
    ResearchAccountMap& m_researchers; //!< Current set of known CPIDs.
    SuperblockPtr m_superblock;        //!< Current historical superblock.

    //!
    //! \brief Read the superblock at the specified block index from disk.
    //!
    //! The optional \p pindex_bind parameter overrides the superblock's block
    //! context by instructing this method to associate a superblock with that
    //! block instead. This allows us to request a SuperblockPtr object with a
    //! different block height and timestamp than the block which contains the
    //! superblock to manipulate the calculated accrual at the baseline window
    //! limit (the deepest block) when that block is not a superblock itself.
    //!
    //! \param pindex      Used to locate the containing block on disk.
    //! \param pindex_bind Context of the block to bind to the superblock.
    //!
    //! \return \c false if an error occurred while reading the block from disk.
    //!
    bool LoadSuperblock(
        const CBlockIndex* const pindex,
        const CBlockIndex* const pindex_bind = nullptr)
    {
        assert(pindex->nIsSuperBlock == 1);

        LogPrint(LogFlags::TALLY, "  Superblock: %" PRId64, pindex->nHeight);

        CBlock block;

        if (!block.ReadFromDisk(pindex)) {
            return error(
                "SnapshotBaselineBuilder: failed to load superblock %" PRIu64,
                pindex->nHeight);
        }

        m_superblock = SuperblockPtr::BindShared(
            block.PullSuperblock(),
            pindex_bind != nullptr ? pindex_bind : pindex);

        return true;
    }

    //!
    //! \brief Apply the accrual earned for the current superblock to the total
    //! accrual for each of the CPIDs that it contains.
    //!
    //! \param payment_time Timestamp of the end of the accrual period.
    //!
    void TallyAccrual(const int64_t payment_time)
    {
        const SnapshotCalculator calc(payment_time, m_superblock);

        for (const auto& cpid_pair : m_superblock->m_cpids) {
            ResearchAccount& account = m_researchers[cpid_pair.Cpid()];
            account.m_accrual += calc.AccrualDelta(cpid_pair.Cpid(), account);
        }
    }
}; // SnapshotBaselineBuilder
} // anonymous namespace
