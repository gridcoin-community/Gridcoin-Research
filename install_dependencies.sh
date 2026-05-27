#!/usr/bin/env bash

export LC_ALL=C

# ==============================================================================
# HELPER: DETECT WSL
# ==============================================================================
check_is_wsl() {
    if grep -qE "(Microsoft|WSL)" /proc/version &> /dev/null; then
        return 0 # True (Is WSL)
    else
        return 1 # False (Is Native Linux)
    fi
}

install_deps() {
    local TARGET="$1"
    local USE_QT6="$2"
    local WITH_GUI="$3"

    # Detect OS Type first
    OS_TYPE=$(uname -s)

    if [[ "$OS_TYPE" == "Darwin" ]]; then
        OS="macos"
        echo "Detected OS: macOS (Darwin)"
    elif [ -f /etc/os-release ]; then
        # The next line prevents the linter from looking into os-release and complaining about unused variables.
        # shellcheck source=/dev/null
        . /etc/os-release
        OS=$ID
    else
        echo "Error: Cannot detect OS distribution."
        return 1
    fi

    # Detect WSL Status (Only relevant for Linux)
    IS_WSL="false"
    if [[ "$OS" != "macos" ]]; then
        if check_is_wsl; then
            IS_WSL="true"
            echo "Detected Environment: WSL (Windows Subsystem for Linux)"
        else
            echo "Detected Environment: Native Linux"
        fi
    fi

    echo "Installing dependencies for Target: $TARGET, Qt6: $USE_QT6, GUI: $WITH_GUI"

    # --- Package Groups Definition ---
    PKGS_BASE=""
    PKGS_QT=""
    PKGS_MINGW=""
    PKGS_WINE=""

    append_base() { PKGS_BASE="$PKGS_BASE $*"; }
    append_qt() { PKGS_QT="$PKGS_QT $*"; }
    append_mingw() { PKGS_MINGW="$PKGS_MINGW $*"; }

    # Conditional Wine Append
    # We only install Wine if the target is win64 AND we are NOT on WSL.
    # On WSL, we can run Windows binaries natively.
    append_wine() {
        if [[ "$IS_WSL" == "false" ]]; then
            PKGS_WINE="$PKGS_WINE $*"
        fi
    }

    # --- Define Packages per OS ---
    case $OS in
        macos)
            # Homebrew Logic
            if ! command -v brew &> /dev/null; then
                echo "Error: Homebrew not found. Please install it from https://brew.sh"
                return 1
            fi

            # Base Tools
            append_base cmake ccache libtool automake autoconf pkg-config
            # Libraries
            append_base boost openssl libevent miniupnpc qrencode libzip

            # Qt Logic for macOS Homebrew (Only if GUI is requested)
            if [[ "$WITH_GUI" == "true" ]]; then
                if [[ "$USE_QT6" == "true" ]]; then
                    append_qt qt
                else
                    # Install Qt5 specific formula for legacy support
                    append_qt qt@5
                fi
            fi
            ;;

        debian|ubuntu|linuxmint)
            # Refresh the apt package index BEFORE we probe for renamed
            # Qt6 packages below. On a fresh debian:sid / ubuntu:noble
            # container the baseline image may ship with stale or empty
            # /var/lib/apt/lists; without an update here the apt-cache
            # probes would miss the new package names and fall back to
            # the legacy names that no longer exist, reintroducing the
            # original Sid CI failure. The subsequent `apt-get install`
            # at the bottom of this case relies on the same fresh index.
            sudo apt-get update

            # Base Build Tools
            append_base build-essential libtool autotools-dev automake pkg-config bsdmainutils python3 cmake git curl ccache doxygen graphviz bison xxd libxkbcommon-dev

            # Libraries for Native Build
            append_base libssl-dev libevent-dev libboost-all-dev libminiupnpc-dev libqrencode-dev libzip-dev libcurl4-openssl-dev zipcmp zipmerge ziptool

            # Qt6 Packages (Qt5 names are not defined here as most are EOL)
            append_qt qt6-base-dev qt6-tools-dev qt6-l10n-tools qt6-tools-dev-tools

            # Several Qt6 module -dev packages were renamed across the
            # Debian/Ubuntu family between 2024 and 2026, moving from the
            # legacy `libqt6<module>6-dev` naming to the upstream-aligned
            # `qt6-<module>-dev` naming. The transitional metapackages have
            # been dropped at different points per distro:
            #
            #   - Debian Sid / forky / trixie:    new names only
            #   - Debian Bookworm (12):           new names only
            #   - Ubuntu Noble (24.04):           new names only
            #   - Ubuntu Jammy (22.04):           old names only
            #
            # `prefer_new_qt_package` picks the new name when apt-cache
            # knows about it, otherwise falls back to the old name. The
            # apt index was refreshed just above so the probe sees the
            # current package set on any environment (CI image, bare-
            # metal dev, fresh container). `-q -q` keeps the detection
            # quiet on the success path. Future renames of other modules
            # in this family only need another one-line call to this
            # helper.
            prefer_new_qt_package() {
                local new=$1 old=$2
                if apt-cache -q -q show "$new" >/dev/null 2>&1; then
                    append_qt "$new"
                else
                    append_qt "$old"
                fi
            }
            prefer_new_qt_package qt6-charts-dev   libqt6charts6-dev
            prefer_new_qt_package qt6-svg-dev      libqt6svg6-dev
            prefer_new_qt_package qt6-5compat-dev  libqt6core5compat6-dev

            # Windows Cross-Compile Tools
            # NOTE: We only append NSIS here. The MinGW compiler (g++-mingw-w64-x86-64)
            # is handled conditionally below to support manual installs on Ubuntu 22.04.
            append_mingw nsis

            # Wine (Emulator for Native Linux only)
            append_wine wine wine64

            # Smart Check for MinGW Headers on Ubuntu 22.04
            if [[ "$TARGET" == "win64" || "$TARGET" == "all" ]]; then
                SHOULD_INSTALL_MINGW="true"

                if grep -q "22.04" /etc/os-release; then
                    # 1. Check if binary exists
                    MINGW_BIN="x86_64-w64-mingw32-g++"
                    if command -v "$MINGW_BIN" &> /dev/null; then

                        # 2. Check HEADER version directly using the preprocessor
                        # We look for __MINGW64_VERSION_MAJOR in _mingw.h
                        MINGW_HEADER_VER=$(echo "#include <_mingw.h>" | "$MINGW_BIN" -E -dM - 2>/dev/null | grep "^#define __MINGW64_VERSION_MAJOR " | awk '{print $3}')

                        # Default to 0 if grep failed
                        if [ -z "$MINGW_HEADER_VER" ]; then MINGW_HEADER_VER=0; fi

                        # 3. Logic Gate: We need headers >= 9 for D3D12/Qt6 support
                        if [[ "$MINGW_HEADER_VER" -ge 9 ]]; then
                            echo "Detected manual MinGW install (Header Version $MINGW_HEADER_VER). Skipping repo package."
                            SHOULD_INSTALL_MINGW="false"
                        else
                             echo "Error: Detected MinGW header version $MINGW_HEADER_VER is too old (Need >= 9)."
                             # We must force 'false' here so we fall through to the fatal error block below
                             # We cannot simply install the repo package to fix this, as the repo package IS the problem.
                             SHOULD_INSTALL_MINGW="false"
                             FORCE_FAIL_2204="true"
                        fi
                    else
                        # Binary missing entirely
                        echo "Error: MinGW compiler not found."
                        SHOULD_INSTALL_MINGW="false" # Do not try to install repo package
                        FORCE_FAIL_2204="true"
                    fi

                    # Fatal Error Block for 22.04
                    if [[ "$FORCE_FAIL_2204" == "true" ]]; then
                        echo "----------------------------------------------------------------"
                        echo "CRITICAL: Windows Cross-Compilation on Ubuntu 22.04 requires manual setup."
                        echo "          The standard repository packages are too old (Headers < 9.0)."
                        echo ""
                        echo "          You must manually install a newer MinGW toolchain (v9+)"
                        echo "          OR upgrade to Ubuntu 24.04+."
                        echo "----------------------------------------------------------------"
                        exit 1
                    fi
                fi

                # If logic allows, append the compiler package (Standard path for non-22.04)
                if [[ "$SHOULD_INSTALL_MINGW" == "true" ]]; then
                    append_mingw g++-mingw-w64-x86-64
                fi
            fi
            ;;

        fedora|rhel)
            append_base libstdc++-static gcc-c++ libtool automake autoconf pkgconf-pkg-config python3 cmake git curl patch perl-FindBin bison flex ccache doxygen graphviz
            append_base openssl-devel libevent-devel boost-devel miniupnpc-devel qrencode-devel libzip-devel libcurl-devel libzip-tools

            append_qt qt6-qtbase-devel qt6-qttools-devel qt6-qtcharts-devel qt6-qtsvg-devel qt6-qt5compat-devel

            append_mingw mingw64-gcc-c++ mingw64-nsis xxd

            # Fedora Wine
            append_wine wine
            ;;

        opensuse*|sles)
            # Detect Tumbleweed vs Leap
            if [[ "$ID" == "sles" ]]; then
                echo "Error: SLES not supported automatically."
                return 1
            fi

            IS_TUMBLEWEED="false"
            if [[ "$PRETTY_NAME" == *"Tumbleweed"* ]]; then
                DISTRO_PATH="openSUSE_Tumbleweed"
                IS_TUMBLEWEED="true"
            elif [[ "$PRETTY_NAME" == *"Leap"* ]]; then
                # The openSUSE Build Service distribution directory for Leap is
                # openSUSE_Leap_<version> (e.g. openSUSE_Leap_16.0). VERSION_ID
                # is sourced from /etc/os-release during OS detection above.
                # Hardcoding a fixed version here produced dead repo URLs on
                # every Leap release other than the hardcoded one.
                DISTRO_PATH="openSUSE_Leap_${VERSION_ID}"
            else
                 echo "Error: Unknown openSUSE version."
                 return 1
            fi

            # Repo Logic for openSUSE
            if [[ "$TARGET" == "all" || "$TARGET" == "win64" ]]; then
                REPO_64_URL="https://download.opensuse.org/repositories/windows:/mingw:/win64/$DISTRO_PATH/"
                REPO_64_NAME="windows_mingw_win64"
                REPO_32_URL="https://download.opensuse.org/repositories/windows:/mingw:/win32/$DISTRO_PATH/"
                REPO_32_NAME="windows_mingw_win32"

                add_repo_if_missing() {
                    local url="$1"
                    local name="$2"
                    local desc="$3"
                    # Detection is done on the OBS project base path
                    # (.../windows:/mingw:/winNN/) rather than the full URL or
                    # our own alias, because the openSUSE-shipped Cross-toolchain
                    # repos provide exactly these MinGW packages but carry a
                    # human-readable alias and a URL whose trailing distro
                    # segment may differ from ours (e.g. no trailing slash).
                    #
                    # A repo only actually satisfies the dependency if its URL
                    # carries BOTH the base path AND the current distro segment
                    # ($DISTRO_PATH). A repo with the base path but a different
                    # distro segment — a stale dead-URL duplicate from the
                    # pre-fix script, or a Leap/Tumbleweed mismatch — does NOT
                    # satisfy it and must not be silently treated as if it did.
                    local base="${url%"$DISTRO_PATH"/}"
                    if sudo zypper lr -u | grep -F "$base" | grep -Fq "$DISTRO_PATH"; then
                        echo "Repository for $desc already provided by an existing repo - skipping."
                    elif sudo zypper lr -u | grep -Fq "$base"; then
                        # Base path present but wrong distro segment: a stale or
                        # mismatched MinGW repo is in the way. It will not
                        # provide $desc packages for this system and will fail
                        # to refresh. Warn with cleanup instructions rather than
                        # adding a second repo for the same OBS project.
                        echo "Warning: a MinGW cross-toolchain repo for a different distribution"
                        echo "         is present under ${base} (expected distro segment"
                        echo "         '$DISTRO_PATH'). It will not provide $desc packages for"
                        echo "         this system and will fail on refresh. Remove the stale"
                        echo "         repo — 'sudo zypper lr' to find its alias, then"
                        echo "         'sudo zypper rr <alias>' — and re-run."
                    elif sudo zypper lr | grep -Fq "$name"; then
                        echo "Warning: Repository alias '$name' exists but URL mismatch - leaving as-is."
                    else
                        echo "Adding $desc repository: $url"
                        sudo zypper ar -f "$url" "$name"
                    fi
                }
                add_repo_if_missing "$REPO_64_URL" "$REPO_64_NAME" "MinGW Win64"
                add_repo_if_missing "$REPO_32_URL" "$REPO_32_NAME" "MinGW Win32"
                sudo zypper --gpg-auto-import-keys refresh
            fi

            # Pattern Install
            #
            # On Tumbleweed (rolling release), the base image as of
            # snapshot ~20260523 ships busybox-gawk (and busybox-less),
            # both of which `provide` the generic `gawk` / `less` capability.
            # `patterns-devel-base-devel_basis` requires the canonical GNU
            # gawk; zypper refuses the install with "not installable
            # providers" because busybox-gawk already owns the `gawk`
            # provider slot. Remove the busybox shims first so the devel
            # pattern can pull in the real GNU tools.
            #
            # Safe for non-CI / bare-metal developer use too: on a system
            # that's about to install patterns-devel-base-devel_basis, the
            # canonical GNU gawk/less are what's actually wanted -- the
            # busybox-* variants are minimal alternates the pattern will
            # replace anyway.
            #
            # Leap and other openSUSE flavours don't hit this in their
            # current snapshots, so the workaround is gated on Tumbleweed.
            # The `rpm -q --quiet` guard skips the rm entirely on systems
            # where neither shim is installed (e.g. an already-provisioned
            # dev box) -- no zypper noise and no exit-code dance. When the
            # rm does run, stderr stays visible so a real removal failure
            # (repo lock, network, solver problem) surfaces in the CI /
            # shell log rather than masquerading as the downstream
            # devel_basis install failure.
            if [[ "$IS_TUMBLEWEED" == "true" ]] \
                && { rpm -q --quiet busybox-gawk || rpm -q --quiet busybox-less; }; then
                echo "Removing busybox shims that conflict with devel_basis (busybox-gawk, busybox-less)..."
                sudo zypper rm -y busybox-gawk busybox-less
            fi

            echo "Installing devel_basis pattern..."
            sudo zypper install -y -t pattern devel_basis

            # Base common packages
            append_base libtool automake autoconf pkg-config python3 cmake git curl ccache doxygen graphviz libzstd-devel
            append_base libopenssl-devel libevent-devel qrencode-devel libzip-devel libcurl-devel libzip-tools
            append_base miniupnpc libminiupnpc-devel

            # Boost Packages
            append_base libboost_headers-devel libboost_filesystem-devel libboost_thread-devel libboost_date_time-devel libboost_iostreams-devel libboost_serialization-devel libboost_test-devel libboost_atomic-devel libboost_regex-devel

            # Boost System Logic:
            # Use 'zypper info' to check which version of headers is (or will be) installed.
            # This handles both Clean Install and Upgrade scenarios correctly.

            INSTALL_BOOST_SYSTEM="true"

            # Get the version string from the repository metadata
            # Output format example: "Version      : 1.89.0-..."
            BOOST_VER_STRING=$(zypper info libboost_headers-devel | grep -i "Version" | head -n 1 | awk '{print $3}')

            if [ -n "$BOOST_VER_STRING" ]; then
                # Extract Major.Minor
                IFS='.' read -r -a VER_PARTS <<< "$BOOST_VER_STRING"
                MAJOR=${VER_PARTS[0]}
                MINOR=${VER_PARTS[1]}

                # Check if >= 1.69 (Boost System became header-only)
                if [[ "$MAJOR" -ge 1 ]] && [[ "$MINOR" -ge 69 ]]; then
                     INSTALL_BOOST_SYSTEM="false"
                     echo "Detected Boost >= 1.69 ($BOOST_VER_STRING) in repo. Skipping libboost_system-devel."
                fi
            fi

            if [[ "$IS_TUMBLEWEED" == "false" ]]; then
                append_base gcc13 gcc13-c++
            fi

            if [[ "$INSTALL_BOOST_SYSTEM" == "true" ]]; then
                append_base libboost_system-devel
            fi

            append_qt qt6-base-devel qt6-tools-devel qt6-charts-devel qt6-svg-devel qt6-qt5compat-devel qt6-linguist-devel

            append_mingw mingw64-cross-gcc-c++ nsis

            # OpenSUSE Wine
            append_wine wine
            ;;

        arch|manjaro)
            sudo pacman -Syu --noconfirm

            append_base base-devel python cmake git ccache doxygen graphviz
            append_base boost boost-libs libevent miniupnpc libzip qrencode curl icu

            append_qt qt6-base qt6-tools qt6-charts qt6-svg qt6-5compat

            append_mingw mingw-w64-gcc nsis

            # Arch Wine
            append_wine wine
            ;;

        alpine)
            # Base Build Tools
            # 'build-base' is Alpine's build-essential.
            # 'linux-headers' often needed. 'bash' is needed for these scripts.
            # 'libexecinfo-dev' is sometimes needed for backtraces on musl.
            append_base build-base cmake git curl ccache doxygen graphviz bison linux-headers xxd bash autoconf automake libtool perl

            # Libraries
            append_base boost-dev openssl-dev libevent-dev miniupnpc-dev libqrencode-dev libzip-dev curl-dev

            # Qt6 Packages
            append_qt qt6-qtbase-dev qt6-qttools-dev qt6-qtcharts-dev qt6-qtsvg-dev qt6-qt5compat-dev
            ;;

        *)
            echo "Error: Unsupported distribution '$OS'."
            return 1
            ;;
    esac

    # --- Determine Final Package List to Install ---
    PKGS_TO_INSTALL=""

    # FIX: Base packages are always required.
    PKGS_TO_INSTALL="$PKGS_TO_INSTALL $PKGS_BASE"

    # --- Qt Package Logic ---
    if [[ "$WITH_GUI" == "true" ]]; then
        if [[ "$OS" == "macos" ]]; then
            # macOS: PKGS_QT was already populated conditionally inside the case statement
            PKGS_TO_INSTALL="$PKGS_TO_INSTALL $PKGS_QT"
        else
            # Linux: PKGS_QT is populated unconditionally in the distro blocks with Qt6 packages.
            # We must handle the logic here to ensure we don't install Qt6 when Qt5 is requested.
            if [[ "$USE_QT6" == "true" ]]; then
                if [[ "$TARGET" == "all" || "$TARGET" == "native" ]]; then
                    PKGS_TO_INSTALL="$PKGS_TO_INSTALL $PKGS_QT"
                fi
            else
                # Case: Linux + Qt5.
                # We do not list Qt5 packages for Linux as distro support is spotty/EOL.
                echo "----------------------------------------------------------------"
                echo "WARNING: Qt5 requested on Linux (WITH_GUI=true, USE_QT6=false)."
                echo "         This script only defines Qt6 packages for Linux."
                echo "         You must install the appropriate Qt5 packages manually."
                echo "----------------------------------------------------------------"
            fi
        fi
    fi

    if [[ "$TARGET" == "all" || "$TARGET" == "win64" ]]; then
        PKGS_TO_INSTALL="$PKGS_TO_INSTALL $PKGS_MINGW"
        # Only adds wine if !WSL
        PKGS_TO_INSTALL="$PKGS_TO_INSTALL $PKGS_WINE"
    fi

    # Clean up leading whitespace
    PKGS_TO_INSTALL=$(echo "$PKGS_TO_INSTALL" | xargs)

    if [ -z "$PKGS_TO_INSTALL" ]; then
        echo "No packages selected for installation."
        return 0
    fi

    # --- Execute Install Command ---
    echo "Installing Packages: $PKGS_TO_INSTALL"

    case $OS in
        macos)
            # macOS uses Homebrew, usually doesn't need sudo if user owns prefix
            brew install $PKGS_TO_INSTALL
            ;;
        debian|ubuntu|linuxmint)
            # apt-get update was run earlier in the debian/ubuntu/linuxmint
            # branch above (before the Qt6 rename-detection probes), so the
            # index is already fresh here.
            sudo apt-get install -y --no-install-recommends $PKGS_TO_INSTALL

            # MinGW Threading Fix (Linux Only)
            if [[ "$TARGET" == "all" || "$TARGET" == "win64" ]]; then
                echo "Configuring MinGW-w64 threading model to POSIX..."
                if [ -f /usr/bin/x86_64-w64-mingw32-g++-posix ]; then
                    sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
                    sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
                    echo "MinGW-w64 threading model set to POSIX."
                else
                    echo "Warning: MinGW POSIX alternative not found. Skipping threading configuration."
                fi
            fi
            ;;
        fedora|rhel)
            sudo dnf install -y $PKGS_TO_INSTALL
            ;;
        opensuse*|sles)
            sudo zypper install -y $PKGS_TO_INSTALL
            ;;
        arch|manjaro)
            sudo pacman -S --noconfirm $PKGS_TO_INSTALL
            ;;
        alpine)
            sudo apk update
            sudo apk add $PKGS_TO_INSTALL
            ;;
    esac
}
