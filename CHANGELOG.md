# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [5.3.0.0] 2021-03-16, mandatory
### Fixed
 - consensus, accrual: Fix accrual post hard-fork at 2197000 #2053 (@jamescowens, @div72, @cyrossignol)

## [5.2.2.0] 2021-03-14, leisure
### Fixed
 - beacon, contracts: Fix sync from zero issue due to ApplyContracts problem in 5.2.1.0 #2047 (@jamescowens)

## [5.2.1.0] 2021-03-07, leisure
### Added
 - voting: Add wait warning to voting tab loading message #2039 (@cyrossignol)
 - rpc: Adds transaction hash and fees paid to consolidateunspent #2040 (@jamescowens)

### Changed
- gui, voting: Make some minor adjustments for VotingDialog flow #2041 (@jamescowens)

### Fixed
 - beacon, util, gui: Fix small error in beacon db for renewals and fix snapshot download functionality #2036 (@jamescowens)

## [5.2.0.0] 2021-03-01, mandatory, "Hilda"
### Added
 - gui: Add RAC column to wizard summary page projects table #1951 (@cyrossignol)
 - rpc: clean up the superblocks function and add magnitude to getmininginfo #1966 (@jamescowens)
 - rpc: Add transaction size to RPC output #1971 (@cyrossignol)
 - voting: Add user-facing support for poll response types #1976 (@cyrossignol)
 - gui: Port Bitcoin Intro class (implement the ability to choose a data directory via the GUI) #1978 (@jamescowens)
 - gui: Port Bitcoin MacOS app nap manager #1991 (@jamescowens)
 - mining, rpc: Implement staking efficiency measure and improve SelectCoinsForStaking and CreateCoinStake #1992 (@jamescowens)
 - accrual, rpc: Implement auditsnapshotaccruals #2001 (@jamescowens)
 - docs: add doxygen support #2000 (@div72)
 - beacon: Specialized beacon storage in leveldb #2009 (@jamescowens)
 - rpc: Add a call to dump contracts in binary form #2011 (@div72)
 - rpc: Add boolean option to report active beacons only in beaconreport #2013 (@jamescowens)
 - consensus: Set Hilda mainnet hardfork height to 2197000 #2022 (@jamescowens)

### Changed
 - refactor: [Memory optimization] Block index duplicate PoS state #1945 (@cyrossignol)
 - refactor: [Memory optimization] Block index superblock and contract flags #1950 (@cyrossignol)
 - refactor: [Memory optimization] Remove stake modifier checksums #1954 (@cyrossignol)
 - refactor: [Memory optimization] Block index allocation overhead #1957 (@cyrossignol)
 - refactor: [Memory optimization] Remove block index subsidy fields #1960 (@cyrossignol)
 - refactor: [Memory optimization] Separate chain trust from the block index #1961 (@cyrossignol)
 - refactor: [Memory optimization] Eliminate padding between block index fields #1962 (@cyrossignol)
 - beacon, gui: Add check for presence of beacon private key to updateBeacon() #1968 (@jamescowens)
 - util: Enhance ETTS calculation #1973 (@jamescowens)
 - refactor: Use new clamp in util.h #1975 (@jamescowens)
 - gui: Redo global status for overview #1983 (@jamescowens)
 - util: Improvements to MilliTimer class and use in the miner and init #1987 (@jamescowens)
 - rpc: Move rpc files to directory #1995 (@Pythonix)
 - rpc: Enhance consolidateunspent and fix fee calculation #1994 (@jamescowens)
 - contract: Double the lookback scope of contract replay #1998 (@jamescowens)
 - net: Don't rely on external IP resolvers #2002 (@Tetrix42)
 - beacon: Change beacon map to pointers #2008 (@jamescowens)
 - gui: Update bitcoin_sv.ts #2014 (@sweede-se)
 - util: Update snapshot URLs and add accrual directory #2019 (@jamescowens)
 - beacon: Tweak BeaconRegistry::Revert #2020 (@jamescowens)
 - rpc, qt: bump fees @2023 (@div72)

### Removed
 - researcher: Remove automatic legacy beacon key import #1963 (@cyrossignol)
 - util: Revert "Close LevelDB after loading the block index" #1969 (@cyrossignol)
 - ci: Fix python symlink issue & remove travis #1990 (@div72)
 - ci: remove python workaround #2005 (@div72)

### Fixed
 - gui: fix mandatory/leisure detection of upgrade check #1959 (@Pythonix)
 - voting: Fix title in "gettransaction" RPC for legacy poll contracts @1970 (@cyrossignol)
 - gui: Fix missing menu items on macOS #1972 (@scribblemaniac)
 - rpc: Fix answer offset in "votedetails" #1974 (@cyrossignol)
 - voting: Implement missing try-catch in VotingVoteDialog::vote #1980 (@jamescowens)
 - scraper: Add check for minimum housekeeping complete in scraper #1977 (@jamescowens)
 - voting: Fix nonsense vote weights for legacy polls #1988 (@cyrossignol)
 - voting: Fix incorrect field returned in ResolveMoneySupplyForPoll() #1989 (@cyrossignol)
 - consensus, accrual: Fix newbie accrual #2004 (@jamescowens)
 - log: grammar correction #2016 (@nathanielcwm)
 - wallet: Correct nMinFee fee calculation in CreateTransaction #2021 (@jamescowens)
 - rpc, miner: Correct GetLastStake #2026 (@jamescowens)
 - wallet: Fix bug in CreateTransaction causing insufficient fees #2029 (@jamescowens)

## [5.1.0.0] 2020-11-01, mandatory, "Gladys"
### Added
 - rpc: Add out-of-sync status to "getinfo" and "getblockchaininfo" #1925 (@cyrossignol)
 - gui: add autocomplete to rpc console #1927 (@Pythonix)
 - consensus: Add checkpoint post block v11 transition #1919 (@cyrossignol)
 - researcher: Add -forcecpid configuration option #1935 (@cyrossignol)
 - gui: Adds detection if version is below last mandatory #1939 (@jamescowens)
 - contract: Reimplement legacy administrative contract validation #1943 (@cyrossignol)
 - voting: Add poll choices to "gettransaction" RPC contract output #1948 (@cyrossignol)
 
### Changed
 - doc: Fix link in build-openbsd.md #1924 (@Pythonix)
 - voting: Decrease poll duration to 90 days #1936 (@cyrossignol)
 - refactor: Revert init order to fix rejected net messages @1941 (@cyrossignol)
 - refactor: port amount.h #1937 (@div72)
 - refactor: Normalize boost::filesystem to fs namespace #1942 (@cyrossignol)
 - accrual: Apply accrual for new CPIDs from existing snapshots #1944 (@cyrossignol)
 - accrual: Reset research account when disconnecting first block #1947 (@cyrossignol)

### Removed
 - refactor: Clean up transitional code for block version 11 #1933 (@cyrossignol)

### Fixed
 - Modify depends packages for openSUSE and other Redhat like distributions and fix mingw bdb53 compile #1932 (@jamescowens)
 - contract: Fix ability to reorganize contracts #1934 (@cyrossignol)
 - accrual: Fix snapshot accrual for new CPIDs #1931 (@cyrossignol)
 - rpc: Clean up getblockstats #1938 (@jamescowens)
 - scraper, rpc: Correct missing mScraperStats initialization in ConvergedScraperStats (@jamescowens)

## [5.0.2.0] 2020-10-08, leisure
### Added
 - trivial: Add and update copyright headers in Gridcoin files #1897 (@cyrossignol)
 - refactor: port chainparams #1878 (@div72)
 - gui: Update default font to Inter-Regular and console font to Inconsolata (@opsinphark, @jamescowens)
 - gui: Add "review beacon verification" button to wizard summary page #1912 (@cyrossignol)
 - rpc, wallet: Implement liststakes #1909 (@jamescowens)
 - rpc: Add "getlaststake" RPC function #1913 (@cyrossignol)
 - gui: Install bold variant of Inter font #1914 (@cyrossignol)

### Changed
 - refactor: Consolidate Gridcoin-specific code #1894 (@cyrossignol)
 - script: Setup improvements #1895 (@nathanielcwm)
 - gui: Diagnostics refresh #1899 (@jamescowens)
 - superblock: Optimize superblock size calculation #1906 (@cyrossignol)
 - gui: Adjust stylesheets and scale icons to improve HiDPI side toolbar display #1911 (@jamescowens)
 - doc: Tell user to disable win32 application support in WSL (for building) #1917 (@nathanielcwm)
 - rpc: Revise and expand help for beaconconvergence rpc call #1918 (@jamescowens)
 - scheduler: Increase default update check interval to 5 days #1920 (@cyrossignol)
 - gui: Prevent multiple dialogs from being open at the same time #1922 (@scribblemaniac)

### Removed
 - refactor: Clean up remaining legacy timer code #1892 (@cyrossignol)

### Fixed
 - build: Add --without-brotli option to curl.mk #1902 (@G_UK)
 - test: Remove fs_tests... file after the fs test #1903 (@div72)
 - util, gui: Fix shutdown segfault and repair broken overview page staking status #1901 (@jamescowens)
 - scraper: Fix order of destruction for global scraper objects #1904 (@cyrossignol)
 - scraper: Fix global object destruction order for MacOS #1905 (@cyrossignol)
 - util: Decouple out-of-sync state from block acceptance #1921 (@cyrossignol)

## [5.0.1.0] 2020-09-20, leisure
### Added
 - wallet, rpc: Implement backup file management functionality #1735 (@jamescowens)
 - build: Add support for building with musl and Alpine Linux #1866 (@cyrossignol)
 - rpc: Display local IP addresses in "getnetworkinfo" output #1884 (@cyrossignol)

### Changed
 - refactor: Implement std::atomic_bool OutOfSyncByAge #1877 (@jamescowens)
 - net: Optimize locator construction for "getblocks" messages #1880 (@cyrossignol)
 - refactor: Combine GetOrphanRoot() and WantedByOrphan() functions #1883 (@cyrossignol)
 - refactor: Convert beacon and backup timers to scheduled jobs #1885 (@cyrossignol, @jamescowens)
 - refactor: Rename "neural network project-wide #1886 (@cyrossignol)

### Fixed
 - collection of post Fern hotfixes (@jamescowens):
   - Change QDateTime::toSecsSinceEpoch() to QDateTime::toMSecsSinceEpoch()
   - Change QDateTime::fromSecsSinceEpoch() to QDateTime::fromMSecsSinceEpoch()
   - Ensure boost placeholders are compatible
   - Fix subtle bug in GetEstimatedStakingFrequency
 - test, ci: xenial support #1867 (@div72)
 - lib: Fix compatibility with Boost 1.74 #1869 (@theMarix)
 - test: Fix tests for _GLIBCXX_ASSERTIONS #1870 (@cyrossignol)
 - util: fix Windows API for default data directory with wide characters #1871 (@cyrossignol)
 - gui: Fix OP_RETURN filter to avoid hiding transactions with messages #1873 (@cyrossignol)
 - net: Fix stalled blockchain progression #1876 (@cyrossignol)
 - superblock: Fix regression for superblock builder optimization #1881 (@cyrossignol)
 - util: Fix scheduler crash after waking from sleep #1888 (@cyrossignol)

## [5.0.0.0] 2020-09-03, mandatory, "Fern"
### Added
 - Backport newer uint256 types from Bitcoin #1570 (@cyrossignol)
 - Implement project level rain for rainbymagnitude #1580 (@jamescowens)
 - Upgrade utilities (Update checker and snapshot downloader/application) #1576 (@iFoggz)
 - Provide fees collected in the block by the miner #1601 (@iFoggz)
 - Add support for generating legacy superblocks from scraper stats #1603 (@cyrossignol)
 - Port of the Bitcoin Logger to Gridcoin #1600 (@jamescowens)
 - Implement zapwallettxes #1605 (@jamescowens)
 - Implements a global event filter to suppress help question mark #1609 (@jamescowens)
 - Add next target difficulty to RPC output #1615 (@cyrossignol)
 - Add caching for block hashes to CBlock #1624 (@cyrossignol)
 - Make toolbars and tray icon red for testnet #1637 (@jamescowens)
 - Add an rpc call convergencereport #1643 (@jamescowens)
 - Implement newline filter on config file read in #1645 (@jamescowens)
 - Implement beacon status icon/button #1646 (@jamescowens)
 - Add gridcointestnet.png #1649 (@caraka)
 - Add precision to support magnitudes less than 1 #1651 (@cyrossignol)
 - Replace research accrual calculations with superblock snapshots #1657 (@cyrossignol)
 - Publish example gridcoinresearch.conf as a md document to the doc directory #1662 (@jamescowens)
 - Add options checkbox to disable transaction notifications #1666 (@jamescowens)
 - Add support for self-service beacon deletion #1695 (@cyrossignol)
 - Add support for type-specific contract fee amounts #1698 (@cyrossignol)
 - Add verifiedbeaconreport and pendingbeaconreport #1696 (@jamescowens)
 - Add preliminary testing option for block v11 height on testnet #1706 (@cyrossignol)
 - Add verified beacons manifest part to superblock validator #1711 (@cyrossignol)
 - Implement beacon, vote, and superblock display categories/icons in UI transaction model #1717 (@jamescowens)
 - neuralnet: Add integrity checking to researcher accrual snapshot registry #1727 (@jamescowens)
 - Add workaround for scrypt assembly on macOS #1740 (@cyrossignol)
 - gui: Build onboarding/beacon wizard #1739 (@cyrossignol)
 - doc: Add CONTRIBUTING.md from bitcoin #1723 (@div72)
 - rpc: Implement inspectaccrualsnapshot and parseaccrualsnapshotfile #1744 (@jamescowens)
 - scraper: Add disk based state backing for verified beacon list in scraper #1751 (@jamescowens)
 - Add ability to recover beacon in block version 11+ #1768 (@cyrossignol)
 - refactor: Add transaction context to contract handlers #1777 (@cyrossignol)
 - gui: Add context for when BOINC is attached to a pool #1775 (@cyrossignol)
 - doc: Clarify what to do if PR in multiple categories (for CONTRIBUTING.md) #1798 (@RoboticMind)
 - qt: Add option to choose not to start the wallet minimized #1804 (@jamescowens)
 - superblock: Add check for OutOfSyncByAge to SuperblockValidator::Validate #1806 (@jamescowens)
 - contract: Standardize contract validation and add block context #1808 (@cyrossignol)
 - add seed.gridcoin.pl to default config #1812 (@wilkart)
 - gui: Implement sidestake send display #1813 (@jamescowens)
 - gui: Add pool/investor pages to researcher wizard #1819 (@cyrossignol)
 - ci: Port lint scripts from Bitcoin #1823 (@div72)
 - doc: Create basic readme in contrib #1826 (@RoboticMind)
 - gui: Implement TransactionRecord::Message #1829 (@jamescowens)
 - rpc: Add private_key_available to beaconstatus #1833 (@a123b)
 - gui: Validate email address in researcher wizard #1840 (@a123b)
 - rpc: Add "getrawwallettransaction" RPC function #1842 (@cyrossignol)
 - consensus: Set block version 11 threshold height for mainnet #1862 (@cyrossignol)

### Changed
 - Upgrade LevelDB from v1.17 to v1.20 #1562 (@cyrossignol)
 - Re-enable scrypt optimizations #1450 (@denravonska)
 - Derive CScript from prevector type (optimization) #1554 (@cyrossignol)
 - Disable quorum for grandfathered blocks to speed up sync #1568 (@cyrossignol)
 - Refactor hashBoinc for binary claim contexts #1558 (@cyrossignol)
 - integrated_scraper_2 branch tracking PR #1559 (@jamescowens)
 - Upgrade depends  - OpenSSL to 1.1.1d #1581 (@jamescowens)
 - Ubuntu 19.10 fixes #1590 (@denravonska)
 - Force a re-parse of legacy claims in generated blocks #1592 (@cyrossignol)
 - Improve the "versionreport" RPC output #1595 (@cyrossignol)
 - Overhaul the core tally and accrual system #1583 (@cyrossignol)
 - Overhaul the superblock quorum system #1597 (@cyrossignol)
 - Add more data to the "superblocks" RPC output #1599 (@cyrossignol)
 - Update Windows Build doc #1606 (@barton2526)
 - Change the order of calls in gridcoinresearchd.cpp to optimize rpc shunt path #1610 (@jamescowens)
 - Change staking tooltip to display frequency #1611 (@jamescowens)
 - Enhancements to ETTS #1442 (@jamescowens)
 - Standardize money values as integers #1614 (@cyrossignol)
 - Clean up and optimize legacy coin age code #1616 (@cyrossignol)
 - Some scraper cleanups #1620 (@jamescowens)
 - Reorganize accrual code and fix 6-month cutoff #1630 (@cyrossignol)
 - Update Copyright years #1633 (@barton2526)
 - Change team whitelist delimiter to <> for CPID detection #1634 (@cyrossignol)
 - Change team whitelist separator to <> to accomodate more team names #1632 (@jamescowens)
 - Change Curl download speed type to support older environments #1640 (@cyrossignol)
 - Optimize logo SVGs used for tray icons #1638 (@cyrossignol)
 - Tweak consolidateunspent rpc function #1644 (@jamescowens)
 - ETTS and staking icon enhancements #1650 (@jamescowens)
 - Implement new transaction fees for block version 11 #1652 (@jamescowens)
 - Optimize in-memory storage of superblock data #1653 (@cyrossignol)
 - Miscellaneous superblock API improvements and housekeeping #1654 (@cyrossignol)
 - Update openssl to 1.1.1f compatibility #1660 (@jamescowens)
 - Optimize bdb to avoid synchronous flush of database #1659 (@jamescowens)
 - Add support for CPID input to "lifetime" RPC function #1668 (@cyrossignol)
 - Overhaul the contract handling system #1669 (@cyrossignol)
 - Make the autostart mainnet/testnet aware #1671 (@jamescowens)
 - Remove slashes from User Agent in peers tab #1674 (@div72)
 - Refactor contracts for polymorphic binary payloads #1676 (@cyrossignol)
 - Overhaul the beacon system #1678 (@cyrossignol)
 - Replace boost::optional<T&> with non-owning pointers #1680 (@cyrossignol)
 - Optimize proof-of-stake validation #1681 (@cyrossignol)
 - Updated Slack link #1683 (@NeuralMiner)
 - Update build-unix.md #1686 (@Quezacoatl1)
 - Replace deprecated QT methods #1693 (@Pythonix)
 - Made protocol.h more similar to bitcoin #1688 (@Pythonix)
 - Touch up some details for block version 11 #1697 (@cyrossignol)
 - More tweaks for block version 11 #1700 (@cyrossignol)
 - Finish the conversion to the BCLog class based logger #1699 (@jamescowens)
 - Move claim version transitional code in miner for proper signature #1712 (@cyrossignol)
 - doc: Update threads in coding.txt #1730 (@div72)
 - qt: Include QPainterPath in trafficgraphwidget.cpp #1733 (@div72)
 - doc: Update doc/build-unix.md #1731 (@div72)
 - gui: Show peers tab on connections icon click #1734 (@div72)
 - refactor: Change return type of IsMine to isminetype && move wallet files to wallet directory #1722 (@div72)
 - build: Updates boost to 1.73.0 for depends #1673 (@jamescowens)
 - doc: Update Unit Test Readme #1743 (@RoboticMind)
 - wallet: Change Assert To Error Message In kernel.cpp #1748 (@RoboticMind)
 - scraper: Shorten display representation of verification codes #1754 (@cyrossignol)
 - log: Change ".B." to Clear Message #1758 (@RoboticMind)
 - util: Fix braindamage in GetDefaultDataDir() #1737 (@jamescowens)
 - scraper: Improve scraper processing of beacon verifications #1760 (@jamescowens)
 - scraper: Add instrumentation to convergencereport #1763 (@jamescowens)
 - rpc: Improve rpc stress test script #1767 (@tunisiano187)
 - Generalize enum serialization #1770 (@cyrossignol)
 - scraper: Improve handling of ETags in http class and tweak verified beacon logic #1776 (@jamescowens)
 - scraper: Improve ProcessNetworkWideFromProjectStats and other tweaks #1778 (@jamescowens)
 - researcher: Automate beacon advertisement for renewals only #1781 (@cyrossignol)
 - gui: Tweak behavior of beacon page in researcher wizard #1784 (@cyrossignol)
 - Prepare for block version 11 hard-fork on testnet #1787 (@cyrossignol)
 - scraper: Modify UpdateVerifiedBeaconsFromConsensus #1791 (@jamescowens)
 - gui: Optimize OverviewPage::updateTransactions() #1794 (@jamescowens)
 - ci: Adopt ci changes from Bitcoin #1795 (@div72)
 - consensus: switch snapshot accrual calculation to integer arithmetic #1799 (@cyrossignol)
 - voting: Overhaul the voting system #1809 (@cyrossignol)
 - contract: Optimize contract replay after chain reorganization #1815 (@cyrossignol)
 - contract: Reimplement transaction messages as contracts #1816 (@cyrossignol)
 - staking: Sign claim contracts with coinstake transaction #1817 (@cyrossignol)
 - gui: Change research wizard text #1820 (@div72)
 - net: Update protocol version and clean up net messaging #1824 (@cyrossignol)
 - rpc, wallet: Corrections to GetAmounts #1825 (@jamescowens)
 - gui: Tweak some minor researcher wizard details #1830 (@cyrossignol)
 - gui: Change GetEstimatedStakingFrequency text #1836 (@jamescowens)
 - scraper: Scraper global statistics cache optimization #1837 (@jamescowens)
 - doc: Update Vulnerability Response Process #1843 (@RoboticMind)
 - scraper: Optimization of manifest and parts sharing between ConvergedScraperStatsCache, mapManifest, and mapParts #1851 (@jamescowens)
 - consensus: Update Checkpoints #1855 (@barton2526)
 - docs: Update docs to build off master #1856 (@barton2526)
 - gui: Fix and improve GUI combo box styles #1858 (@cyrossignol)
 - build: Tweak Gridcoin installer for Fern release #1863 (@jamescowens)

### Removed
 - Remove old research age checks (rebase #1365) #1572 (@cyrossignol)
 - Remove PrimaryCPID check from diagnostics dialog #1586 (@cyrossignol)
 - Remove missed label for PrimaryCPID from diagnostics #1588 (@cyrossignol)
 - Remove legacy quorum messaging system (@neural network) #1589 (@cyrossignol)
 - Remove old remnants of legacy smart contract experiments #1594 (@cyrossignol)
 - Remove block nonce for version 11 #1622 (@cyrossignol)
 - Delete obsolete contrib/Installer and Upgrader directories #1623 (@jamescowens)
 - Remove redundant LoadAdminMessages() calls #1625 (@cyrossignol)
 - Remove some legacy informational RPC commands #1658 (@cyrossignol)
 - Remove informational magnitude field from binary claims #1661 (@cyrossignol)
 - Remove fDebug3,4, and net and convert to BCLog::LogFlags #1663 (@jamescowens)
 - Remove qt5.7.1 depends support build System #1665 (@iFoggz)
 - Remove unused jQuery library #1679 (@cyrossignol)
 - Remove unused NetworkTimer() function and global state #1701 (@cyrossignol)
 - Refactor claim context objects into contracts #1704 (@cyrossignol)
 - Clean old assets up #1718 (@div72)
 - Remove legacy "rain" RPC (not by-project rain) #1742 (@cyrossignol)
 - Temporarily disable voting system on testnet #1769 (@cyrossignol)
 - gui: Remove legacy GUI transaction description for contracts #1772 (@cyrossignol)
 - gui: Remove transaction fee setting #1780 (@cyrossignol)
 - trivial: Cleanup unused legacy functions #1793 (@cyrossignol)
 - mining, rpc: Remove kernel-diff-best and kernel-diff-sum #1796 (@jamescowens)
 - refactor: Remove libs subdirectory #1802 (@div72)
 - scraper: cleanup unused/unnecessary functions #1803 (@jamescowens)
 - gui: Remove useless "Detach databases at shutdown" #1810 (@jamescowens)
 - test: Remove testnet condition for standard transactions #1814 (@cyrossignol)
 - consensus: Remove transitional testnet code #1854 (@cyrossignol)

### Fixed
 - Fix "Owed" amount in output of "magnitude" RPC method #1569 (@cyrossignol)
 - Add support for paths with special characters on Windows #1571 (@cyrossignol)
 - Fix lingering peers.dat temp files and clean up remaining paths #1582 (@cyrossignol)
 - Fix incorrect beacon length warning in GUI transaction list #1585 (@cyrossignol)
 - Fix default config file line endings on Windows #1587 (@cyrossignol)
 - Reenable Travis builds for MacOS #1591 (@jamescowens)
 - Correct peer detail info background color #1593 (@jamescowens)
 - Fix exception in debug3 mode #1598 (@cyrossignol)
 - Fix deadlock in "getmininginfo" RPC function #1596 (@cyrossignol)
 - Fix accuracy of statistics in "network" RPC output #1602 (@cyrossignol)
 - Fix heights for quorum vote weight calculations #1604 (@cyrossignol)
 - Fix deadlock in log archiver when rename fails #1607 (@cyrossignol)
 - Fix a spurious segmentation fault during client load on Windows with fast CPUs #1608 (@jamescowens)
 - Fix lock order debugging and potential deadlocks #1612 (@jamescowens)
 - Add dependencies #1613 (@Scalextrix)
 - Fix std namespace pollution #1617 (@denravonska)
 - Add missing condition for newbie accrual computer #1618 (@cyrossignol)
 - Track first reward blocks in research accounts #1619 (@cyrossignol)
 - Fix lingering beacon warning after advertisement #1627 (@cyrossignol)
 - Fix accrual calculation for new, zero-magnitude CPIDs #1636 (@cyrossignol)
 - Fix diagnostics, add ETTS test, fix tooltipcolor, add missing lock, and add email=investor check #1647 (@jamescowens)
 - Fix help message of two RPC methods #1656 (@div72)
 - Fix legacy accrual for newbie with non-zero past reward #1667 (@cyrossignol)
 - Fix GUI autostart on Windows for paths with wide characters #1670 (@cyrossignol)
 - Qualify boost bind placeholders with their full namespace #1672 (@Ponce)
 - Fix suffix when copying txids #1677 (@div72)
 - Unnecessary if-statement removed #1685 (@Pythonix)
 - Fix consolidatemsunspent Help Message #1687 (@Pythonix)
 - Fix gettransaction help message #1691 (@Pythonix)
 - Fix GetNewMint To Look for Stakes #1692 (@RoboticMind)
 - Suppress deprecated copy warnings for Qt with GCC 9+ #1702 (@cyrossignol)
 - Fix exclusion error on stats processing and misplaced ENDLOCK logging entry #1710 (@jamescowens)
 - Removed unnecessary comparison #1708 (@Pythonix)
 - Fixed typo #1707 (@Pythonix)
 - Fix out-of-bounds exception for peers tab version slashes #1713 (@cyrossignol)
 - Fix transition for v1 superblocks when reorganizing #1714 (@cyrossignol)
 - Touch up transition to version 2 transactions #1715 (@cyrossignol)
 - Avoid mutating transactions in ConnectBlock() #1716 (@cyrossignol)
 - Skip beacon advertisement when already pending #1726 (@cyrossignol)
 - Fix Windows cross-compilation in newer environments #1728 (@cyrossignol)
 - Fix out-of-bounds access in IsMineInner() #1736 (@cyrossignol)
 - Fix a couple of block version 11 issues #1738 (@cyrossignol)
 - Fix null pointer dereference in GUI researcher model #1741 (@cyrossignol)
 - accrual: Reset research accounts when rebuilding accrual snapshots #1745 (@cyrossignol)
 - scraper: Correct update for verified beacons #1747 (@jamescowens)
 - accrual: Refactor tally initialization for snapshot rebuild #1749 (@cyrossignol)
 - rpc: Fix "cpid" field in "beaconconvergence" RPC output #1750 (@cyrossignol)
 - accrual: Fix snapshot accrual superblock state transitions #1752 (@cyrossignol)
 - scraper: Correct stale verified beacon logic #1753 (@jamescowens)
 - rpc: Correct possible divide by zero in getblockstats #1755 (@jamescowens)
 - gui: Fix issues with researcher wizard flow #1756 (@cyrossignol)
 - wallet: Stop Error When Starting From Zero #1759 (@RoboticMind)
 - Don't count empty email as explicit investor #1761 (@cyrossignol)
 - accrual: Fix snapshot accrual superblock state transitions #1764 (@cyrossignol)
 - rpc: Cleanup Help Message and Fix Typo #1771 (@RoboticMind)
 - scraper: Fix scraper etag header case sensitivity #1773 (@cyrossignol)
 - consensus: Use explicit time to check if superblock needed #1774 (@cyrossignol)
 - gui: Fix scroll area dark theme styles #1785 (@cyrossignol)
 - rpc, gui: Fix three divide by zero possibilities #1789 (@jamescowens)
 - rpc: Fix balance pre-check in "rainbymagnitude" RPC #1792 (@cyrossignol)
 - accrual: Fix outdated comment and correct grammar #1800 (@RoboticMind)
 - gui: Fix stuck cursor on labels #1801 (@div72)
 - beacon: Fix research wizard beacon renewal status #1805 (@cyrossignol)
 - gui: Fix translations for port numbers #1818 (@cyrossignol)
 - util: Create parent directory #1821 (@div72)
 - mining: Fix coinstake/claim signature order #1828 (@cyrossignol)
 - voting: Remove double increment in loop #1831 (@cyrossignol)
 - neuralnet, scraper: Fix compilation with gcc5 and older libcurl #1832 (@a123b)
 - wallet: Fix smallest coin selection for contracts #1841 (@cyrossignol)
 - gui: Fix display of polls with no votes yet #1844 (@cyrossignol)
 - gui: add indentation to diagnostic status bar labels #1849 (@jamescowens)
 - voting, gui: Fix formatting and alignment of vote shares and percent #1850 (@jamescowens)
 - wallet, rpc: Fix for self-transactions in listtransactions #1852 (@jamescowens)
 - accrual: Clear any accrual snapshots when syncing from pre-v11 #1853 (@cyrossignol)
 - accrual: Fix reset of accrual directory if starting sync below research age height #1857 (@jamescowens)
 - gui: Fix researcher wizard layout on macOS with native theme #1860 (@cyrossignol)

## [4.0.6.0] 2019-10-22, leisure, "Ernestine"
### Added
 - Add testnet desktop launcher action for Linux #1516 (@caraka)
 - Shuffle vSideStakeAlloc if necessary to support sidestaking to more than 6 destinations #1532 (@jamescowens)
 - New Superblock format preparations for Fern #1526, #1542 (@jamescowens, @cyrossignol)
 - Multisigtools
   - Consolidate multisig unspent #1529 (@iFoggz)
   - Scanforunspent #1547 (@iFoggz)
   - consolidatemsunspent and scanforunspent bug fix #1561 (@iFoggz)
 - New banning misbehavior handling and Peers Tab on Debug Console #1537 (@jamescowens)
 - Reimplement getunconfirmedbalance rpc #1548 (@jamescowens)
 - Add CLI switch to display binary version #1553 (@cyrossignol)

### Changed
 - Select smallest coins for contracts #1519 (@iFoggz)
 - Move some functionality from miner to SelectCoinsForStaking + Respect the coin reserve setting + Randomize UTXO order #1525 (@iFoggz)
 - For voting - if url does not contain http then add it #1531 (@ifoggz)
 - Backport newer serialization facilities from Bitcoin #1535 (@cyrossignol)
 - Refactor ThreadSocketHandler2() Inactivity checks #1538 (@iFoggz)
 - Update outdated checkpoints #1539 (@barton2526)
 - Change needed to build Gridcoin for OSX using homebrew #1540 (@Git-Jiro)
 - Optimize scraper traffic for expiring manifests #1542 (@jamescowens)
 - Move legacy neural vote warnings to debug log level #1560 (@cyrossignol)
 - Change banlist save interval to 5 minutes #1564 (@jamescowens)
 - Change default rpcconsole.ui window size to better support new Peers tab #1566 (@jamescowens)

### Removed
 - Remove deprecated RSA weight and legacy kernel #1507 (@cyrossignol)

### Fixed
 - Clean up compiler warnings #1521 (@cyrossignol)
 - Handle missing external CPID in client_state.xml #1530 (@cyrossignol)
 - Support boost 1.70+ #1533 (@iFoggz)
 - Fix diagnostics failed to make connection to NTP server #1545 (@Git-Jiro)
 - Install manpages in correct system location #1546 (@Git-Jiro)
 - Fix ability to show help and version without a config file #1553 (@cyrossignol)
 - Refactor QT UI variable names to be more consistent, Fix Difficulty default #1563 (@barton2526)
 - Fix two regressions in previous UI refactor #1565 (@barton2526)
 - Fix "Owed" amount in output of "magnitude" RPC method #1569 (@cyrossignol)

## [4.0.5.0] 2019-08-20, leisure, "Elizabeth"
### Added
 - Add freedesktop.org desktop file and icon set #1438 (@a123b)
 - Add warning in help for blockchain scan for importprivkey #1469 (@jamescowens)
 - Consolidateunspent rpc function #1472 (@jamescowens)
 - Scraper 2.0 improvements #1481, #1488, #1509, and #1514 (@jamescowens, @cyrossignol)
   - explorer mode operation
   - simplified explainmagnitude output
   - improved convergence reporting, including scraper information in the tooltip when fDebug3 is set
   - improved statistics and SB contract core caching based on a bClean flag in the cache global
   - new SB format and packing for bv11
   - new SB contract hashing (native) for bv11
   - changes to accommodate new beacon approach
   - Implement in memory versioning for team file ETags 
 - Implement local dynamic team requirement removal and whitelist #1502 (@cyrossignol)

### Changed
 - Quiet logging for getmininginfo and scraper INFO logging level #1460 (@jamescowens)
 - Spelling corrections #1461, #1462 (@caraka)
 - Update crypto module #1453 (@denravonska)
 - Update .travis.yml for Bionic #1475 (@jamescowens)
 - Create CPID classes and clean up CPID code #1477 (@cyrossignol)
 - Refactor researcher context and CPID harvesting #1480 (@cyrossignol)
   - Remove boinckey export RPC method and import handler
 - Notify when wallet locked in advertisebeacon RPC method #1504 (@cyrossignol)
 - Notify when wallet locked in beaconstatus RPC method #1506 (@cyrossignol)
 - Change spacer minimum height hint #1511 (@jamescowens)

### Removed
 - Remove safe mode #1434 (@denravonska)
 - Remove bitcoin.moc in Makefile.qt.include #1444 (@RoboticMind)
 - Clean up legacy Proof-of-Work functions #1497 (@cyrossignol)

### Fixed
 - Constrain walletpassphrase to 10000000 seconds #1459 (@jamescowens)
 - Straighten out localization in the scraper. #1471 (@jamescowens)
 - Quick fix for rainbymagnitude #1473 (@jamescowens)
 - Correct negation error in scraper tooltip for vScrapersNotPublishing #1484 (@jamescowens)
 - Fix staked block rejection when active researcher #1485 (@cyrossignol)
 - Add back informational magnitude to generated blocks #1489 (@cyrossignol)
 - Add back in the in sync check in ScraperGetNeuralContract #1492 (@jamescowens)
 - Scraper correct team file processing. #1501 (@jamescowens)
 - Have importwallet file path default to datadir #1508 (@jamescowens)
 - Scraper add Beacon Map size check to ensure convergence #1515 (@jamescowens)

## [4.0.4.0] 2019-05-16, leisure
### Fixed
 - Adds back the new user wizard inadvertently removed #1464 (@jamescowens).
 - Repair scraper team filtering #1466 (@jamescowens).

## [4.0.3.0] 2019-05-10, leisure, "Denise"
### Added
 - Replace NeuralNetwork with portable C++ scraper #1387 (@jamescowens,
   @tomasbrod, @cycy, @TheCharlatan, @denravonska).
 - Allow compile flags to be used for depends #1423 (@G-UK).
 - Add stake splitting and side staking info to getmininginfo #1424
   (@jamescowens).
 - Add freedesktop.org desktop file and icon set #1438 (@a123b).

### Changed
 - Disable Qt for windows Travis builds #1276 (@TheCharlatan).
 - Replace use of AppCache PROJECT section with strongly-typed structures #1415
   (@cyrossignol).
 - Change dumpwallet to use appropriate data directory #1416 (@jamescowens).
 - Optimize ExtractXML() calls by avoiding unnecessary string copies #1419
   (@cyrossignol).
 - Change signature of IsLockTimeWithinMinutes #1422 (@jamescowens).
 - Restore old poll output for getmininginfo RPC #1437 (@a123b).
 - Prevent segfault when using rpc savescraperfilemanifest #1439 (@jamescowens).
 - Improve miner status messages for ineligible staking balances #1447
   (@cyrossignol).
 - Enhance scraper log archiving #1449 (@jamescowens).

### Fixed
 - Re-enable full GUI 32-bit Windows builds - part of #1387 (@jamescowens).
 - Re-activate Windows Installer #1409 (@TheCharlatan).
 - Fix Depends and Travis build issues for ARM #1417 (@jamescowens).
 - Fix syncupdate icons #1421 (@jamescowens).
 - Fix potential BOINC crash when reading projects #1426 (@cyrossignol).
 - Fix freeze when unlocking wallet #1428 (@denravonska).
 - Fix RPC after high priority alert #1432 (@denravonska).
 - Fix missing poll in GUI when most recent poll expired #1455 (@cyrossignol).

### Removed
 - Remove old, rudimentary side staking implementation #1381 (@denravonska).
 - Remove auto unlock #1402 (@denravonska).
 - Remove superblock forwarding #1430 (@denravonska).

## [4.0.2.0] 2019-04-03, leisure, "Camilla"
### Added
 - Add `rainbymagnitude` RPC command #1235 (@Foggyx420).
 - Add stake splitting and side staking #1265 (@jamescowens).
 - Detect and block Windows shutdown so wallet can exit cleanly #1309
   (@jamescowens).
 - Add message support to sendfrom and sendtoaddress #1400 (@denravonska).

### Changed
 - Configuration options are now case insensitive #294 (@Foggyx420).
 - Update command in beaconstatus help message #1312 (@chrstphrchvz).
 - Improve synchronization speeds:
   - Refactor superblock pack/unpack #1194 (@denravonska).
   - Optimize neuralsecurity calculations #1255 (@denravonska).
   - Reduce hash calculations when checking blocks #1206 (@denravonska).
 - Make display of private key in beaconstatus OPT-IN only #1275 (@Foggyx420).
 - Store Beacon keys in Wallet #1088 (@tomasbrod).
 - Use default colors for pie chart #1333 (@chrstphrchvz).
 - Show hand cursor when hovering clickable labels #1332 (@chrstphrchvz).
 - Update README.md #1337 (@Peppernrino).
 - Fix integer overflow with displayed nonce #1297 (@personthingman2).
 - Improve application cache performance #1317 (@denravonska).
 - Improve reorg speeds #1263 (@denravonska).
 - Update Polish translation #1375 (@michalkania).

## Fixed
 - Remove expired polls from overview page #1250 (@personthingman2).
 - Fix plural text on block age #1304 (@scribblemaniac).
 - Fix researcher staking issue if your chain head was staked by you,
   #1299 (@denravonska).
 - Fix incorrect address to grcpool node #1314 (@wilkart).
 - Do not replace underscores by spaces in Qt Poll URLs #1327 (@tomasbrod).
 - Fix scraper SSL issues #1330 (@Foggyx420).

### Removed
 - Remove or merged several RPC commands #1228 (@Foggyx420):
    - `newburnaddress`, removed.
    - `burn2`: Removed.
    - `cpid`: Merged into `projects`.
    - `mymagnitude`: Merged into `magnitude`.
    - `rsa`: Removed, use `magnitude`.
    - `rsaweight`: Removed, use `magnitude`.
    - `proveownership`: Removed.
    - `encrypt`: Removed.
 - Remove obsolete POW fields from RPC responses #1358 (@jamescowens).
 - Remove obsolete netsoft fields for slight RAM requirement reduction
   #1336 (@denravonska).
 - Remove unused attachment functionality #1345 (@denravonska).

## [4.0.1.0] 2018-11-30, leisure
### Fixed
 - Wrong RA scan range causing reward calculation disagreements and forks
   #1366, (@tomasbrod, @jamescowens, @denravonska).
 - Fix crashes when voting in polls #1369 (@denravonska).

## [4.0.0.0] 2018-10-19, mandatory, "Betsy"
### Added
 - Linux nodes can now stake superblocks using forwarded contracts,
   #1060 (@tomasbrod).

### Changed
 - Replace interest with constant block reward #1160 (@tomasbrod).
   Fork is set to trigger at block 1420000.
 - Raise coinstake output count limit to 8 #1261 (@tomasbrod).
 - Port of Bitcoin hash implementation #1208 (@jamescowens).
 - Minor canges for the build documentation #1091 (@Lenni).
 - Allow sendmany to be used without an account specified #1158 (@Foggyx420).
### Fixed
 - Fix `cpids` and `validcpids` not returning the correct data #1233
   (@Foggyx420).
 - Fix `listsinceblock` not showing mined blocks to change addresses,
   #501 (@Foggyx420).
 - Fix crash when raining using a locked wallet #1236 (@Foggyx420).
 - Fix invalid stake reward/fee calculation (@jamescowens).
 - Fix divide by zero bug in `getblockstats` RPC #1292 (@Foggyx420).
 - Bypass historical bad blocks on testnet #1252 (@Quezacoatl1).
 - Fix MacOS memorybarrier warnings #1193 (@ghost).

### Removed
 - Remove neuralhash from the getpeerinfo and node stats #1123 (@Foggyx420).
 - Remove obsolete NN code #1121 (@Foggyx420).
 - Remove (lower) Mint Limiter #1212 (@tomasbrod).

## [3.7.16.0] 2018-09-03, leisure
### Fixed
 - Fix burned coins incorrectly showing up in wallets #1283 (@jamescowens).
 - Fix decimal output in RPC commands #1272 (@Foggyx420).
 - Fix verbose flag in `getrawtransaction` RPC output #1271 (@jamescowens).

## [3.7.15.0] 2018-07-18, leisure
### Changed
 - Balance now includes unconfirmed coins sent by self #1192 (@Foggyx420).

## [3.7.14.0] 2018-07-17, leisure, "Annie Sue"
### Added
 - Support for Qt 5.9 (@thecharlatan)
 - Compatibility with boost-1.67 (@denravonska)
 - Calculations to reduce network time offset (@jamescowens)
 - Feedback for addnode RPC command (@tomasbrod)
 - Added data acquisiton commands (@tomasbrod):
    - getrecentblocks
    - exportstats1
    - getsupervotes
 - /var/lib/boinc/ as a valid boinc path on Linux (@rsparlin)
 - Stress testing script  (@Foggyx420)
 - refhash command also on linux (@jamescowens)
 - Documentation for out of source build (@thecharlatan)
 
### Changed
 - More accurate time to stake and network weight estimations (@jamescowens)
 - Compressed image files (@Peppernrino)
 - Poll (voting) code refactoring (@thecharlatan)
 - BITCOIN optimize command listunspent (@Foggyx420)
 - RPC server refactoring (Wladimir J. van der Laan) (@thecharlatan)
 - Replace json spirit with Univalue JSON library (@thecharlatan)
 - Change repository URL (@Foggyx420)
 - Pretty-print rpc output (@denravonska)
 - Logging for debugging reward computation (@tomasbrod)
 - Clean-up beacon manipulation (@Foggyx420)
 
### Fixed
 - Building errors on Mac related to SVG framework (@thecharlatan)
 - neural data response
 - neural network fixes (@Foggyx420)
 - investor cpid's appearing as zeros in block index (@tomasbrod)
 - ensure that daemon functionality is correct when built together with gui wallet (@jamescowens)
 - improve logging, remove empty lines (@jamescowens) (@Foggyx420) (@tomasbrod) (@denravonska)
 - windows socket warnings (@thecharlatan)

### Removed
 - unused components of neural network (@Foggyx420)
 - GRCRestarter (@Foggyx420)
 - Galaza (game) (@Foggyx420)
 - unused images (@barton2526)
 - unused code (@Foggyx420) (@Pythonix)
 - unusual activity report (@tomasbrod)
 - burnamount and recipient from appcache (@tomasbrod)
 - GUI FAQ (@Lenni)
 - unusable limit from magnitude command (@Foggyx420)
 - cgminer support (@Foggyx420)
 - deprecated menu items (@jamescowens) 

## [3.7.13.0] 2018-06-02, leisure
### Fixed
 - Fix voting regression when done from the UI #1133 (@Foggyx420).

## [3.7.12.0] 2018-05-25, leisure

### Fixed
 - Fixes for displaying on high DPI displays #517 (@skcin).
 - Re-enable unit tests, add unit test to Travis #769 #808 (@thecharlatan).
 - Fix empty string in sendalert2 (@tomasbrod).

### Added
 - Neural Report RPC command #1063 (@tomasbrod).
 - GUI wallet redign with new icons and purple native style (@skcin).
 
### Changed
 - Switch to autotools and Depends from Bitcoin #487 (@thecharlatan).
 - Clean and update docs for new build system, remove outdated #828 (@thecharlatan).
 - Change estimated time to stake calculations to be more accurate #1084 (@jamescowens).
 - Move logging to tinyformat #1009 (@thecharlatan).
 - Improve appcache performance #734 (@denravonska).
 - Improve block index memory access performance #679 (@denravonska).
 - NN fixes: clean logging, explain mag single response, move contract to ndata_nresp (@denravonska)
 - Updated translations:
    - Turkish #771 (@confuest).
    - Chinese #1012 (@linnaea).
 - RPC refactor: Cleaner locks, better error handling, move execute calls to straght rpc calls #1024 (@Foggyx420).
 - Change locking primitives from Boost to STL #1029 (@Foggyx420).

### Removed
 - gridcoindiagnostic RPC call (@denravonska).
 - Galaza #945 (@barton2526).
 - Assertion in SignSignature #998 (@thecharlatan).
 - Upgrade menu #1094 (@jamescowens).
 - Acid test functions #871 (@tomasbrod).
 - Qt4 support #801 (@denravonska).
 
## [3.7.11.0] 2018-03-15, leisure
### Fixed
 - Fix wallet being locked while flushing. It now requires a clean shutdown
   or a backup to migrate the wallet.dat to a different system #1010 (@jamescowens).

### Changed
 - Automatic backups can now be disabled by using `-walletbackupinterval=0`,
  #1018 (@denravonska).
 - Trigger a fix spent coins check on start and after block disconnect #1018 (@denravonska).

## [3.7.10.0] 2018-03-05, leisure
### Fixed
 - Fix sync issues due to beacon age checks #1003 (@denravonska).

## [3.7.9.0] 2018-03-03, leisure
### Fixed
 - Fix issues with NN participation on Windows #986 (@Foggyx420).
 - Fix stray data in beaconreport RPC #986 (@Foggyx420).
 - Fix spelling error #989 (@caraka).

## [3.7.8.0] 2018-03-01, mandatory
### Fixed
 - Move context sensitive DPoR block checks to ConnectBlock #922 (@tomasbrod).
 - Check incoming blocks for malformed DPoR signature #922.
 - Correct tally height on init #917 (@denravonska).
 - Prevent staking of a block with a failed signature #948 (@Foggyx420).
 - Fix UI and RPC slowdown regression #961 (@denravonska).
 - Fix Debian lint errors #886 #885 #884 #883 (@caraka).
 - Fix fork issue due to research age calculation inconsistencies #939
   (@denravonska).
 - Fix crashes when tallying #934 (@denravonska).
 - Revert reorganize of the chain trust becomes less than what it was #957
   (@tomasbrod).
 - Fix sync issues with incorrectly accepted v8 beacons #979 (@tomasbrod).

### Changed
  - Double check PoS kernel #958 (@tomasbrod).
  - Don't tally until V9 to speed up syncing #943 (@denravonska).

## [3.7.7.0] 2018-02-02
### Fixed
 - Beacon validation are now done when accepting blocks, not when receiving,
   #899 (@denravonska).
 - Fix crashes due to buffer overflow in encrypt/decrypt #890 (@denravonska).
 - Rewrite reorganize routine to be more reliable and drop contracts received
   or issued while on a side chain to help reducing forks #902 (@tomasbrod).

## [3.7.6.0]
Internal test version used to sort out the forks.

## [3.7.5.0] 2018-01-24
### Fixed
 - Fix crash when switching to new tally on block 1144120 #868 (@denravonska).
 - Fix crash when staking while tallying #866 (@denravonska).

## [3.7.4.0] 2018-01-20
### Fixed
 - Fix RPC resource leak regression. This also reduces RPC overhead,
   making calls ~25-35% faster #848 (@denravonska).
 - Fix incorrect return code when forking #832 (@denravonska).

### Removed
 - Remove upgrader option until rewritten #836 (@Foggyx420).

## [3.7.3.0] 2018-01-13
### Fixed
 - Fix for UI getting stuck in splash screen (@denravonska).

## [3.7.2.0] 2018-01-13
### Fixed
 - Properly fix for wallet not daemonizing #822 (@denravonska).

## [3.7.1.0] 2018-01-10
### Fixed
 - Fix several crashes in diagnostic dialog #816 (@Foggyx420).
 - Fix client not exiting when running as daemon (@denravonska).
 - Fix issue with boincstake.dll not updating on dirty installs (@Foggyx420).

### Changed
 - Update splash screen #685 (acey1).

## [3.7.0.0] 2018-01-08
### Added
 - Provide Difficulty of best kernel found #766 (@tomasbrod).
 - Add Travis support for OSX, 665 (@acey1).
 - Add better command for sending alerts #731 (@tomasbrod).
 - Add RPC for sending raw contracts #683 (@tomasbrod).
 - Add portable diagnostic page #631 (@fooforever).

### Fixed
 - Fixed minor spelling mistakes #742 (@denravonska).
 - Several tally improvements. There should now be less forking and the wallet
   should use ~50MB less memory #668 #756 (@denravonska, @tomasbrod)
 - Data scraper can no longer run concurrently #742 (@denravonska).
 - Improve superblock validations #730 (@tomasbrod).
 - Fix potential deadlock #708 (@denravonska).
 - Prevent duplicate superblocks #534 (@tomasbrod).
 - Fix issue with application cache clears #577 (@tomasbrod).
 - Fix bug which caused rewards to be lost when staking the newbie block.
   Missing rewards will be reimbursed #552 (@Foggyx420).
 - Fix minor UI typos #661 (@Erkan-Yilmaz).
 - Fix stake modifier #686 (@tomasbrod).
 - Improve boost-1.66.0 compatibility #800 (@denravonska).
 - Fix crash in diagnostics dialog #794 (@Foggyx420).

### Changed
 - Changed versioning extraction from git. Test builds can no longer be used to
   stake in production unless explicitly enabled #729 (@tomasbrod).
 - Don't update network quorum while syncing #728 (@Foggyx420).
 - Snapshot URL now uses https #727 (@Foggyx420).
 - Code cleanup (@Foggyx420, @denravonska).
 - Use more efficient data structure for blocks #679 (@denravonska).
 - Improve transaction description dialog #676 (@Foggyx420).
 - Improve beacon handling #604 #645 #649 #684 #701 (@Foggyx420, @tomasbrod).
 - Optimize double<->string conversions #692 (@denravonska).
 - Optimize application cache access #506 (@denravonska).
 - Improve thread handling #656 (@skcin).
 - Replace `boost::shared_ptr` with `std::shared_ptr`.
 - Optimize string split function #672 (@denravonska).
 - Improve sync speeds #650 (@denravonska).
 - The RPC command `restartclient` is now called `restart`.
 - Fix voting sorting issues #610 (@MagixInTheAir).
 - Improve wallet backup #610 (@Foggyx420).
 - Update seed nodes #783 (@barton2526).
 - Auto upgrades are now opt-in via the "autoupgrade" flag #796 (@denravonska).
 - Clean up seed nodes #783 (@barton2526).

### Removed
 - Remove CSV exporter which used unreliable data #759 (@denravonska).
 - Remove block download menu options on non-Windows #727 (@Foggyx420).
 - Removed RPC commands (@Foggyx420, @denravonska):
    - debugexplainmagnitude
    - executecode
    - getsubsidy
    - list newbieage
    - list staking
    - leder
    - reboot
 - Remove checkpoint relaying to improve sync speeds #678 (@denravonska).
 - Remove IRC peer discovery.

## [3.6.3.0] 2017-10-09
### Fixed
 - Fix problems sending beacons on Windows #684 (@tomasbrod).
 - Fix clients getting stuck at V8 blocks when syncing #686 (@tomasbrod).

## [3.6.2.0] 2017-09-28
### Added
 - Add "backupprivatekeys" RPC command #593 (@Foggyx420).
 - Add more transaction details to the UI #573 (@tomasbrod).
 - Add Additional logging to diagnose PoR reward loss (@tomasbrod)

### Fixed
 - Reduce startup time by 15 seconds #626 (@tomasbrod, @Foggyx420).
 - Prevent email being leaked in CPIDv2 block field #621 (@tomasbrod).
 - Fixed memory leaks when receiving orphans while in sync #622 (@denravonska).
 - Unconfirmed balance was not shown in UI #615, (@Foggyx420).
 - Fix memory leaks when clearing orphans #609, (@MagixInTheAir).
 - Fix an issue where multiple beacons could be advertised in rapid
   succession #604 (@Foggyx420).
 - Stake weight in the UI will no longer include old DPOR weght #602
   (@Foggyx420). 
 - Fix stake modifier mismatch which caused nodes to get stuck on first
   V8 block #581 (@tomasbrod).
 - Fix beacon auto advertisement issue when done automatically #580 (@Foggyx420).
 - Fix for loss of PoR rewards due to reorganize #578 (@tomasbrod).
 - Fix upgrader compile error on Linux #541 (@theMarix).
 - Fix duplicate poll entries #539 (@denravonska).
 - Importing private keys will no longer require a restart for the addresses
   to show up #634 (@Foggyx420).
 - Fix invalid backup filenames on Windows #569 (@denravonska).

### Changed
 - Code cleanup (@Foggyx420, @tomasbrod, @denravonska).
 - Several NN consensus sync improvements #616 (@Foggyx420).
 - Windows nodes will no longer automatically reboot/shutdown #605
   (@denravonska).
 - Display "No Polls!" in poll window if no polls are running #596
   (@MagixInTheAir).
 - Change poll min search length from 2 to 1 #595 (@MagixInTheAir).
 - Return the results of "backupwallet" RPC command #593 (@Foggyx420).
 - Changing the community links #654 (@grctest)

## [3.6.1.0] 2017-09-11
### Fixed
 - Fix problems forging superblock due to rounding differences #608 (@denravonska).
 - Fetch data from project servers if missing on scraper #564 (@denravonska).

## [3.6.0.2] 2017-08-26
### Fixed
 - Fix incorrect V8 height trigger check. Many thanks to @barton2526 for discovering this.
 - Fix invalid superblock height formatting #532 (@denravonska).
 - Fix several spelling mistakes, 533 (@Erkan-Yilmaz).

## [3.6.0.1] 2017-08-22
### Added
 - Added [V8 stake engine](https://github.com/gridcoin/Gridcoin-Research/wiki/Stake-V8)
   set to start producing V8 blocks at block 1010000. This fixes several security issues,
   see wiki for details.
 - Blocks can now carry identification from the "org" argument/configuration option (@tomasbrod).
 - Add "reorganize" RPC command (@tomasbrod).

### Changed
 - Berkeley DB V6+ compatibility #451 (@xPh03n1x).
 - Improved poll loading speeds #497 (@denravonska).
 - Versions now contain the git hash #500 (@tomasbrod).
 - Improved security on NeuralNet votes #496 (@Foggyx420).
 - Improved RPC help. It now supports "execute help" and "list help",
   #512 (@Foggyx420).
 - Voting is now integrated in wallet as a tab and cleaned up #416 (@skcin, @JoShoeAh).
 - Improve low-peer mining ability on testnet (@tomasbrod).
 - Improve poll error message when low on funds #415 (@Erkan-Yilmaz).
 - Code cleanup (@denravonska, @tomasbrod, @Foggyx420, @skcin).

### Removed
 - Remove RPC commands:
    - DAO #486 (@denravonska).
    - volatilecode, testnet0917, testboinckey, chainrsa, testcpidv2, testcpid, windows
      error report disabling, list betatest, fDebug4/fDebug5 flags (@Foggyx420).
 - Set magnitude boost to be removed at 2017-Sep-07 00:00:00 UTC

### Fixed
 - Fixed security issue where superblocks could be injected #526 (@tomasbrod).
 - Fix poll sorting bug #512 (@skcin)

## [3.6.0.0] - 2017-08-14
### Fixed
 - Fix a crash when starting up as a new user #488 (@Foggyx420, 
   @denravonska).
 - Fix an out of memory crash when syncing from 0 #508 (@tomasbrod).

## [3.5.9.9] - 2017-08-05
### Changed
 - Staking cleanup #301 (@tomasbrod). This also solves several other issues:
 - UI:
    - Wallet window can now be made smaller #384 (@skcin). 
    - Interest and Research subsidy visible in getmininginfo (@tomasbrod).
    - External links now use HTTPS where possible, and the code has been cleaned
      up #339 (@skcin).
    - Rearrange menu items to reduce the number of entries. Remove references
      to broken function #362 (@skcin).
 - Replace translations which were just question marks with new files from
   the Bitcoin source tree: Arabic, Belarusian, Bulgarian, Greek, Persian,
   Hebrew, Hindi, Japanese, Georgian, Kirghiz, Serbian, Thai, Ukrainian,
   Urdu and Chinese.
 - Don't print the "Bootup" and "Signing block" messages unless fDebug (@tomasbrod). 
 - Print beacons as they are loaded and debug3=true (@tomasbrod).
 - Show superblock information in getblock (@tomasbrod).
 - Code cleanup (@skcin).
 - Update Lithuanian translations #469 (@Rytiss).
 - Add block size min, max, avg to block stats RPC (@tomasbrod).
 - Fields on overview page are now selectable.

### Fixed
 - High CPU usage #349 (@tomasbrod)
 - Repetetive block signing #295 (@tomasbrod)
 - Staking creates 1 cent output #311 (@tomasbrod)
 - Client no longer has to be restarted for a beacon to activate #253
   (@Foggyx420).
 - Fixed a coin age bug which made it hard to stake on testnet (@denravonska)
 - Fixed reloading of polls in the voting GUI #431 (@skcin) 
 - Fix crash when listing receivedby on addresses with no transactions,
   #456 (@denravonska).
 - Fix buffer overflow in TX message unscambling #468 (@tomasbrod).
 - Splash screen can no longer be dismissed and the UI can no longer be shown
   until the wallet has fully loaded #353 (@denravonska).

### Removed
 - Removed newbie boost #332
 - Removed obsolete functionality.

## [3.5.9.8] - 2017-07-29
### Changed
 - Revised Neural Network magnitude calculation to prevent diluted magnitudes.
 - Cap magnitude to 32766 in NeuralNet to avoid future hash inconsistencies when packing binary superblocks.

### Fixed
 - Fix binary pack/unpack bug which could cause the contract to get a different hash when unpacked.
 - Revised Neural Network business logic rule fix inability to stake current superblock.

## [3.5.9.7] - 2017-07-25
### Fixed
 - Add artificial researcher to contract to push the average magnitude above 70. Without this the superblock is rejected by the wallet.

## [3.5.9.6] - 2017-07-24
### Fixed
 - Use UTC time instead of local time when determining file mirror filename suffix.

## [3.5.9.5] - 2017-07-22
### Fixed
 - Fix incorrect handling of 404 errors in NeuralNet.
 - Fix a bug causing the NeuralNet to skip Rosetta.

## [3.5.9.4] - 2017-07-16
### Changed
 - Added checkpoint (block 950000).

### Fixed
 - Fix neural network missing folder error.
 - Fix speech bug.

## [3.5.9.3] - 2017-07-15
### Changed
 - Require superblocks to be populated with more than half of the
   whitelisted projects.
 - Add subfolders to Neural Network

### Fixed
 - Fix neural network inability to stake superblocks.

## [3.5.9.2] - 2017-07-04
### Fixed
 - Fix neural network project gather bug related to timezones.

## [3.5.9.1] - 2017-07-03
### Changed
 - Neural Network improvements:
    - Don't download stats data that hasn't changed.
    - Use gridcoin.us as a stat mirror to reduce BOINC server loads.
    - Use UTC instead of local time when filtering idle CPIDs.
    - Only include beacons younger than 6 months when calculating mags.

### Security
 - Prevent untauthorized poll and vote deletions.

## [3.5.9.0] - 2017-06-05
### Added
 - Added execute unspentreport (shows proof of unspent coins in wallet).
 - Add RPC commands for changing debug flags: debug, debugnet, debug2, debug3,
   debug4, debug5, debug10. #309 (@Foggyx420).
 - Add support for themes via stylesheets #233 (@skcin).
 - Add support for OpenSSL 1.1.x #164.
 - BOINC data dir auto detection #242 (@3ullShark)
 - Add install (make install) target for UNIX systems.
 - Add aarch64 support #151 (@datenklause).
 - Add diagnostic message for if web lookup fails for cpid valid test,
   #175 (@fooforever).

### Changed
 - Wallet overview cleanup #233 (@skcin)
    - The main overview page is now cleaner, more structured and holds more of
     the recent transactions.
    - Displayed DPOR weight should now be accurate #233 (@skcin).
 - Show as many of the recent transactions as we can fit on the overview page.
 - Translation updates
    - Portuguese (Miguel Veiga)
    - Slovak (@tomasbrod)
    - Swedish (@denravonska)
    - Afrikaan and Spanish (@philipswift)
    - French (@PsiPhiTheta)
    - Russian (@rambinho)
 - Gridcoinstats is now used as block explorer #308.
 - Slight RAM usage reduction.
 - Improve beacon advertise error message #133 (@comprehendreality).
 - Code cleanup (@Foggyx420, @TheCharlatan).

### Fixed
 - Fix numerous beacon issues #344 #321 and #334 (@Foggyx420).
 - Fix incorrect WCG URL #323 (@3ullShark).
 - Fix alt key shortcut order #326 (@TheCharlatan).
 - Fix a bug where beacons were stored even though none were generated due
   to the wallet being locked #264 (@denravonska).

### Removed
 - Remove empty "wcgtest" RPC command.

### Security
 - Security enhancement (@tomasbrod)
 - Upgraded security on voting system - voting proof of balance and proof of
   magnitude.

## [3.5.8.9] - 2017-05-15
### Added
- Implement voting functionality for Linux and OSX (@skcin).
- Add man pages to doc folder #135 (@caraka).

### Changed
 - Windows are now resizable 
 - Replace Windows voting dialog with the new dialog.
 - Update Gridcoin icon on Windows.
 - Enable C++11.
 - Update Hungarian translations (@matthew11).
 - Update Portuguese translations (Miguel Veiga).
 - Update icon set by @Peppernrino.
 - Update icon on OSX #193 (@coagmano).
 - Lossless compression of resources #227 (@Peppernrino).
 - Reduced memory usage by around 100MB+.
 - Improve UI when used with dark themes on Linux #222 (@skcin).

### Fixed
 - Fix OSX build issues #174 (@coagmano).
 - Fix occasional crashes when starting on Linux #139.
 - Fix freeze when clicking on the "Amount" field under Send Coins when using
   KDE #210.
 - Possible fix for invalid time check in diagnostic.

### Removed
 - Remove lots of dead, obsolete code.
 - Removed unused link dependencies: librt, boost_chrono, boost_date_time, libz
   and libdl.
