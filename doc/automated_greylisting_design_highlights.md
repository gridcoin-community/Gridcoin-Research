# Automated Greylisting Design Highlights

## Executive Summary

This document provides the highlights for the functionality included in Gridcoin PR [scraper, project, superblock: Implement automated greylisting by jamescowens · Pull Request #2778 · gridcoin-community/Gridcoin-Research · GitHub](https://github.com/gridcoin-community/Gridcoin-Research/pull/2778), which implements both manual and automated greylisting. The Gridcoin network rules for project whitelisting and the conditions for greylisting are documented on the main Gridcoin website at [Whitelist Process](https://gridcoin.us/wiki/whitelist-process.html).

When projects temporarily do not meet the requirements for whitelisting, the main two rules of which are a Work Availability Score (WAS) of less than 0.1, and/or a Zero Credit Days (ZCD) count of greater than 7, the project is "greylisted". Traditionally, from a network operations perspective, this has meant temporarily removing the project from the whitelist using the administrative protocol update procedures, and then re-adding the project when the two rules return to normal. This is a manual and labor intensive process that does not scale well as the number of whitelisted projects increase, and it depends on the administrator to take action in a timely manner.

To address the scalability and administrative stablility, it was recognized several years ago that a form of automatic greylisting would be important functionality to implement. This PR addresses that functionality need.

This PR implements *manual greylisting* via a new project entry status *MAN_GREYLISTED*. This is set by administrative contract just like whitelisting. The purpose of the manual listing is twofold: 1) Not all possible issues that could result in a need to greylist are covered by the WAS and ZCD rules, and 2) a long term low level availability of a project at a fairly consistent level for more than 40 days will cause the WAS to pass, because both the numerator and denominator forming WAS will see similar results. This project status is stored in the project registry.

*Automatic greylisting* is implemented via a new AutoGreylist class. This class essentially records total credit data across the entire project (all CPIDs, whether they have an active beacon or not) collected from the scrapers via a pending, or existing (last) superblock for all whitelisted projects, and the history of 40 superblocks back from the pending or last superblock, and evaluates the WAS and ZCD rules. Because it operates at the granularity of the superblocks, technically the definition of these rules is *slightly* different than the documentation since the time between superblocks is **slightly** more than 24 hours. In practice this is essentially equivalent. Because the automatic greylisting status is aligned along superblock boundaries, the AutoGreylist class is implemented as a caching singleton, so that repeated calls to the class for the greylist simply report the cached state rather than doing the heavyweight walk of 40 superblocks to recompute the automatic greylist state from the superblock history as long as the referenced superblock hash for the cache has not changed.

The AutoGreylist class *overrides* a project registry status of ACTIVE or MAN_GREYLISTED with the status of AUTO_GREYLISTED if it meets greylisting criteria. This is done directly in the Whitelist::Snapshot method, preserving the underlying project entries. Since the underlying registry entries are preserved, the project state returns to the underlying status as soon as the greylist entry for that project returns to normal. Conversely there is also the ability to set via addkey the project status AUTO_GREYLIST_OVERRIDE in the project registry, which takes precedence over the automatic greylisting. This allows an override to keep a project active even if the automated greylist rules would greylist it. A good example of this would be a project that had a one day correction of TC due to a database issue that distorts the WAS, causing a false failure. In that case it would be a good idea to override the automatic greylisting for that project temporarily after evaluation by the community.

Note that the auto greylisting ruleset is entirely contained within the AutoGreylist class. While the rules are encapsulated in that class, no attempt to write a formal rules engine was done as that would be too heavyweight for just two rules. Additionally, WAS has been implemented using 64 bit integer arithmetic and the Gridcoin Fraction class to avoid consensus issues due to floating point. For ease of display, WAS values in reports are shown as floating point.

The *excluded project* functionality of the scraper convergence and the associated superblocks formed still applies. This means that a project that does not export statistics at all in a 48 hour period will be excluded from superblocks until access to project statistics are restored. This is related to the 48 hour statistics retention rule for scraper statistics that has been in place since the scraper rewrite a number of years ago. When a project is *excluded* this is effectively an override of all project registry statuses and the operation of the automated greylisting.

## Changes to the scraper

The scraper was modified to collect total credit across the entire project for each project that does not have a status of deleted in the project registry. This was accomplished via the following changes (this is not all inclusive):

1. Addition of two fields in the scraper file manifest registry, All_cpid_total_credit and No_records. All_cpid_total_credit captures the total credit summation across the entire project regardless of whether the cpid is active or not. No_records is a flag to record when a file has no active cpids. Formerly, a file and its corresponding entry would have been deleted, but now we must retain the file and entry to record the all_cpid_total_credit even if there are no current active cpids in that project.

2. Implementation of an additional pseudo-project entry in the CScraperManifest with the name ProjectsAllCpidTotalCredits to allow convergence to be calculated by each node from the scraper data. This means the total credits data must also match just like other project data for a convergence to occur and insures integrity of the total credits data similar to other project data.

3. Extension of the ScraperStatsVerifiedBeacons structure used in the convergence to include total credits, and renamed ScraperStatsVerifiedBeaconsTotalCredits. This is the primary method of transferrance of state to the superblock of the total credits for each project from a manifest convergence.

4. Modification of the scraper machinery to assign zero magnitude to greylisted projects for purposes of magnitude calculation.

5. Modification of the ProcessProjectRacFileByCPID function to accumulate total credit for a project from all CPIDs regardless of status.

6. Reporting for CScraperManifest and the convergence report rpc functions were extended to provide information on the total credits across all projects.

## Changes to the registry_db.h template

The registry db template provides generic programming common code underlying the registry implementations for each of the contract types that require historized, revertable state maintenance of their corresponding objects. This implements versioned state storage of registered contract type objects via the defined key in leveldb. The AutoGreylist class needs to know when the first entry actually occurred for a project to properly apply the rules for projects whitelisted within the 40 superblock lookback scope. As a result, the registry db template had to be extended to include a generic type and code to accomodate this.

Each of the corresponding contract type classes had to be modified to accomodate changes in the registry template, even if they did not actually use the first occurance functionality, i.e. trivial modifications to all other contract types besides project.

## Changes to the superblock

The superblock class version was incremented to v3 and was extended to include the m_projects_all_cpids_total_credits map which contains the total credits data for all projects, and which is from the ScraperStatsVerifiedBeaconsTotalCredits structure in the manifest convergence from the scraper. This data is serialized and is stored on the blockchain when the superblock is staked and in turn is used by the AutoGreylist class for greylist status calculations.

The superblock also was extended to store projects that have been greylisted.

## Implementation of the AutoGreylist class and changes to the project class and project registry (whitelist)

### ProjectEntryStatus enum class changes

The ProjectEntryStatus num class was extended and moved to fwd.h to avoid recursive include problems.

```
{
//!
//! \brief Enumeration of project entry status. Unlike beacons this is for both storage
//! and memory.
//!
//! UNKNOWN status is only encountered in trivially constructed empty
//! project entries and should never be seen on the blockchain.
//!
//! DELETED status corresponds to a removed entry.
//!
//! ACTIVE corresponds to an active entry.
//!
//! GREYLISTED means that the project temporarily does not meet the whitelist qualification criteria.
//!
//! OUT_OF_BOUND must go at the end and be retained for the EnumBytes wrapper.
//!
enum class ProjectEntryStatus
{
    UNKNOWN,
    DELETED,
    ACTIVE,
    MAN_GREYLISTED,
    AUTO_GREYLISTED,
    AUTO_GREYLIST_OVERRIDE,
    OUT_OF_BOUND
};

```

MAN_GREYLISTED, AUTO_GREYLISTED, AUTO_GREYLIST_OVERRIDE are new states. The order of enum entries for the extending states was not changed to avoid serialization issues with older project entries.

### Implementation of project filter for whitelist snapshots

A ProjectFilterFlag was implemented to accomplish easy filtering of the whitelist snapshot depending on intended use.

```
    //!
    //! \brief Project filter flag enumeration.
    //!
    //! This controls what project entries by status are in the project whitelist snapshot. Note that REG_ACTIVE
    //! is the original "ACTIVE" and represents project entries with a status of "ACTIVE" in the registry. The
    //! filter flag "ACTIVE" here includes both REG_ACTIVE and AUTO_GREYLIST_OVERRIDE project entry statuses from
    //! the registry, since both mean the project is active assuming a convergence can be formed.
    //!
    enum ProjectFilterFlag : uint8_t {
        NONE                   = 0b00000,
        DELETED                = 0b00001,
        MAN_GREYLISTED         = 0b00010,
        AUTO_GREYLISTED        = 0b00100,
        GREYLISTED             = MAN_GREYLISTED | AUTO_GREYLISTED,
        REG_ACTIVE             = 0b01000,
        AUTO_GREYLIST_OVERRIDE = 0b10000,
        ACTIVE                 = REG_ACTIVE | AUTO_GREYLIST_OVERRIDE,
        NOT_ACTIVE             = 0b00111,
        ALL_BUT_DELETED        = 0b11110,
        ALL                    = 0b11111
    };

```
Note that the ACTIVE enum value is actually a combination of the original ACTIVE (now labled REG_ACTIVE, which is short for registry active) and AUTO_GREYLIST_OVERRIDE, since a project status of AUTO_GREYLIST_OVERRIDE means that the project is not only active, but overrides any determination by the automatic greylisting.

### ProjectEntry class modifications

The ProjectEntry class current version has been incremented to v4 and now includes a requires_ext_adapter boolean to replace the temporary protocol entry based approach to show this status in the GUI. This boolean is serialized and the serialization is conditioned on the version to ensure compatibility with older project records.

### AutoGreylist class implementation

#### GreylistCandidateEntry

The GreylistCandidateEntry is a class implemented within the AutoGreylist class that formalizes the greylist state maintenance and state history for each project in the whitelist filtered by ALL_BUT_DELETED (i.e. all but deleted). This class uses a "reverse" bookmark based approach to effectively deal with a number of tricky situations involving lack of availability of project total credit data (drop-outs). In addition, each update is stored in the m_update_history vector to provide visibility of the historical evolution of the total credit data and greylist status according to the rules.

The GreylistCandidateEntry contains an "empty" constructor and a parameterized constructor, the latter of which both creates the GreylistCandidateEntry and establishes the baseline for the measurements.

##### uint8_t GetZCD()

This method simply returns the m_zcd_20_SB_count member variable, which is a count of the number of zero credit days in the 20 superblock lookback from the baseline.

##### Fraction GetWAS()

The method computes the average total credit over a 7 superblock lookback and a 40  superblock lookback and then constructs a Fraction of the result. Note that if the lookback is less than 40, the number of superblocks in the average is reduced for the 40 superblock lookback and similarly if the lookback is less than 7, the number of superblocks in the average is reduced for the 7 superblock lookback. Given that when data is first being collected for a newly listed project, this can lead to odd behavior of WAS, there is a grace period implemented in the application of the rules for setting the m_meets_greylisting_crit flag in the GreylistCandidateEntry. This grace period is currently set at 7 superblocks.

##### void UpdateGreylistCandidateEntry(std::optional<uint64_t> total_credit, uint8_t sb_from_baseline)

This method is used by the RefreshWithSuperblock method to update each GreylistCandidateEntry and add each update to the entry history.

##### struct UpdateHistoryEntry

This is the struct that stores the greylist state at the given update for the greylist candidate. *Note that this is the history viewed BACKWARDS as a lookback from the current state, not forwards looking, so this can be misleading if you do not understand that*. Each time the AutoGreylist class is updated due to the current superblock hash changing, the historical entries will be rebuilt from the current superblock backwards. This struct contains most of its member variables as std::optionals to accomodate the lack of information at a particular update.

##### const std::vector<UpdateHistoryEntry> GetUpdateHistory() const

This is a getter that returns a constant version of m_update_history.

##### Public member variables

```
        const std::string m_project_name;

        uint8_t m_zcd_20_SB_count;
        uint64_t m_TC_7_SB_sum;
        uint64_t m_TC_40_SB_sum;
        bool m_meets_greylisting_crit;

```

The m_project_name contains the project name, which is the key to the greylist map. The next three contain the undertying state with which to compute the ZCD and WAS but since they are public, they can be independently accessed. The m_meets_greylisting_crit stores the current greylist qualification state for the entry. If it is true, the project currently meets automatic greylisting criteria.

##### Private member variables

```
        std::optional<uint64_t> m_TC_initial_bookmark; //!< This is a "reverse" bookmark - we are going backwards in SB's.
        std::optional<uint64_t> m_TC_bookmark;
        uint8_t m_sb_from_baseline_processed;

        std::vector<UpdateHistoryEntry> m_update_history;

```

These store the bookmarks (which are for internal use only) and the m_update_history vector, which is accessed via GetUpdateHistory().

#### The Greylist map

```
    typedef std::map<std::string, GreylistCandidateEntry> Greylist;

    //!
    //! \brief Smart pointer around a collection of projects.
    //!
    typedef std::shared_ptr<Greylist> GreylistPtr;

```

The actual greylist entries are collected into a std::map keyed by project name. This in turn is wrapped by a shared_ptr.

#### AutoGreylist iterator overloads

Similar to other registry and registry like classes in Gridcoin, the AutoGreylist contains iterator overloads to allow accessing the AutoGreylist map using range loops and other iterator like uses.

#### void Refresh()

This refreshes the AutoGreylist object from the current superblock. The cached state is used if the superblock hash has not changed to reduce overhead.

#### void RefreshWithSuperblock(SuperblockPtr superblock_ptr_in, std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>> unit_test_blocks = nullptr)

This refreshes the AutoGreylist object from an input Superblock pointer. It contains a second parameter that provides an alternate way to input test superblocks for unit testing.

#### void AutoGreylist::RefreshWithSuperblock(Superblock& superblock)

This refreshes the AutoGreylist object from an input Superblock that is going to be associated with the current head of the chain (i.e. a stake). This mode is used in the scraper during the construction of the superblock contract as part of the call chain from the miner. The superblock object will be updated with the greylist status. This is critical distinction. The other two forms simply use the superblocks on the chain, while this form is freezing the state into the provided superblock object as well as doing the historical lookback.

#### void Reset()

Resets the AutoGreylist object. This is called from the Whitelist Reset().

#### Private members

```
    mutable CCriticalSection autogreylist_lock;

    GreylistPtr m_greylist_ptr;
    QuorumHash m_superblock_hash;
```

The autogreylist_lock is an internal critical section to ensure thread safety, since the AutoGreylist singleton can be accessed by multiple threads. The m_greylist_ptr is the shared smart pointer to the actual greylist map, and the m_superblock_hash stores the hash of the superblock used for the last AutoGreylist update and is used to detect a state change, otherwise the cached information is used.

### Changes to Whitelist (Project Registry) class

#### Change to WhitelistSnapshot Snapshot method

This method has been extended to take the Project Filter as an argument, defaulting to ACTIVE, and also the refresh_greylist boolean defautling to true, and the include_override boolean defaulting to true. This method implements the AUTO_GREYLISTED override of project status when the corresponding AutoGreylist greylist candidate entry meets greylisting criteria according to the rules.

#### const ProjectEntryMap GetProjectsFirstActive() const

This is a new method that provides a map of the first entry for each project in the registry. This is used by the AutoGreylist class to determine abbreviated lookbacks for projects that were whitelisted within the 40 superblock lookback window.

#### std::shared_ptr<AutoGreylist> GetAutoGreylist()

This is a new method that returns a shared smart pointer to the AutoGreylist object. This object is a singleton just like the Whitelist registry.

#### New private members

ProjectEntryMap m_project_first_actives stores the first (active) entry for each project keyed by project name, and is returned read-only by GetProjectsFirstActive(). This map is populated by the registry contract handlers and leveldb initialization method. The std::shared_ptr<AutoGreylist> m_auto_greylist is the smart shared pointer to the AutoGreylist object.

### Change to the WhitelistSnapshot class

The constructor of this class was changed to accept the project filter used as a parameter, which is stored for convenience in the WhitelistSnapshot object.

## Quorum changes

The QuorumHash ComputeQuorumHash() was extended (implicitly) to include the project all cpid total credit data as part of the superblock hash. The superblock version is validated to ensure that no superblocks less than v4 are accepted once the superblock v4 block height has been reached.

## GUI - ResearcherModel and ProjectTableModel changes

The researcher and project table models were extended to deal appropriately with auto greylisted status, following the order of precedence. In the project table displayed in the GUi, automatic greylisting status and manual greylisting status takes precedence over excluded, because if a project has those statuses, it has the same effect as exclusion, but is not a scraper directive.

## RPC changes

### UniValue SuperblockToJson(const GRC::Superblock& superblock)

This helper function that provides JSON superblock outputs to several different RPC functions has been modified to include the project greylist status and the project all CPID total credits.

### UniValue addkey(const UniValue& params, bool fHelp)

This administrative function has been extended to handle manual greylisting and the automatic greylisting override.

## Unit Tests

### superblock_tests.cpp

The superblock tests were modified to use version two in the superblock tests. A todo would be to change them for the new structures in v3 to fully test serialization and deserialization, but this has been covered in the isolated testnet live network test.

### project_tests.cpp

A unit test that uses the std::pair<CBlockIndex*, SuperblockPtr>>> unit_test_blocks parameter of the AutoGreylist::RefreshWithSuperblock method has been implemented with 47 superblocks of test data to test the operation of the greylisting rules. These superblocks exercise every real-world condition expected to be encountered that can be solved with automatic application of the implemented automatic greylisting rules, including no statistics on the first superblock after whitelisting, statistics "drop outs" with no data or no increase in total credit and/or both (i.e. a total credit number then no data then another total credit number that is the same), and a drastic drop in total credit change per superblock that results in WAS meeting greylisting criteria. More superblocks than the lookback limit was tested to ensure the lookback stopped at the appropriate place.

