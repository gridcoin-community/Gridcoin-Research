# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]
### Added
- Implement voting functionality for Linux and OSX (@skcin).

### Changed
- Windows are now resizable 
- Replace Windows voting dialog with the new dialog.
- Update Gridcoin icon on Windows.
- Enable C++11.
- Removed unused link dependencies: librt, boost_chrono, boost_date_time, libz
  and libdl.
- Update Hungarian translations (@matthew11).
- Update Portuguese translations (Miguel Veiga).
- Update icon set by @Peppernrino.
- Update icon on OSX, #193 (@coagmano).
- Lossless compression of resources, #227 (@Peppernrino).
- Reduced memory usage by around 100MB+.
- Improve UI when used with dark themes on Linux, #222 (@skcin).
- Fix occasional crashes when starting on Linux, #139.
- Fix freeze when clicking on the "Amount" field under Send Coins when using
  KDE, #210.
- Fix OSX build issues, #174 (@coagmano).
- Add man pages to doc folder, #135 (@carak).
- Remove lots of dead, obsolete code.
