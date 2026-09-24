// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_ACCRUAL_SNAPSHOT_H
#define GRIDCOIN_ACCRUAL_SNAPSHOT_H

#include "amount.h"
#include "arith_uint256.h"
#include "chainparams.h"
#include "fs.h"
#include "gridcoin/account.h"
#include "gridcoin/accrual/computer.h"
#include "gridcoin/cpid.h"
#include "gridcoin/protocol.h"
#include "gridcoin/sidestake.h"
#include "gridcoin/superblock.h"
#include "gridcoin/support/block_finder.h"
#include "gridcoin/support/filehash.h"
#include "serialize.h"
#include "streams.h"
#include "tinyformat.h"

#include <stdexcept>
#include <unordered_map>

class CBlockIndex;

namespace GRC {

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
    //! \brief Returns the current magnitude unit allocation fraction as of the provided superblock. Prior to block v13 this is
    //! fixed at 1/4.
    //!
    //! The magnitude unit here will only change along superblock boundaries. Whatever the active value of the magnitude unit is
    //! at the superblock provided will be used for accruals.
    //!
    //! \return Allocation Fraction representing Magnitude Unit.
    //!
    Allocation GetMagnitudeUnit() const
    {
        // Fern+ and before V13 magnitude unit is fixed at 1/4.
        if (!IsV13Enabled(m_superblock.m_height)) {
            return Allocation(1, 4);
        }

        Allocation magnitude_unit = Params().GetConsensus().DefaultMagnitudeUnit;
        Allocation max_magnitude_unit = Params().GetConsensus().MaxMagnitudeUnit;

        // Find the current protocol entry value for Magnitude Weight Factor, if it exists.
        ProtocolEntryOption protocol_entry = GetProtocolRegistry().TryLastBeforeTimestamp("magnitudeunit", m_superblock.m_timestamp);

        // If their is an entry prior or equal in timestamp to the superblock and it is active then set the magnitude unit
        // to that value. If the last entry is not active (i.e. deleted), then leave at the default.
        if (protocol_entry != nullptr && protocol_entry->m_status == ProtocolEntryStatus::ACTIVE) {
            magnitude_unit = Fraction::FromString(protocol_entry->m_value);
        }

        // Clamp to MaxMagnitudeUnit if necessary
        if (magnitude_unit > max_magnitude_unit) {
            magnitude_unit = max_magnitude_unit;
            LogPrintf("WARN: %s: Magnitude Unit specified by protocol is greater than %s. Clamping to %s.",
                      __func__,
                      max_magnitude_unit.ToString(),
                      max_magnitude_unit.ToString());
        }

        return magnitude_unit;
    }

    //!
    //! \brief Static version of GetMagnitudeUnit used in Tally class.
    //!
    //! \param index for which to return the current magnitude unit.
    //!
    //! \return Allocation fraction representing magnitude unit.
    //!
    static Allocation GetMagnitudeUnit(CBlockIndex* index) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        CBlockIndex* sb_index = BlockFinder::FindLatestSuperblock(index);

        // Before V13 magnitude unit is 1/4.
        if (!IsV13Enabled(sb_index->nHeight)) {
            return Allocation(1, 4);
        }

        Allocation magnitude_unit = Params().GetConsensus().DefaultMagnitudeUnit;
        Allocation max_magnitude_unit = Params().GetConsensus().MaxMagnitudeUnit;

        // Find the current protocol entry value for Magnitude Weight Factor, if it exists.
        ProtocolEntryOption protocol_entry = GetProtocolRegistry().TryLastBeforeTimestamp("magnitudeunit", sb_index->GetBlockTime());

        // If their is an entry prior or equal in timestamp to the superblock and it is active then set the magnitude unit
        // to that value. If the last entry is not active (i.e. deleted), then leave at the default.
        if (protocol_entry != nullptr && protocol_entry->m_status == ProtocolEntryStatus::ACTIVE) {
            magnitude_unit = Fraction::FromString(protocol_entry->m_value);
        }

        // Clamp to MaxMagnitudeUnit if necessary
        if (magnitude_unit > max_magnitude_unit) {
            magnitude_unit = max_magnitude_unit;
            LogPrintf("WARN: %s: Magnitude Unit specified by protocol is greater than %s. Clamping to %s.",
                      __func__,
                      max_magnitude_unit.ToString(),
                      max_magnitude_unit.ToString());
        }

        return magnitude_unit;
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
    CAmount AccrualDelta(const Cpid& cpid, const ResearchAccount& account) const
    {
        int64_t accrual_timespan;

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
            accrual_timespan = AccrualAge(account);
        } else {
            accrual_timespan = SuperblockAge();
        }

        // CONSENSUS: avoid floating-point arithmetic.
        //
        // Some 32-bit x86 implementations do not perfectly conform to the IEEE
        // 754 floating-point spec for extended precision. To sustain consensus
        // on these platforms, we eschew the semantic representation of accrual
        // calculation for block version 11+ and compute accrual using integer-
        // only arithmetic. The following produces an accrual value defined by:
        //
        //   accrual_days * CurrentMagnitude(cpid) * MagnitudeUnit() * COIN
        //
        // We deconstruct the magnitude unit coefficient ratio as a reminder to
        // review this value if the protocol rule changes in the future:
        //
        const uint64_t base_accrual = accrual_timespan
            * CurrentMagnitude(cpid).Scaled()
            * GetMagnitudeUnit().GetNumerator();

        // If the accrual calculation will overflow a 64-bit integer, we need
        // more bits. Arithmetic with the big integer type is much slower and
        // unnecessary in the majority of cases so switch only when required:
        //
        CAmount accrual = 0;

        if (base_accrual > std::numeric_limits<uint64_t>::max() / COIN) {
            arith_uint256 accrual_bn(base_accrual);
            accrual_bn *= COIN;
            accrual_bn /= 86400;
            accrual_bn /= Magnitude::SCALE_FACTOR;
            accrual_bn /= GetMagnitudeUnit().GetDenominator();

            accrual = accrual_bn.GetLow64();
        } else {
            accrual = base_accrual
                    * COIN
                    / 86400
                    / Magnitude::SCALE_FACTOR
                    / GetMagnitudeUnit().GetDenominator();
        }

        LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: CPID = %s, LastRewardHeight() = %u, accrual_timespan = %" PRId64 ", "
                    "accrual = %" PRId64 ".", __func__, cpid.ToString(), account.LastRewardHeight(),
                    accrual_timespan, accrual);

        return accrual;
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
    Magnitude CurrentMagnitude(const Cpid& cpid) const
    {
        return m_superblock->m_cpids.MagnitudeOf(cpid);
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
    //! \brief Calculate the age of the active superblock to determine the
    //! duration of the accrual period.
    //!
    //! \return Superblock age as seconds from the payment time.
    //!
    int64_t SuperblockAge() const
    {
        const int64_t timespan = m_payment_time - m_superblock.m_timestamp;

        if (timespan <= 0) {
            return 0;
        }

        return timespan;
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
    //! \brief Initialize a delta snapshot accrual calculator.
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

    CAmount MaxReward() const override;

    //!
    //! \brief Get the magnitude unit factored into the reward calculation.
    //!
    //! CONSENSUS: This method produces a semantic floating-point value for
    //! the magnitude unit. Do not use this value directly to implement any
    //! consensus-critical routine. Instead, prefer integer arithmetic for a
    //! protocol implementation that needs to avoid floating-point error or
    //! that requires portability between platforms.
    //!
    //! \return Amount paid per unit of magnitude per day in units of GRC.
    //!
    double MagnitudeUnit() const override;

    int64_t AccrualAge() const override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    double AccrualDays() const override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    int64_t AccrualBlockSpan() const override;

    CAmount PaymentPerDay() const override;

    CAmount PaymentPerDayLimit() const override;

    CAmount NearRewardLimit() const override;

    bool ExceededRecentPayments() const override;

    CAmount ExpectedDaily() const override;

    CAmount RawAccrual() const override;

    CAmount Accrual() const override;

private:
    const Cpid m_cpid;                //!< CPID to calculate accrual for.
    const ResearchAccount& m_account; //!< CPID's historical accrual context.
    const uint32_t m_last_height;     //!< Height of the block for the reward.
}; // SnapshotAccrualComputer

//!
//! \brief Get the path to the accrual snapshot storage directory.
//!
fs::path SnapshotDirectory();

//!
//! \brief Get the path to a snapshot file.
//!
//! \param height Block height of the snapshot data.
//!
//! \return Path to the snapshot file in the snapshot directory.
//!
fs::path SnapshotPath(const uint64_t height);

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
    using AccrualMap = std::unordered_map<Cpid, int64_t>;

    //!
    //! \brief Version number of the current format for a serialized snapshot.
    //!
    static constexpr uint32_t CURRENT_VERSION = 1;

    uint32_t m_version; //!< Version of the serialized snapshot format.
    uint64_t m_height;  //!< Block height of the snapshot.

    //!
    //! \brief Maps CPIDs to rewards accrued at the time of the snapshot.
    //!
    //! Accrual values stored in units of 1/100000000 GRC.
    //!
    AccrualMap m_records;

    //!
    //! \brief Initialize an empty accrual snapshot.
    //!
    AccrualSnapshot();

    //!
    //! \brief Initialize an accrual snapshot by deserializing it from the
    //! provided file.
    //!
    //! \param s The input stream.
    //!
    AccrualSnapshot(deserialize_type, CAutoHasherFile& file);

    //!
    //! \brief Get the accrual at the time of the snapshot for the specified
    //! CPID.
    //!
    //! \param cpid CPID to fetch accrual for.
    //!
    //! \return Accrued research rewards at the time of the snapshot in units
    //! of 1/100000000 GRC or zero if the CPID does not exist in the snapshot.
    //!
    CAmount GetAccrual(const Cpid cpid) const;
}; // AccrualSnapshot

//!
//! \brief Base class for types that read and write accrual snapshot files.
//!
class AccrualSnapshotFile
{
public:
    //!
    //! \brief Initialize an accrual snapshot file.
    //!
    //! \param file     Handle of the snapshot file to manage.
    //! \param ser_type Type of serialization target.
    //!
    AccrualSnapshotFile(FILE* file, const int ser_type);

    //!
    //! \brief Initialize an accrual snapshot file.
    //!
    //! \param file Handle of the snapshot file to manage.
    //!
    AccrualSnapshotFile(FILE* file);

    //!
    //! \brief Extract the block height from an accrual snapshot file name.
    //!
    //! \param snapshot_path Path to a snapshot file.
    //!
    //! \return Block height contained in the file name or zero if the file
    //! name does not contain a valid height number.
    //!
    static uint64_t ParseHeight(const fs::path& snapshot_path);

    //!
    //! \brief Remove the accrual snapshot file at the specified path.
    //!
    //! \param snapshot_path Path to a snapshot file.
    //!
    static void Remove(const fs::path& snapshot_path);

    //!
    //! \brief Remove the accrual snapshot file for the specified height.
    //!
    //! \param height Block height of the accrual snapshot to remove.
    //!
    static void Remove(const uint64_t height);

    //!
    //! \brief Determine whether the wrapped file handle is \c nullptr .
    //!
    //! \return \c true if initialized with a null file handle. This may occur
    //! when the operating system filesystem API failed to open the file.
    //!
    bool IsNull() const;

    //!
    //! \brief Get the hash of the snapshot after reading or writing the file.
    //!
    //! \return SHA256 hash of the snapshot file.
    //!
    uint256 GetHash();

protected:
    CAutoHasherFile m_file; //!< Abstracts snapshot file operations.
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
    //! \param ser_type      Type of serialization target.
    //!
    AccrualSnapshotReader(const fs::path& snapshot_path, const int ser_type);

    //!
    //! \brief Initialize an accrual snapshot file reader.
    //!
    //! \param snapshot_path Path to the snapshot file to read.
    //!
    AccrualSnapshotReader(const fs::path& snapshot_path);

    //!
    //! \brief Compute the hash of the specified snapshot file.
    //!
    //! \param snapshot_path Path to the snapshot file to hash.
    //!
    //! \return SHA256 hash of the snapshot file.
    //!
    static uint256 Hash(const fs::path& snapshot_path);

    //!
    //! \brief Deserialize the snapshot file from disk.
    //!
    //! \return The contents of the snapshot file.
    //!
    AccrualSnapshot Read();
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
    AccrualSnapshotWriter(const fs::path& snapshot_path);

    //!
    //! \brief Write the header of an accrual snapshot.
    //!
    //! \param height Block height of the snapshot. Usually a superblock.
    //!
    void WriteHeader(const uint64_t height);

    //!
    //! \brief Write a CPID to accrual mapping to the snapshot file.
    //!
    //! \param cpid    Identifies the owner of the accrual.
    //! \param accrual Accrued research rewards in units of 1/100000000 GRC.
    //!
    void WriteRecord(const Cpid cpid, const int64_t accrual);
}; // AccrualSnapshotWriter

//!
//! \brief Thrown when encountering a problem with persistent state in the
//! snapshot repository.
//!
class SnapshotStateError : public std::runtime_error
{
public:
    explicit SnapshotStateError(const std::string& what)
        : std::runtime_error(what)
    {
    }
}; // SnapshotStateError

//!
//! \brief Thrown when a snapshot file hash does not match the hash recorded in
//! the snapshot registry.
//!
class SnapshotHashMismatchError : public SnapshotStateError
{
public:
    explicit SnapshotHashMismatchError(
        const uint64_t height,
        const uint256& expected_hash,
        const uint256& computed_hash)
        : SnapshotStateError(strprintf(
            "Snapshot hash mismatch for %" PRIu64 ": expected %s, computed %s",
            height,
            expected_hash.ToString(),
            computed_hash.ToString()))
    {
    }
}; // SnapshotHashMismatchError

//!
//! \brief Thrown when a superblock in the chain does not have a registry entry
//! in the snapshot registry.
//!
class SnapshotMissingError : public SnapshotStateError
{
public:
    explicit SnapshotMissingError(const uint64_t height)
        : SnapshotStateError(strprintf(
            "Superblock at %" PRIu64 " does not have a registry entry.",
            height))
    {
    }
}; // SnapshotMissingError

//!
//! \brief Thrown when the snapshot registry file format on disk does not match
//! the current version supported by the application.
//!
class SnapshotRegistryVersionMismatchError : public SnapshotStateError
{
public:
    explicit SnapshotRegistryVersionMismatchError(
        const uint32_t disk_version,
        const uint32_t min_supported_version)
        : SnapshotStateError(strprintf(
            "Unsupported snapshot registry version: %" PRIu32 " < %" PRIu32,
            disk_version,
            min_supported_version))
    {
    }
}; // SnapshotRegistryVersionMismatchError

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
    //! Version 1: Format released with the snapshot accrual system in v5.0.0.
    //!
    //! Version 2: Version incremented to force the accrual snapshot system to
    //! rebuild the stored snapshot state to fix a bug for v5.2.3. Disk format
    //! does not change.
    //!
    static constexpr uint32_t CURRENT_VERSION = 2;

    //!
    //! \brief A record of an accrual snapshot in the registry.
    //!
    class Entry
    {
    public:
        uint64_t m_height;       //!< Block height of the snapshot.
        uint256 m_snapshot_hash; //!< Hash of the snapshot data.

        //!
        //! \brief Initialize a snapshot registry entry.
        //!
        Entry(const uint64_t height, const uint256 snapshot_hash)
            : m_height(height), m_snapshot_hash(snapshot_hash)
        {
        }

        bool operator<(const uint64_t height) const
        {
            return m_height < height;
        }

        bool operator<(const Entry& other) const
        {
            return m_height < other.m_height;
        }

        //!
        //! \brief Assert that the provided hash matches the hash stored in the
        //! registry entry.
        //!
        //! \param hash Recomputed hash of an accrual snapshot file.
        //!
        //! \throws SnapshotHashMismatchError If the supplied hash does not
        //! match the hash recorded in the registry.
        //!
        void AssertHash(const uint256 hash) const
        {
            if (m_snapshot_hash != hash) {
                throw SnapshotHashMismatchError(m_height, m_snapshot_hash, hash);
            }
        }
    };

    //!
    //! \brief Load the snapshot registry from disk and prepare it for use.
    //!
    //! \return \c false if the registry failed to initialize because of an IO
    //! error.
    //!
    bool Initialize();

    //!
    //! \brief Close the registry file.
    //!
    //! \return \c true if the file closed successfully.
    //!
    bool Close();

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
    bool ResetBaseline(const uint64_t height);

    //!
    //! \brief Get the height of the baseline accrual snapshot.
    //!
    //! \return Zero if no baseline snapshot exists yet.
    //!
    uint64_t BaselineHeight() const;

    //!
    //! \brief Get the height of the most recent accrual snapshot.
    //!
    //! \return Zero if no baseline snapshot exists yet.
    //!
    uint64_t LatestHeight() const;

    //!
    //! \brief Get the registry entry for the specified height if it exists.
    //!
    //! \param height Height of a block for a snapshot.
    //!
    //! \return A null pointer if the registry contains no entry for the height.
    //!
    const Entry* TryHeight(const uint64_t height) const;

    //!
    //! \brief Assert that the supplied hash matches the hash in the registry
    //! for the snapshot at the specified height.
    //!
    //! \param height Height of the snapshot to check the hash for.
    //! \param hash   Computed hash of the snapshot to check.
    //!
    //! \throws SnapshotHashMismatchError If the supplied hash does not match
    //! the hash recorded in the registry.
    //!
    void AssertHashMatches(const uint64_t height, const uint256 hash) const;

    //!
    //! \brief Add the height of a new accrual snapshot to the registry.
    //!
    //! \param height Block height of the new snapshot. Usually a superblock.
    //!
    //! \return \c false if the registry failed to store the snapshot context
    //! because of an IO error.
    //!
    bool Register(const uint64_t height, const uint256 snapshot_hash);

    //!
    //! \brief Remove the height of a defunct accrual snapshot from the registry.
    //!
    //! \param height Block height of the snapshot. Usually a superblock.
    //!
    //! \return \c false if the registry failed to store the snapshot context
    //! because of an IO error.
    //!
    bool Deregister(const uint64_t height);

    //!
    //! \brief Serialize the provided data to the registry file.
    //!
    //! \param pch  The bytes to serialize.
    //! \param size Byte length of the data.
    //!
    //! TODO: encapsulate this
    //!
    void write(Span<const std::byte> src);
private:
    //!
    //! \brief Represents a state change for the snapshot registry.
    //!
    enum class Action
    {
        BASELINE,    //!< Set the height of the baseline accrual snapshot.
        REGISTER,    //!< Add a new accrual snapshot.
        DEREGISTER,  //!< Remove an existing accrual snapshot.
    };

    //!
    //! \brief Disk format for a registry entry.
    //!
    class DiskEntry : public Entry
    {
    public:
        Action m_action;

        DiskEntry(const Action action, const Entry& entry)
            : Entry(entry)
            , m_action(action)
        {
        }

        template <typename Stream>
        DiskEntry(deserialize_type, Stream& s) : Entry(0, uint256())
        {
            Unserialize(s);
        }

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            uint8_t action = static_cast<uint8_t>(m_action);
            READWRITE(action);
            m_action = static_cast<Action>(action);

            READWRITE(m_height);
            READWRITE(m_snapshot_hash);
        }
    }; // DiskEntry

    FILE* m_file = nullptr;       //!< Handle of the registry file.
    std::vector<Entry> m_entries; //!< Ordered heights of each snapshot.

    //!
    //! \brief Get the path to the accrual snapshot registry file.
    //!
    static fs::path RegistryPath();

    //!
    //! \brief Truncate the registry file and reopen it for writing.
    //!
    //! \return \c false if an IO error occurred.
    //!
    bool ReopenForWrite();

    //!
    //! \brief Prunes the registry file of any non-current entries and opens it
    //! for writing.
    //!
    //! \return \c false if an IO error occurred.
    //!
    bool Rewrite();

    //!
    //! \brief Write a registry entry to disk.
    //!
    //! \param action Type of registry entry to write.
    //! \param entry  Registry entry to write to disk.
    //!
    //! \return \c false if an IO error occurred.
    //!
    bool WriteEntry(const Action action, const Entry entry);

    //!
    //! \brief Read the snapshot registry file from disk.
    //!
    //! \param file Wraps the registry file to deserialize.
    //!
    void Unserialize(CAutoFile& file);
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
    bool Initialize();

    //!
    //! \brief Destroy all accrual snapshots. This erases the snapshot files on
    //! disk and resets the snapshot registry.
    //!
    //! \return \c false if the snapshot system failed to initialize because of
    //! an error.
    //!
    bool EraseAll();

    //!
    //! \brief Assert that the registry contains an entry for a snapshot at the
    //! specified height and that the snapshot file hash matches.
    //!
    //! \param height Height of the snapshot to check the hash for.
    //!
    //! \throws SnapshotHashMismatchError If the supplied hash does not match
    //! the hash recorded in the registry.
    //! \throws SnapshotMissingError If the registry contains no entry for the
    //! supplied height.
    //!
    void AssertMatch(const uint64_t height) const;

    //!
    //! \brief Clean up extraneous accrual snapshot files.
    //!
    void PruneSnapshotFiles() const;

    //!
    //! \brief Determine whether the node already stored a baseline accrual
    //! snapshot.
    //!
    //! \return \c true if the node previously activated the accrual snapshot
    //! system.
    //!
    bool HasBaseline() const;

    //!
    //! \brief Store a snapshot of accrual for each account as the baseline.
    //!
    //! \param height   Height of the block to associate with the snapshot.
    //! \param accounts Research accounts to record accrual from.
    //!
    //! \return \c false when an error occurs while creating a snapshot.
    //!
    bool StoreBaseline(const uint64_t height, const ResearchAccountMap& accounts);

    //!
    //! \brief Store a snapshot of accrual for each account to disk.
    //!
    //! \param height   Height of the block to associate with the snapshot.
    //! \param accounts Research accounts to record accrual from.
    //!
    //! \return \c false when an error occurs while creating a snapshot.
    //!
    bool Store(const uint64_t height, const ResearchAccountMap& accounts);

    //!
    //! \brief Load the most recent accrual snapshot for each account.
    //!
    //! \param accounts Research accounts to apply snapshot accrual to.
    //!
    //! \return \c false when an error occurs while loading a snapshot.
    //!
    //! \throws SnapshotHashMismatchError If the hash of the disk snapshot does
    //! not match the hash recorded in the registry.
    //!
    bool ApplyLatest(ResearchAccountMap& accounts) const;

    //!
    //! \brief Load the specified accrual snapshot for each account.
    //!
    //! \param height   Block height of the snapshot. Usually a superblock.
    //! \param accounts Research accounts to apply snapshot accrual to.
    //!
    //! \return \c false when an error occurs while loading a snapshot.
    //!
    //! \throws SnapshotHashMismatchError If the hash of the disk snapshot does
    //! not match the hash recorded in the registry.
    //!
    bool Apply(const uint64_t height, ResearchAccountMap& accounts) const;

    //!
    //! \brief Erase the specified accrual snapshot.
    //!
    //! \param height Block height of the snapshot. Usually a superblock.
    //!
    //! \return \c false when an error occurs while removing a snapshot.
    //!
    bool Drop(const uint64_t height);

    //!
    //! \brief Drop every snapshot whose height is strictly greater than the
    //! supplied chain tip. Used by Tally::ActivateSnapshotAccrual to reconcile
    //! the registry with a chain tip that has rewound below the latest
    //! snapshot -- typically after a Phase 2 abandonment-style recovery
    //! (issue #2865) where pindexBest jumps backward without going through
    //! the normal DisconnectBlock path. Without this pass, the registry's
    //! strict-monotonic Register invariant would assert on the first SB the
    //! forward sync re-crosses, killing the wallet on startup.
    //!
    //! Implementation walks the registry's LIFO Deregister path, popping the
    //! latest entry each iteration. Bounded by the SB-cross count of the
    //! rewind -- at most a few entries in any realistic recovery (the
    //! -coherencewalkmax default of 10000 blocks is well under ten SBs on
    //! mainnet, around ten on testnet). PruneSnapshotFiles() should be
    //! called after this to clean up the orphaned snapshot files on disk.
    //!
    //! \param tip_height Current chain tip height. Snapshots strictly above
    //!                   this are dropped.
    //! \return Count of snapshots dropped, or -1 if a Drop call failed
    //!         (registry is then in a partially-trimmed state; the caller
    //!         should fall through to RebuildAccrualSnapshots()).
    //!
    int DropAboveHeight(const uint64_t tip_height);

    bool CloseRegistryFile();

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
    SnapshotBaselineBuilder(ResearchAccountMap& researchers);

    //!
    //! \brief Scan the chain to establish the baseline delta snapshot accrual
    //! for each CPID in the network and apply it to the research accounts.
    //!
    //! \param pindex             Block to establish the accrual baseline from.
    //! \param current_superblock Baseline starts from before this superblock.
    //!
    //! \return \c false if an error occurs while processing historical accrual.
    //!
    bool Run(const CBlockIndex* pindex, const SuperblockPtr current_superblock);

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
        const CBlockIndex* const pindex_bind = nullptr);

    //!
    //! \brief Apply the accrual earned for the current superblock to the total
    //! accrual for each of the CPIDs that it contains.
    //!
    //! \param payment_time Timestamp of the end of the accrual period.
    //!
    void TallyAccrual(const int64_t payment_time);
}; // SnapshotBaselineBuilder
} // namespace GRC

#endif // GRIDCOIN_ACCRUAL_SNAPSHOT_H
