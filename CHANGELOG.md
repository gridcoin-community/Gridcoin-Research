# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

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
 - Client no longer has be restarted for a beacon to activate, #253
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
