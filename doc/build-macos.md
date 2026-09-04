# Building Gridcoin for macOS

This guide describes how to build the Gridcoin macOS client (Qt6) from source. These instructions match the official build process used in our CI/CD pipelines.

## Automatic Build Script

We have created a fairly comprehensive and easy to use build script that automates the macOS build (among other targets), ***build_targets.sh***, and its helper script, ***install_dependencies.sh***.

```
./build_targets.sh -h
Usage: ./build_targets.sh [OPTIONS]

Options:
  TARGET=<target>     Select build target. Options: native, depends, win64, all.
                      Default: all
  BUILD_TYPE=<type>   Set the CMake build type. Options: Release, Debug, RelWithDebInfo.
                      Default: RelWithDebInfo
  CLEAN_BUILD=<bool>  Force a clean build even if executables exist. Options: true, false.
                      Default: false
  SKIP_DEPS=<bool>    Skip installing system dependencies (step 1). Options: true, false.
                      Default: false
  USE_CCACHE=<bool>   Enable ccache compiler launcher. Options: true, false.
                      Default: false
  USE_QT6=<bool>      Use Qt6 for Linux Native build. Options: true, false.
                      Default: true (Set to false for Qt5)
  PARALLEL=<int>      Specify number of build threads to use (i.e. -j X).
                      Default: number of cpu threads reported by OS
  CC=<path>           Override C compiler for Native Linux build.
                      (e.g., CC=/usr/bin/gcc-13).
  CXX=<path>          Override C++ compiler for Native Linux build.
                      (e.g., CXX=/usr/bin/g++-13).
  --help, -h          Show this help message.
```

**macOS 14 (Sonoma) or newer is required.**

If you use the build helper script above:

1. Ensure that you clone the repo and select the desired branch using step 3 below (if not already done)
2. Run the build_targets.sh helper script. Here is an example command:

```
./build_targets.sh TARGET=macos BUILD_TYPE=Release CLEAN_BUILD=true USE_CCACHE=true
```

3. Run the application using step 6 below and/or create the installation package via step 7.

## Step-by-step

### Prerequisites

1. macOS 14 (Sonoma) or newer is required.
2. Xcode Command Line Tools: Install these by running the following in your terminal:

```
xcode-select --install
```

3. Homebrew: The standard package manager for macOS (see [https://brew.sh](https://brew.sh)). If you don't have it, install it by running the following in your terminal:

```
/bin/bash -c "$(curl -fsSL [https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh](https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh))"
```

### 1. Install Dependencies

We use Homebrew to install the required libraries (Qt6, Boost, OpenSSL, etc.) and build tools.

Open your terminal and run:

```
brew update
brew install qt boost openssl libevent miniupnpc qrencode libzip ccache cmake
```

Note: Homebrew installs Qt6 by default when you request qt.

Note for Intel Macs: Homebrew no longer ships x86_64 bottles for many
formulae (Tier 3 wind-down), and modern brew builds those from source
automatically, which for the qt meta-formula means an infeasible
qtwebengine build. Install the Qt subset instead of qt:

```
brew install qtbase qttools qtsvg qttranslations boost openssl libevent miniupnpc qrencode libzip ccache cmake
```

and pass the brew prefix (e.g. `-DCMAKE_PREFIX_PATH=$(brew --prefix)`) so
CMake sees the aggregate linked Qt view. CI uses
contrib/devtools/brew-install-with-source-fallback.sh to source-build and
cache the bottle-less formulae; it works locally too.

### 2. Get the Source Code

Clone the repository and enter the directory (substitute the desired branch if not master):

```
git clone [https://github.com/gridcoin-community/Gridcoin-Research.git](https://github.com/gridcoin-community/Gridcoin-Research.git)
cd Gridcoin-Research
git checkout master
```

### 3. Configure the Build

We need to tell CMake where Homebrew installed Qt and OpenSSL. We do this dynamically using brew --prefix.
Create a build directory and configure the project by running the following in the terminal:

```
# Set paths for Homebrew libraries
QT6_PATH=$(brew --prefix qt)
OPENSSL_ROOT=$(brew --prefix openssl)

# Configure CMake
cmake -B build \
  -DCMAKE_PREFIX_PATH="$QT6_PATH" \
  -DENABLE_GUI=ON \
  -DENABLE_QRENCODE=ON \
  -DENABLE_UPNP=ON \
  -DENABLE_TESTS=ON \
  -DUSE_QT6=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

#### Build Types
 * RelWithDebInfo (Recommended): Optimized, but includes debug symbols.
 * Release: Fully optimized, smaller binaries.
 * Debug: Unoptimized, useful for development and debugging.

### 4. Compile

Start the compilation process. This will use all available CPU cores automatically. Note that Gridcoin compilationi requires about 1 GB per core when parallel compiling, so if you are limited on memory, you may want to substitute a lower number than is automatically determined by sysctl -n hw.logicalcpu.

```
cmake --build build_macos -j $(sysctl -n hw.logicalcpu)
```

### 5. Run Tests (Optional)

It is highly recommended to run the self-tests to ensure the binary is functioning correctly.

```
ctest --test-dir build_macos --output-on-failure
```

### 6. Run the Application

You can run the application directly from the build folder:

```
./build_macos/bin/gridcoinresearch.app/Contents/MacOS/gridcoinresearch
```

### 7. Create the DMG Installer (Optional)

If you want to create a distributable disk image (.dmg), you can use CPack (included with CMake).

```
cpack -G DragNDrop --config build/CPackConfig.cmake -B release_packages
```

The resulting .dmg file will be located in the release_packages directory.
