# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [3.7.10.0] 2018-03-05, leisure
### Fixed
 - Fix sync issues due to beacon age checks, #1003 (@denravonska).

## [3.7.9.0] 2018-03-03, leisure
### Fixed
 - Fix issues with NN participation on Windows, #986 (@Foggyx420).
 - Fix stray data in beaconreport RPC, #986 (@Foggyx420).
 - Fix spelling error, #989 (@caraka).

## [3.7.8.0] 2018-03-01, mandatory
### Fixed
 - Move context sensitive DPoR block checks to ConnectBlock, #922 (@tomasbrod).
 - Check incoming blocks for malformed DPoR signature, #922.
 - Corect tally height on init, #917 (@denravonska).
 - Prevent staking of a block with a failed signature, #948 (@Foggyx420).
 - Fix UI and RPC slowdown regression, #961 (@denravonska).
 - Fix Debian lint errors, #886, #885, #884, #883 (@caraka).
 - Fix fork issue due to research age calculation inconsistencies, #939
   (@denravonska).
 - Fix crashes when tallying, #934 (@denravonska).
 - Revert reorganize of the chain trust becomes less than what it was, #957
   (@tomasbrod).
 - Fix sync issues with incorrectly accepted v8 beacons, #979 (@tomasbrod).

### Changed
  - Double check PoS kernel, #958 (@tomasbrod).
  - Don't tally until V9 to speed up syncing, #943 (@denravonska).

## [3.7.7.0] 2018-02-02
### Fixed
 - Beacon validation are now done when accepting blocks, not when receiving,
   #899 (@denravonska).
 - Fix crashes due to buffer overflow in encrypt/decrypt, #890 (@denravonska).
 - Rewrite reorganize routine to be more reliable and drop contracts received
   or issued while on a side chain to help reducing forks, #902 (@tomasbrod).

## [3.7.6.0]
Internal test version used to sort out the forks.

## [3.7.5.0] 2018-01-24
### Fixed
 - Fix crash when switching to new tally on block 1144120, #868 (@denravonska).
 - Fix crash when staking while tallying, #866 (@denravonska).

## [3.7.4.0] 2018-01-20
### Fixed
 - Fix RPC resource leak regression. This also reduces RPC overhead,
   making calls ~25-35% faster, #848 (@denravonska).
 - Fix incorrect return code when forking, #832 (@denravonska).

### Removed
 - Remove upgrader option until rewritten, #836 (@Foggyx420).

## [3.7.3.0] 2018-01-13
### Fixed
 - Fix for UI getting stuck in splash screen (@denravonska).

## [3.7.2.0] 2018-01-13
### Fixed
 - Properly fix for wallet not daemonizing, #822 (@denravonska).

## [3.7.1.0] 2018-01-10
### Fixed
 - Fix several crashes in diagnostic dialog, #816 (@Foggyx420).
 - Fix client not exiting when running as daemon (@denravonska).
 - Fix issue with boincstake.dll not updating on dirty installs (@Foggyx420).

### Changed
 - Update splash screen, #685 (acey1).

## [3.7.0.0] 2018-01-08
### Added
 - Provide Difficulty of best kernel found, #766 (@tomasbrod).
 - Add Travis support for OSX, 665 (@acey1).
 - Add better command for sending alerts, #731 (@tomasbrod).
 - Add RPC for sending raw contracts, #683 (@tomasbrod).
 - Add portable diagnostic page, #631 (@fooforever).

### Fixed
 - Fixed minor spelling mistakes, #742 (@denravonska).
 - Several tally improvements. There should now be less forking and the wallet
   should use ~50MB less memory, #668, #756 (@denravonska, @tomasbrod)
 - Data scraper can no longer run concurrently, #742 (@denravonska).
 - Improve superblock validations, #730 (@tomasbrod).
 - Fix potential deadlock, #708 (@denravonska).
 - Prevent duplicate superblocks, #534 (@tomasbrod).
 - Fix issue with application cache clears, #577 (@tomasbrod).
 - Fix bug which caused rewards to be lost when staking the newbie block.
   Missing rewards will be reimbursed, #552 (@Foggyx420).
 - Fix minor UI typos, #661 (@Erkan-Yilmaz).
 - Fix stake modifier, #686 (@tomasbrod).
 - Improve boost-1.66.0 compatibility, #800 (@denravonska).
 - Fix crash in diagnostics dialog, #794 (@Foggyx420).

### Changed
 - Changed versioning extraction from git. Test builds can no longer be used to
   stake in production unless explicitly enabled, #729 (@tomasbrod).
 - Don't update network quorum while syncing, #728 (@Foggyx420).
 - Snapshot URL now uses https, #727 (@Foggyx420).
 - Code cleanup (@Foggyx420, @denravonska).
 - Use more efficient data structure for blocks, #679 (@denravonska).
 - Improve transaction description dialog, #676 (@Foggyx420).
 - Improve beacon handling, #604, #645, #649, #684, #701 (@Foggyx420, @tomasbrod).
 - Optimize double<->string conversions, #692 (@denravonska).
 - Optimize application cache access, #506 (@denravonska).
 - Improve thread handling, #656 (@skcin).
 - Replace `boost::shared_ptr` with `std::shared_ptr`.
 - Optimize string split function, #672 (@denravonska).
 - Improve sync speeds, #650 (@denravonska).
 - The RPC command `restartclient` is now called `restart`.
 - Fix voting sorting issues, #610 (@MagixInTheAir).
 - Improve wallet backup, #610 (@Foggyx420).
 - Update seed nodes, #783 (@barton2526).
 - Auto upgrades are now opt-in via the "autoupgrade" flag, #796 (@denravonska).
 - Clean up seed nodes, #783 (@barton2526).

### Removed
 - Remove CSV exporter which used unreliable data, #759 (@denravonska).
 - Remove block download menu options on non-Windows, #727 (@Foggyx420).
 - Removed RPC commands (@Foggyx420, @denravonska):
    - debugexplainmagnitude
    - executecode
    - getsubsidy
    - list newbieage
    - list staking
    - leder
    - reboot
 - Remove checkpoint relaying to improve sync speeds, #678 (@denravonska).
 - Remove IRC peer discovery.

## [3.6.3.0] 2017-10-09
### Fixed
 - Fix problems sending beacons on Windows, #684 (@tomasbrod).
 - Fix clients getting stuck at V8 blocks when syncing, #686 (@tomasbrod).

## [3.6.2.0] 2017-09-28
### Added
 - Add "backupprivatekeys" RPC command, #593 (@Foggyx420).
 - Add more transaction details to the UI, #573 (@tomasbrod).
 - Add Additional logging to diagnose PoR reward loss (@tomasbrod)

### Fixed
 - Reduce startup time by 15 seconds, #626 (@tomasbrod, @Foggyx420).
 - Prevent email being leaked in CPIDv2 block field, #621 (@tomasbrod).
 - Fixed memory leaks when receiving orphans while in sync, #622 (@denravonska).
 - Unconfirmed balance was not shown in UI, #615, (@Foggyx420).
 - Fix memory leaks when clearing orphans, #609, (@MagixInTheAir).
 - Fix an issue where multiple beacons could be advertised in rapid
   succession, #604 (@Foggyx420).
 - Stake weight in the UI will no longer include old DPOR weght, #602
   (@Foggyx420). 
 - Fix stake modifier mismatch which caused nodes to get stuck on first
   V8 block, #581 (@tomasbrod).
 - Fix beacon auto advertisment issue when done automatically, #580 (@Foggyx420). 
 - Fix for loss of PoR rewards due to reorganize, #578 (@tomasbrod).
 - Fix upgrader compile error on Linux, #541 (@theMarix).
 - Fix duplicate poll entries, #539 (@denravonska).
 - Importing private keys will no longer require a restart for the addresses
   to show up, #634 (@Foggyx420).
 - Fix invalid backup filenames on Windows, #569 (@denravonska).

### Changed
 - Code cleanup (@Foggyx420, @tomasbrod, @denravonska).
 - Several NN consensus sync improvements, #616 (@Foggyx420).
 - Windows nodes will no longer automatically reboot/shutdown, #605
   (@denravonska).
 - Display "No Polls!" in poll window if no polls are running, #596
   (@MagixInTheAir).
 - Change poll min search length from 2 to 1, #595 (@MagixInTheAir).
 - Return the results of "backupwallet" RPC command, #593 (@Foggyx420).
 - Changing the community links #654 (@grctest)

## [3.6.1.0] 2017-09-11
### Fixed
 - Fix problems forging superblock due to rounding differences, #608 (@denravonska).
 - Fetch data from project servers if missing on scraper #564 (@denravonska).

## [3.6.0.2] 2017-08-26
### Fixed
 - Fix incorrect V8 height trigger check. Many thanks to @barton2526 for discovering this.
 - Fix invalid superblock height formatting, #532 (@denravonska).
 - Fix several spelling mistakes, 533 (@Erkan-Yilmaz).

## [3.6.0.1] 2017-08-22
### Added
 - Added [V8 stake engine](https://github.com/gridcoin/Gridcoin-Research/wiki/Stake-V8)
   set to start producing V8 blocks at block 1010000. This fixes several security issues,
   see wiki for details.
 - Blocks can now carry identification from the "org" argument/configuration option (@tomasbrod).
 - Add "reorganize" RPC command (@tomasbrod).

### Changed
 - Berkeley DB V6+ compatibility, #451 (@xPh03n1x).
 - Improved poll loading speeds, #497 (@denravonska).
 - Versions now contain the git hash, #500 (@tomasbrod).
 - Improved security on NeuralNet votes, #496 (@Foggyx420).
 - Improved RPC help. It now supports "execute help" and "list help",
   #512 (@Foggyx420).
 - Voting is now integrated in wallet as a tab and cleaned up, #416 (@skcin, @JoShoeAh).
 - Improve low-peer mining ability on testnet (@tomasbrod).
 - Improve poll error message when low on funds, #415 (@Erkan-Yilmaz).
 - Code cleanup (@denravonska, @tomasbrod, @Foggyx420, @skcin).

### Removed
 - Remove RPC commands:
    - DAO, #486 (@denravonska).
    - volatilecode, testnet0917, testboinckey, chainrsa, testcpidv2, testcpid, windows
      error report disabling, list betatest, fDebug4/fDebug5 flags (@Foggyx420).
 - Set magnitude boost to be removed at 2017-Sep-07 00:00:00 UTC

### Fixed
 - Fixed security issue where superblocks could be injected, #526 (@tomasbrod).
 - Fix poll sorting bug, #512 (@skcin)

## [3.6.0.0] - 2017-08-14
### Fixed
 - Fix a crash when starting up as a new user, #488 (@Foggyx420, 
   @denravonska).
 - Fix an out of memory crash when syncing from 0, #508 (@tomasbrod).

## [3.5.9.9] - 2017-08-05
### Changed
 - Staking cleanup, #301 (@tomasbrod). This also solves several other issues:
 - UI:
    - Wallet window can now be made smaller, #384 (@skcin). 
    - Interest and Research subsidy visible in getmininginfo (@tomasbrod).
    - External links now use HTTPS where possible, and the code has been cleaned
      up, #339 (@skcin).
    - Rearrange menu items to reduce the number of entries. Remove references
      to broken function, #362 (@skcin).
 - Replace translations which were just question marks with new files from
   the Bitcoin source tree: Arabic, Belarusian, Bulgarian, Greek, Persian,
   Hebrew, Hindi, Japanese, Georgian, Kirghiz, Serbian, Thai, Ukranian,
   Urdu and Chinese.
 - Don't print the "Bootup" and "Signing block" messages unless fDebug (@tomasbrod). 
 - Print beacons as they are loaded and debug3=true (@tomasbrod).
 - Show superblock information in getblock (@tomasbrod).
 - Code cleanup (@skcin).
 - Update Lithuanian translations, #469 (@Rytiss).
 - Add block size min, max, avg to block stats RPC (@tomasbrod).
 - Fields on overview page are now selectable.

### Fixed
 - High CPU usage, #349 (@tomasbrod)
 - Repetetive block signing, #295 (@tomasbrod)
 - Staking creates 1 cent output, #311 (@tomasbrod)
 - Client no longer has to be restarted for a beacon to activate, #253
   (@Foggyx420).
 - Fixed a coin age bug which made it hard to stake on testnet (@denravonska)
 - Fixed reloading of polls in the voting GUI, #431 (@skcin) 
 - Fix crash when listing receivedby on addresses with no transactions,
   #456 (@denravonska).
 - Fix buffer overflow in TX message unscambling, #468 (@tomasbrod).
 - Splash screen can no longer be dismissed and the UI can no longer be shown
   until the wallet has fully loaded, #353 (@denravonska).

### Removed
 - Removed newbie boost, #332
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
 - Add support for themes via stylesheets, #233 (@skcin).
 - Add support for OpenSSL 1.1.x, #164.
 - BOINC data dir auto detection, #242 (@3ullShark)
 - Add install (make install) target for UNIX systems.
 - Add aarch64 support, #151 (@datenklause).
 - Add diagnostic message for if web lookup fails for cpid valid test,
   #175 (@fooforever).

### Changed
 - Wallet overview cleanup, #233 (@skcin)
    - The main overview page is now cleaner, more structured and holds more of
     the recent transactions.
    - Displayed DPOR weight should now be accurate, #233 (@skcin).
 - Show as many of the recent transactions as we can fit on the overview page.
 - Translation updates
    - Portuguese (Miguel Veiga)
    - Slovak (@tomasbrod)
    - Swedish (@denravonska)
    - Afrikaan and Spanish (@philipswift)
    - French (@PsiPhiTheta)
    - Russian (@rambinho)
 - Gridcoinstats is now used as block explorer, #308.
 - Slight RAM usage reduction.
 - Improve beacon advertise error message, #133 (@comprehendreality).
 - Code cleanup (@Foggyx420, @TheCharlatan).

### Fixed
 - Fix numerous beacon issues, #344, #321 and #334 (@Foggyx420).
 - Fix incorrect WCG URL, #323 (@3ullShark).
 - Fix alt key shortcut order, #326 (@TheCharlatan).
 - Fix a bug where beacons were stored even though none were generated due
   to the wallet being locked, #264 (@denravonska).

### Removed
 - Remove empty "wcgtest" RPC command.

### Security
 - Security enhancement (@tomasbrod)
 - Upgraded security on voting system - voting proof of balance and proof of
   magnitude.

## [3.5.8.9] - 2017-05-15
### Added
- Implement voting functionality for Linux and OSX (@skcin).
- Add man pages to doc folder, #135 (@caraka).

### Changed
 - Windows are now resizable 
 - Replace Windows voting dialog with the new dialog.
 - Update Gridcoin icon on Windows.
 - Enable C++11.
 - Update Hungarian translations (@matthew11).
 - Update Portuguese translations (Miguel Veiga).
 - Update icon set by @Peppernrino.
 - Update icon on OSX, #193 (@coagmano).
 - Lossless compression of resources, #227 (@Peppernrino).
 - Reduced memory usage by around 100MB+.
 - Improve UI when used with dark themes on Linux, #222 (@skcin).

### Fixed
 - Fix OSX build issues, #174 (@coagmano).
 - Fix occasional crashes when starting on Linux, #139.
 - Fix freeze when clicking on the "Amount" field under Send Coins when using
   KDE, #210.
 - Possible fix for invalid time check in diagnostic.

### Removed
 - Remove lots of dead, obsolete code.
 - Removed unused link dependencies: librt, boost_chrono, boost_date_time, libz
   and libdl.
