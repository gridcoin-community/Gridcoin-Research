#!/usr/bin/env bash

export LC_ALL=C.UTF-8

set -e # Exit immediately if a command exits with a non-zero status.

# Error handling trap
trap 'echo "Error occurred on line $LINENO. Exiting..." >&2' ERR

# ==============================================================================
# HELPER FUNCTIONS
# ==============================================================================

print_help() {
    echo "Usage: ./build_targets.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  TARGET=<target>     Select build target. Options: native, depends, win64, macos, all."
    echo "                      Default: all"
    echo "  BUILD_TYPE=<type>   Set the CMake build type. Options: Release, Debug, RelWithDebInfo."
    echo "                      Default: RelWithDebInfo"
    echo "  CLEAN_BUILD=<mode>  Clean build behavior."
    echo "                      Options:"
    echo "                        true  : Full clean (wipes build dir AND depends artifacts)"
    echo "                        main  : Main clean (wipes build dir, KEEPS depends artifacts)"
    echo "                        false : Smart build (skips if version/commit matches)"
    echo "                      Default: false"
    echo "  SKIP_DEPS=<bool>    Skip installing system dependencies (step 1). Options: true, false."
    echo "                      Default: false"
    echo "  USE_CCACHE=<bool>   Enable ccache compiler launcher. Options: true, false."
    echo "                      Default: false"
    echo "  WITH_GUI=<bool>     Build the GUI wallet. Options: true, false."
    echo "                      Default: true"
    echo "  WITH_DOCS=<bool>    Build Doxygen documentation. Options: true, false."
    echo "                      Default: false"
    echo "  USE_QT6=<bool>      Use Qt6 for Native/macOS build. Options: true, false."
    echo "                      Default: true (Set to false for Qt5)"
    echo "  PARALLEL=<int>      Specify number of build threads to use (i.e. -j X)."
    echo "                      Default: number of cpu threads reported by OS"
    echo "  QT_PATH=<path>      Override path to Qt root (e.g. /usr/local/Qt/6.6.0/macos)."
    echo "                      Bypasses Homebrew detection if set."
    echo "  DEBUG_LOCKORDER=<bool> Enable run-time lock-order checking. Options: true, false."
    echo "                      Default: false"
    echo "  EXTRA_CMAKE_ARGS    Pass additional arguments to CMake (e.g. '-DBoost_USE_STATIC_LIBS=ON')"
    echo "  CC=<path>           Override C compiler (also read from the CC environment variable)."
    echo "  CXX=<path>          Override C++ compiler (also read from the CXX environment variable)."
    echo "  --help, -h          Show this help message."
    echo ""
}

# Get the current robust git version string (hash + dirty status)
get_current_git_state() {
    # .git is a directory in a primary checkout but a plain FILE in linked
    # worktrees (git worktree) and submodules, so ask git itself rather than
    # testing for the directory. The state is trusted only when the current
    # directory IS the work-tree root: being merely *inside* some enclosing
    # repo (e.g. a tarball extracted under a git-managed home directory)
    # would silently compute the parent repo's state. Anything else (no git,
    # not a work tree, wrong root) yields "unknown", which should_skip_build
    # treats as never skippable. The root is accepted by either its physical
    # (pwd -P) or logical ($PWD) path, since git's canonicalization of
    # symlinked paths has differed across versions/platforms; a false
    # negative here only costs a rebuild, never a wrong skip.
    # Every git command substitution is ||-guarded so a failure (no git, not
    # a repo) degrades to "unknown" instead of aborting the script under
    # set -e, regardless of the calling context.
    local TOPLEVEL
    TOPLEVEL=$(git rev-parse --show-toplevel 2>/dev/null) || TOPLEVEL=""
    if [ -n "$TOPLEVEL" ] && { [ "$TOPLEVEL" = "$(pwd -P)" ] || [ "$TOPLEVEL" = "$PWD" ]; }; then
        local DESCRIBE
        DESCRIBE=$(git describe --always --dirty --abbrev=12 --exclude='*' 2>/dev/null) || DESCRIBE=""

        if [ -z "$DESCRIBE" ]; then
            # e.g. unborn HEAD (git init with no commits): indeterminate.
            echo "unknown"
            return 0
        fi

        # A bare "-dirty" suffix is content-independent: two different sets of
        # uncommitted changes on the same commit yield the same state string,
        # so a recorded dirty build state could wrongly skip a build of
        # different working-tree content. Disambiguate by folding a hash of
        # the tracked diff (staged + unstaged, vs HEAD) into the state.
        # Untracked files are ignored, matching --dirty semantics. git
        # hash-object is used instead of sha256sum for macOS portability.
        case "$DESCRIBE" in
            *-dirty)
                local DIFF_TMP
                local DIFF_HASH
                local STATE_OUT
                # Write the diff to a temp file rather than a shell variable
                # (a --binary diff can be large) and check its exit status: a
                # failed diff must not be hashed as partial output, which
                # could wrongly MATCH a recorded state. On any failure emit a
                # never-matching token so the error degrades to a rebuild.
                DIFF_TMP=$(mktemp 2>/dev/null) || {
                    echo "${DESCRIBE}-difffail-$$-$(date +%s)"
                    return 0
                }
                if git diff --no-ext-diff --no-color --binary HEAD >"$DIFF_TMP" 2>/dev/null; then
                    DIFF_HASH=$(git hash-object -- "$DIFF_TMP" 2>/dev/null | cut -c1-12) || DIFF_HASH=""
                fi
                if [ -n "$DIFF_HASH" ]; then
                    STATE_OUT="${DESCRIBE}-${DIFF_HASH}"
                else
                    STATE_OUT="${DESCRIBE}-difffail-$$-$(date +%s)"
                fi
                rm -f "$DIFF_TMP"
                echo "$STATE_OUT"
                ;;
            *)
                echo "$DESCRIBE"
                ;;
        esac
    else
        echo "unknown"
    fi
}

# Git state captured by should_skip_build at skip-decision time (i.e., just
# before a build starts) and recorded by write_build_state on success.
CAPTURED_BUILD_STATE=""

# Check if we can skip the build
# Usage: should_skip_build "BUILD_DIR" "FILE_1" "FILE_2" ...
# Returns 0 (true) if we should skip, 1 (false) if we must build
should_skip_build() {
    local BUILD_DIR=$1
    shift

    local CURRENT_STATE
    CURRENT_STATE=$(get_current_git_state) || CURRENT_STATE=""

    # Capture now for write_build_state: recomputing the state AFTER the
    # build would silently record source edits made while the build ran as
    # built. Recording the pre-build snapshot instead means such edits leave
    # the recorded state stale and the next run rebuilds -- the safe
    # direction.
    CAPTURED_BUILD_STATE="$CURRENT_STATE"

    # An indeterminate state (not a git work-tree root, git unavailable, or
    # an empty result) cannot prove the artifacts match the sources: never
    # skip. The -z test is defense-in-depth; get_current_git_state already
    # maps empty describe output to "unknown".
    if [ -z "$CURRENT_STATE" ] || [ "$CURRENT_STATE" == "unknown" ]; then
        return 1
    fi

    # If explicit clean requested, never skip
    if [ "$CLEAN_BUILD" == "true" ] || [ "$CLEAN_BUILD" == "main" ]; then
        return 1
    fi

    # Check that ALL passed executables exist
    for EXE in "$@"; do
        if [ ! -f "$EXE" ]; then
            return 1
        fi
    done

    # If no state file from previous build, never skip
    if [ ! -f "$BUILD_DIR/.build_state" ]; then
        return 1
    fi

    local LAST_STATE
    LAST_STATE=$(cat "$BUILD_DIR/.build_state")

    if [ "$CURRENT_STATE" == "$LAST_STATE" ]; then
        echo ">>> Build up-to-date ($CURRENT_STATE). Skipping..."
        return 0
    fi

    return 1
}

# Write the state captured at skip-decision time upon success (see
# should_skip_build). Falls back to "unknown" -- never skippable -- if no
# capture happened.
write_build_state() {
    local BUILD_DIR=$1
    echo "${CAPTURED_BUILD_STATE:-unknown}" > "$BUILD_DIR/.build_state"
}

# ==============================================================================
# ARGUMENT PARSING
# ==============================================================================

# Defaults
TARGET="all"
BUILD_TYPE="RelWithDebInfo"
CLEAN_BUILD="false"
SKIP_DEPS="false"
USE_CCACHE="false"
WITH_GUI="true"
WITH_DOCS="false"
USE_QT6="true"
# Seed from the conventional CC/CXX environment variables; a positional
# CC=/CXX= argument (parsed below) still overrides them.
CC_OVERRIDE="${CC:-}"
CXX_OVERRIDE="${CXX:-}"
MANUAL_QT_PATH=""
DEBUG_LOCKORDER="false"
EXTRA_ARGS=""

for arg in "$@"; do
    case $arg in
        TARGET=*)
            TARGET="${arg#*=}"
            shift
            ;;
        BUILD_TYPE=*)
            BUILD_TYPE="${arg#*=}"
            shift
            ;;
        CLEAN_BUILD=*)
            CLEAN_BUILD="${arg#*=}"
            shift
            ;;
        SKIP_DEPS=*)
            SKIP_DEPS="${arg#*=}"
            shift
            ;;
        USE_CCACHE=*)
            USE_CCACHE="${arg#*=}"
            shift
            ;;
        WITH_GUI=*)
            WITH_GUI="${arg#*=}"
            shift
            ;;
        WITH_DOCS=*)
            WITH_DOCS="${arg#*=}"
            shift
            ;;
        USE_QT6=*)
            USE_QT6="${arg#*=}"
            shift
            ;;
        PARALLEL=*)
            PARALLEL="${arg#*=}"
            shift
            ;;
        QT_PATH=*)
            MANUAL_QT_PATH="${arg#*=}"
            shift
            ;;
        DEBUG_LOCKORDER=*)
            DEBUG_LOCKORDER="${arg#*=}"
            shift
            ;;
        EXTRA_CMAKE_ARGS=*)
            EXTRA_ARGS="${arg#*=}"
            shift
            ;;
        CC=*)
            CC_OVERRIDE="${arg#*=}"
            shift
            ;;
        CXX=*)
            CXX_OVERRIDE="${arg#*=}"
            shift
            ;;
        --help|-h)
            print_help
            exit 0
            ;;
        *)
            echo "Error: Unknown argument '$arg'"
            print_help
            exit 1
            ;;
    esac
done

# Validate Target
if [[ ! "$TARGET" =~ ^(native|depends|win64|macos|all)$ ]]; then
    echo "Error: Invalid TARGET '$TARGET'. Must be one of: native, depends, win64, macos, all."
    exit 1
fi

# Catch common mistake: using Linux-only targets on macOS
if [[ "$(uname -s)" == "Darwin" ]] && [[ "$TARGET" =~ ^(native|depends|win64)$ ]]; then
    echo "Error: TARGET='$TARGET' is a Linux-only target. Use TARGET=macos for native macOS builds."
    exit 1
fi

# Prepare specific CMake arguments for Native/macOS
NATIVE_CMAKE_ARGS=""

if [ -n "$CC_OVERRIDE" ]; then
    NATIVE_CMAKE_ARGS="$NATIVE_CMAKE_ARGS -DCMAKE_C_COMPILER=$CC_OVERRIDE"
fi

if [ -n "$CXX_OVERRIDE" ]; then
    NATIVE_CMAKE_ARGS="$NATIVE_CMAKE_ARGS -DCMAKE_CXX_COMPILER=$CXX_OVERRIDE"
fi

if [ "$USE_CCACHE" = "true" ]; then
    NATIVE_CMAKE_ARGS="$NATIVE_CMAKE_ARGS -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
fi

# Qt6 Logic
if [ "$USE_QT6" = "true" ]; then
    NATIVE_QT_FLAG="-DUSE_QT6=ON"
else
    NATIVE_QT_FLAG="-DUSE_QT6=OFF"
fi

# GUI Logic
if [ "$WITH_GUI" = "true" ]; then
    GUI_CMAKE_FLAG="-DENABLE_GUI=ON"
else
    GUI_CMAKE_FLAG="-DENABLE_GUI=OFF"
fi

# Docs Logic
if [ "$WITH_DOCS" = "true" ]; then
    DOCS_CMAKE_FLAG="-DENABLE_DOCS=ON"
else
    DOCS_CMAKE_FLAG="-DENABLE_DOCS=OFF"
fi

# Lock-order checking logic
if [ "$DEBUG_LOCKORDER" = "true" ]; then
    LOCKORDER_CMAKE_FLAG="-DENABLE_DEBUG_LOCKORDER=ON"
else
    LOCKORDER_CMAKE_FLAG="-DENABLE_DEBUG_LOCKORDER=OFF"
fi

# Determine Concurrency
if [ -n "$PARALLEL" ]; then
    CORES="$PARALLEL"
else
    # Cross-platform nproc detection
    if command -v nproc >/dev/null 2>&1; then
        CORES=$(nproc)
    elif command -v sysctl >/dev/null 2>&1; then
        CORES=$(sysctl -n hw.logicalcpu)
    else
        CORES=2 # Safe fallback
    fi
fi

echo "================================================================"
echo "CONFIGURATION"
echo "================================================================"
echo "Target:       $TARGET"
echo "CPU Cores:    $CORES"
echo "Build Type:   $BUILD_TYPE"
echo "Clean Build:  $CLEAN_BUILD"
echo "Skip Deps:    $SKIP_DEPS"
echo "Use Ccache:   $USE_CCACHE"
echo "With GUI:     $WITH_GUI"
echo "With Docs:    $WITH_DOCS"
echo "Lock Order:   $DEBUG_LOCKORDER"
echo "Qt6:          $USE_QT6"
if [ -n "$MANUAL_QT_PATH" ]; then echo "Manual Qt:    $MANUAL_QT_PATH"; fi
if [ -n "$EXTRA_ARGS" ]; then     echo "Extra Args:   $EXTRA_ARGS"; fi
if [ -n "$CC_OVERRIDE" ]; then    echo "C Compiler:   $CC_OVERRIDE"; fi
if [ -n "$CXX_OVERRIDE" ]; then   echo "CXX Compiler: $CXX_OVERRIDE"; fi
echo "================================================================"

# ==============================================================================
# STEP 1: INSTALL SYSTEM DEPENDENCIES
# ==============================================================================
if [ "$SKIP_DEPS" = "true" ]; then
    echo "----------------------------------------------------------------"
    echo "[Step 1] Skipping System Dependencies..."
    echo "----------------------------------------------------------------"
else
    echo "----------------------------------------------------------------"
    echo "[Step 1] Installing System Dependencies..."
    echo "----------------------------------------------------------------"

    # Check if the dependency script exists
    if [ -f "./install_dependencies.sh" ]; then
        source ./install_dependencies.sh
        # Pass TARGET, USE_QT6, and WITH_GUI to install_deps
        install_deps "$TARGET" "$USE_QT6" "$WITH_GUI"
    else
        echo "Error: install_dependencies.sh not found. Cannot install dependencies."
        exit 1
    fi
fi

# ==============================================================================
# STEP 2: BUILD LINUX NATIVE (Target #1)
# ==============================================================================
TARGET1_DAEMON="build/bin/gridcoinresearchd"
TARGET1_GUI="build/bin/gridcoinresearch"

if [[ "$TARGET" == "all" || "$TARGET" == "native" ]] && [[ "$(uname -s)" == "Linux" ]]; then
    echo "----------------------------------------------------------------"
    echo "[Step 2] Building Target 1: Linux Native..."
    echo "----------------------------------------------------------------"

    ARTIFACTS=("$TARGET1_DAEMON")
    if [ "$WITH_GUI" == "true" ]; then ARTIFACTS+=("$TARGET1_GUI"); fi

    if should_skip_build "build" "${ARTIFACTS[@]}"; then
        echo ">>> Executable(s) found and version matches. Skipping build."
    else
        # Clean previous build if requested
        if [ "$CLEAN_BUILD" == "true" ] || [ "$CLEAN_BUILD" == "main" ]; then
            echo ">>> Cleaning build directory..."
            rm -rf build
        fi

        # Configuration from build.md "1. Linux Native Build"
        cmake -B build \
            $GUI_CMAKE_FLAG \
            $DOCS_CMAKE_FLAG \
            $LOCKORDER_CMAKE_FLAG \
            -DENABLE_QRENCODE=ON \
            -DUSE_DBUS=ON \
            -DENABLE_UPNP=ON \
            -DDEFAULT_UPNP=ON \
            -DENABLE_PIE=ON \
            -DENABLE_TESTS=ON \
            $NATIVE_QT_FLAG \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            $NATIVE_CMAKE_ARGS \
            $EXTRA_ARGS

        # Build
        cmake --build build -j $CORES

        # Test
        ctest --test-dir build -j $CORES --output-on-failure

        # Write state file (Only if build and test succeeded)
        write_build_state "build"

        echo ">>> Linux Native Build Successful."
    fi
fi

# ==============================================================================
# STEP 3: BUILD LINUX STATIC DEPENDS (Target #2)
# ==============================================================================
TARGET2_DAEMON="build_linux_depends/bin/gridcoinresearchd"
TARGET2_GUI="build_linux_depends/bin/gridcoinresearch"

if [[ "$TARGET" == "all" || "$TARGET" == "depends" ]] && [[ "$(uname -s)" == "Linux" ]]; then
    echo "----------------------------------------------------------------"
    echo "[Step 3] Building Target 2: Linux Static (Depends System)..."
    echo "----------------------------------------------------------------"

    ARTIFACTS=("$TARGET2_DAEMON")
    if [ "$WITH_GUI" == "true" ]; then ARTIFACTS+=("$TARGET2_GUI"); fi

    if should_skip_build "build_linux_depends" "${ARTIFACTS[@]}"; then
        echo ">>> Executable(s) found and version matches. Skipping build."
    else
        # Clean previous build if requested
        if [ "$CLEAN_BUILD" == "true" ] || [ "$CLEAN_BUILD" == "main" ]; then
            echo ">>> Cleaning build directory..."
            rm -rf build_linux_depends
        fi

        # Build Dependencies
        cd depends
        if [ "$CLEAN_BUILD" = "true" ]; then
            echo "CLEAN_BUILD=true: Cleaning depends work/build/stamps/targets for x86_64-pc-linux-gnu..."
            rm -rf x86_64-pc-linux-gnu
            rm -rf built/x86_64-pc-linux-gnu
            rm -rf work/build/x86_64-pc-linux-gnu
            rm -rf work/staging/x86_64-pc-linux-gnu
        fi

        # Configure Ccache for Depends
        DEPENDS_ARGS=""
        if [ "$USE_CCACHE" = "true" ]; then
             DEPENDS_ARGS="CC_CACHE=ccache"
        fi

        make HOST=x86_64-pc-linux-gnu $DEPENDS_ARGS -j $CORES
        cd ..

        # This is necessary because for hermeticity reasons, depends locks down search paths
        DEPENDS_NATIVE_BIN="$(pwd)/depends/x86_64-pc-linux-gnu/native/bin"

        if [ -x "$DEPENDS_NATIVE_BIN/xxd" ]; then
            echo ">>> Forcing CMake to use Native xxd: $DEPENDS_NATIVE_BIN/xxd"
            # We append this to EXTRA_ARGS so it gets passed to the cmake configuration line below
            EXTRA_ARGS="$EXTRA_ARGS -DXXD=$DEPENDS_NATIVE_BIN/xxd"
        else
            echo ">>> WARNING: Native xxd not found at $DEPENDS_NATIVE_BIN/xxd"
        fi

        # Set DEP_LIB variable required by the recipe
        DEP_LIB=$(pwd)/depends/x86_64-pc-linux-gnu/lib
        export DEP_LIB

        # Configuration from build.md "2. Linux Static Build"
        cmake -B build_linux_depends \
            --toolchain depends/x86_64-pc-linux-gnu/toolchain.cmake \
            $GUI_CMAKE_FLAG \
            $DOCS_CMAKE_FLAG \
            $LOCKORDER_CMAKE_FLAG \
            -DUSE_QT6=ON \
            -DSTATIC_LIBS=ON \
            -DENABLE_UPNP=ON \
            -DDEFAULT_UPNP=ON \
            -DENABLE_TESTS=ON \
            -DDEP_LIB="${DEP_LIB}" \
            -DCMAKE_CXX_FLAGS="-fPIE" \
            -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -Wl,-Bdynamic" \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            $EXTRA_ARGS

        # Build
        cmake --build build_linux_depends -j $CORES

        # Test
        ctest --test-dir build_linux_depends -j $CORES --output-on-failure

        # Write state file
        write_build_state "build_linux_depends"

        echo ">>> Linux Static Build Successful."
    fi
fi

# ==============================================================================
# STEP 4: BUILD WINDOWS CROSS-COMPILE (Target #3)
# ==============================================================================
TARGET3_DAEMON="build_win64/bin/gridcoinresearchd.exe"
TARGET3_GUI="build_win64/bin/gridcoinresearch.exe"

if [[ "$TARGET" == "all" || "$TARGET" == "win64" ]] && [[ "$(uname -s)" == "Linux" ]]; then
    echo "----------------------------------------------------------------"
    echo "[Step 4] Building Target 3: Windows Cross-Compile..."
    echo "----------------------------------------------------------------"

    ARTIFACTS=("$TARGET3_DAEMON")
    if [ "$WITH_GUI" == "true" ]; then ARTIFACTS+=("$TARGET3_GUI"); fi

    if should_skip_build "build_win64" "${ARTIFACTS[@]}"; then
        echo ">>> Executable(s) found and version matches. Skipping build."
    else
        # Clean previous build if requested
        if [ "$CLEAN_BUILD" == "true" ] || [ "$CLEAN_BUILD" == "main" ]; then
            echo ">>> Cleaning build directory..."
            rm -rf build_win64
        fi

        # Build Dependencies
        cd depends
        if [ "$CLEAN_BUILD" = "true" ]; then
            echo "CLEAN_BUILD=true: Cleaning depends work/build/stamps/targets for x86_64-w64-mingw32..."
            rm -rf x86_64-w64-mingw32
            rm -rf built/x86_64-w64-mingw32
            rm -rf work/build/x86_64-w64-mingw32
            rm -rf work/staging/x86_64-w64-mingw32
        fi

        # Configure Ccache for Depends
        DEPENDS_ARGS=""
        if [ "$USE_CCACHE" = "true" ]; then
             DEPENDS_ARGS="CC_CACHE=ccache"
        fi

        make HOST=x86_64-w64-mingw32 $DEPENDS_ARGS -j $CORES
        cd ..

        # This is necessary because for hermeticity reasons, depends locks down search paths
        DEPENDS_NATIVE_BIN="$(pwd)/depends/x86_64-w64-mingw32/native/bin"

        if [ -x "$DEPENDS_NATIVE_BIN/xxd" ]; then
            echo ">>> Forcing CMake to use Native xxd: $DEPENDS_NATIVE_BIN/xxd"
            # We append this to EXTRA_ARGS so it gets passed to the cmake configuration line below
            EXTRA_ARGS="$EXTRA_ARGS -DXXD=$DEPENDS_NATIVE_BIN/xxd"
        else
            echo ">>> WARNING: Native xxd not found at $DEPENDS_NATIVE_BIN/xxd"
        fi

        # WSL Detection for Emulator Flag
        if grep -qE "(Microsoft|WSL)" /proc/version &> /dev/null; then
            echo ">>> WSL Environment detected: Using native execution for Windows binaries (No Wine)."
            CROSS_EMULATOR_FLAG=""
        else
            CROSS_EMULATOR_FLAG="-DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/wine"
        fi

        # Configuration from build.md "3. Windows Cross-Compile Build"
        cmake -B build_win64 \
            --toolchain depends/x86_64-w64-mingw32/toolchain.cmake \
            $GUI_CMAKE_FLAG \
            $DOCS_CMAKE_FLAG \
            $LOCKORDER_CMAKE_FLAG \
            -DUSE_QT6=ON \
            -DENABLE_UPNP=ON \
            -DDEFAULT_UPNP=ON \
            -DENABLE_TESTS=ON \
            -DSYSTEM_XXD=ON \
            $CROSS_EMULATOR_FLAG \
            -DCMAKE_EXE_LINKER_FLAGS="-static" \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            $EXTRA_ARGS

        # Build
        cmake --build build_win64 -j $CORES

        # Test
        ctest --test-dir build_win64 -j $CORES --output-on-failure

        # Write state file
        write_build_state "build_win64"

        echo ">>> Windows Build Successful."
    fi
fi

# ==============================================================================
# STEP 5: BUILD macOS NATIVE (Target #4)
# ==============================================================================
TARGET4_DAEMON="build_macos/bin/gridcoinresearchd"
TARGET4_GUI="build_macos/bin/gridcoinresearch.app/Contents/MacOS/gridcoinresearch"

if [[ "$TARGET" == "all" || "$TARGET" == "macos" ]] && [[ "$(uname -s)" == "Darwin" ]]; then
    echo "----------------------------------------------------------------"
    echo "[Step 5] Building Target 4: macOS Native..."
    echo "----------------------------------------------------------------"

    ARTIFACTS=("$TARGET4_DAEMON")
    if [ "$WITH_GUI" == "true" ]; then ARTIFACTS+=("$TARGET4_GUI"); fi

    if should_skip_build "build_macos" "${ARTIFACTS[@]}"; then
        echo ">>> Executable(s) found and version matches. Skipping build."
    else
        # Clean previous build if requested
        if [ "$CLEAN_BUILD" == "true" ] || [ "$CLEAN_BUILD" == "main" ]; then
            echo ">>> Cleaning build directory..."
            rm -rf build_macos
        fi

        OPENSSL_ROOT=$(brew --prefix openssl)
        # Fix for missing icudata on macOS (ICU is keg-only)
        ICU_PREFIX=$(brew --prefix icu4c)

        # QT PATH SELECTION LOGIC
        # We only need to check for Qt if we are actually building the GUI
        if [[ "$WITH_GUI" == "true" ]]; then
            if [ -n "$MANUAL_QT_PATH" ]; then
                echo "Using Manual Qt Path: $MANUAL_QT_PATH"
                QT_PREFIX_PATH="$MANUAL_QT_PATH"
            else
                # Default Homebrew Logic
                if [ "$USE_QT6" = "true" ]; then
                     QT_FORMULA="qt"
                else
                     QT_FORMULA="qt@5"
                fi

                echo "Checking for Homebrew Qt ($QT_FORMULA)..."

                if ! QT_PREFIX_PATH=$(brew --prefix "$QT_FORMULA" 2>/dev/null); then
                     echo "Error: brew --prefix $QT_FORMULA failed. Installation broken or missing."
                     echo "Check your Homebrew install or use WITH_GUI=false if you only want the daemon."
                     exit 1
                fi
            fi
            echo "Final Qt Path: $QT_PREFIX_PATH"
        else
            echo "GUI disabled: Skipping Qt detection."
            # Set to empty or don't set CMAKE_PREFIX_PATH for Qt
            QT_PREFIX_PATH=""
        fi

        echo "Detected OpenSSL Path: $OPENSSL_ROOT"
        echo "Detected ICU Path: $ICU_PREFIX"

        # Configuration from build.md / cmake_production.yml
        # Note: We pass QT_PREFIX_PATH only if it was set
        if [ -n "$QT_PREFIX_PATH" ]; then
             PREFIX_PATHS="$QT_PREFIX_PATH;$ICU_PREFIX"
        else
             PREFIX_PATHS="$ICU_PREFIX"
        fi

        cmake -B build_macos \
            -DCMAKE_PREFIX_PATH="$PREFIX_PATHS" \
            $GUI_CMAKE_FLAG \
            $DOCS_CMAKE_FLAG \
            $LOCKORDER_CMAKE_FLAG \
            -DENABLE_QRENCODE=ON \
            -DENABLE_UPNP=ON \
            -DDEFAULT_UPNP=ON \
            -DENABLE_TESTS=ON \
            $NATIVE_QT_FLAG \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT" \
            $NATIVE_CMAKE_ARGS \
            $EXTRA_ARGS

        # Build
        cmake --build build_macos -j $CORES

        # Test
        ctest --test-dir build_macos -j $CORES --output-on-failure

        # Write state file
        write_build_state "build_macos"

        echo ">>> macOS Build Successful."
    fi
fi

echo "----------------------------------------------------------------"
echo "ALL BUILDS COMPLETE"
echo "----------------------------------------------------------------"
if [[ "$(uname -s)" == "Linux" ]]; then
    if [[ "$TARGET" == "all" || "$TARGET" == "native" ]]; then echo "1. Linux Native: $TARGET1_DAEMON"; fi
    if [[ "$TARGET" == "all" || "$TARGET" == "depends" ]]; then echo "2. Linux Static: $TARGET2_DAEMON"; fi
    if [[ "$TARGET" == "all" || "$TARGET" == "win64" ]]; then echo "3. Windows:      $TARGET3_DAEMON"; fi
fi
if [[ "$(uname -s)" == "Darwin" ]]; then
    if [[ "$TARGET" == "all" || "$TARGET" == "macos" ]]; then echo "4. macOS Native: $TARGET4_DAEMON"; fi
fi
