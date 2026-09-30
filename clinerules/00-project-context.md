# Gridcoin-Research Codebase Context

## Project Overview
This is the Gridcoin-Research cryptocurrency project - an open-source blockchain platform that rewards users for contributing computational power to scientific research through BOINC (Berkeley Open Infrastructure for Network Computing).

## Current Version Status
- **Latest Stable Release**: 5.4.9.0 - "leisure"
- **Development Version**: 5.4.9.8 (current development/testing)
- **Next Major Release**: Natasha (5.5.0.0) - Mandatory upgrade, in testnet/development
- **Build Version in CMakeLists.txt**: 5.4.9.8 (increments for CI/testing purposes)
- **Note**: Version numbers between releases may increment for CI/testing purposes

## Codebase Statistics (Last Updated: December 23, 2025)
**Note**: Statistics are approximate and updated periodically. They represent a snapshot of the codebase at the time of calculation.

### Overall Statistics
- **Total Files:** 10,814
- **Total Size:** 477.59 MB

### Source Code Breakdown

#### Core C/C++ Source Files
- **File Types:** .cpp, .h, .c, .cc
- **File Count:** 1,763 files
- **Size:** 6.14 MB (6,439,684 bytes)
- **Estimated Tokens:** ~1.6M tokens

#### Build & Script Files
- **File Types:** .py, .js, .sh, .am, .ac, .cmake, CMakeLists.txt, Makefile*
- **Size:** 2.12 MB (2,228,113 bytes)
- **Estimated Tokens:** ~557K tokens

#### Documentation Files
- **File Types:** .md, .txt, .rst
- **Size:** 0.88 MB (919,196 bytes)
- **Estimated Tokens:** ~230K tokens

### Credit Ingestion Estimate
- **Total Relevant Source Code:** 9.14 MB (9,586,993 bytes)
- **Estimated Tokens for Full Ingestion:** ~2.4 million tokens
- **Approximate Credits Needed:** 2.4M credits (using 1:1 token-to-credit ratio)

## Key Directories
- `src/` - Main C++ source code
  - `src/gridcoin/` - Gridcoin-specific functionality
  - `src/qt/` - Qt GUI components
  - `src/rpc/` - RPC interface
  - `src/test/` - Unit tests
- `doc/` - Documentation
- `contrib/` - Contribution tools and scripts
- `depends/` - Build dependencies
- `build-aux/` - Build automation files
- `.github/workflows/` - GitHub Actions CI/CD configuration

## Build System
- **CMake** (CMakeLists.txt) - Sole build system since the 5.5.0.0 "Natasha" release

## Programming Languages
- Primary: C++ (cryptocurrency core)
- Secondary: Python (build/test scripts), Shell scripts, JavaScript

## Third-Party Dependencies

### Core Dependencies (Required)

- **Boost 1.89.0** (minimum 1.63.0) - C++ utility libraries for filesystem, threading, iostreams, serialization, and date_time operations
  - Location: External via depends/ or system
  - Components: filesystem, iostreams, thread, serialization, date_time, interprocess, test framework

- **OpenSSL 1.1.1l** - Limited cryptographic use (minimal dependency)
  - Location: External via depends/ or system
  - Note: OpenSSL usage has been significantly reduced; configured without many optional ciphers for reduced attack surface

- **Berkeley DB 5.3.x** - Wallet database storage (BDB CXX)
  - Location: Bundled in src/bdb53/ or system
  - Note: Version 5.3 specifically required for wallet compatibility

### Bundled Libraries (Included in src/)

- **LevelDB** - High-performance key-value store for blockchain index database
  - Location: src/leveldb/
  - Used for: Block index, transaction index, and other blockchain data

- **libsecp256k1** (minimum 0.2.0) - Bitcoin's elliptic curve cryptography library for ECDSA signatures
  - Location: src/secp256k1/
  - Used for: Transaction signing and verification

- **UniValue** - JSON parsing and manipulation library
  - Location: Bundled or external via depends/
  - Used for: RPC interface and configuration

- **CRC32C** - Hardware-accelerated CRC32C checksum library
  - Location: src/crc32c/
  - Used for: Data integrity verification

### Network & Protocol Libraries

- **libcurl 7.88.1** - HTTP/HTTPS client library
  - Location: External via depends/ or system
  - Used for: BOINC project communication and statistics

- **libevent 2.1.12-stable** - Event notification library for asynchronous I/O
  - Location: External via depends/ or system
  - Used for: HTTP server, RPC server, and network event handling

- **libzip 1.11.1** - ZIP archive handling
  - Location: External via depends/ or system
  - Used for: Snapshot handling and compressed data

- **miniupnpc 2.2.2** (minimum 1.9) - UPnP port mapping client (Optional)
  - Location: External via depends/ or system
  - Used for: Automatic port forwarding configuration
  - Build flag: ENABLE_UPNP

### GUI Dependencies (Optional - Qt-based GUI only)

- **Qt 6.7.3** (Qt6 minimum 6.2.0, Qt5 minimum 5.9.5) - Cross-platform GUI framework
  - Location: External via depends/ or system
  - Components: qtbase, qttools, qttranslations, qtsvg, qt5compat
  - Required modules: Core, Concurrent, Gui, LinguistTools, Network, Widgets, Svg
  - Build flag: ENABLE_GUI, USE_QT6
  - **Default**: Qt5 is used by default; Qt6 requires explicit `USE_QT6=ON` flag

- **QRencode 4.1.1** - QR code generation library (Optional)
  - Location: External via depends/ or system
  - Used for: Generating QR codes for receiving addresses
  - Build flag: ENABLE_QRENCODE

### Compression Libraries

- **zlib 1.3.1** - General-purpose compression library
  - Location: External via depends/ or system

- **bzip2 1.0.8** - Block-sorting file compressor
  - Location: External via depends/ or system

- **xz 5.2.5** (liblzma) - LZMA compression library
  - Location: External via depends/ or system

### Text Processing & Parsing

- **expat 2.4.8** - XML parsing library
  - Location: External via depends/ or system

- **PCRE2 10.45** - Perl-compatible regular expressions (Qt dependency)
  - Location: External via depends/ or system

### Linux GUI Support Libraries

These dependencies are required for building the Qt GUI on Linux systems:

- **freetype 2.11.0** - Font rendering engine
- **fontconfig 2.12.6** - Font configuration and customization
- **harfbuzz 11.2.1** - Text shaping engine
- **libpng 1.6.48** - PNG image support
- **libxcb 1.17.0** - X11 C Bindings for X Window System communication
- **libxkbcommon 0.8.4** - Keyboard handling library
- **xcb utilities** (0.3.9-0.4.1) - Various XCB utility libraries (cursor, image, keysyms, render, wm)
- **X11 protocol libraries** - libXau, libXdmcp, libICE, libSM

### Build & Development Tools

- **CMake 3.18+** - Build system generator
- **pkg-config** - Library metadata helper
- **Python 3** - Build scripts and utilities

### Platform-Specific Dependencies

**Hunter Package Manager** (Windows, optional):
- Version: 0.25.8
- Used for: Managing dependencies on Windows when system packages unavailable
- Build flag: HUNTER_ENABLED

**Native Build Tools**:
- **ccache 3.7.12** - Compiler cache for faster rebuilds (optional)
- **xxd 9.1 .0** - Hex dump utility for embedding resources
- **dmg tools** (macOS) - For creating macOS disk images

## Build System Configuration

The project supports two build systems:
- **CMake** (primary, experimental) - Requires external dependencies only
- **Autotools** (legacy) - Supports both bundled and external dependencies

Dependencies can be:
- Installed from system package manager
- Built from source via the `depends/` directory
- Managed by Hunter on Windows (when HUNTER_ENABLED=ON)

## Notes
- This is a Bitcoin-derived codebase with significant modifications for Gridcoin's unique proof-of-stake and research reward mechanisms
- Many dependencies are optional and controlled by build flags (ENABLE_GUI, ENABLE_UPNP, etc.)
- The `depends/` directory provides a deterministic build system for cross-compilation
- Minimum versions are specified for external dependencies to ensure compatibility
- Token estimates exclude binary files, build artifacts, and third-party compiled libraries
