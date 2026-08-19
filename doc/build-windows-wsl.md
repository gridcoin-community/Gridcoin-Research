# Building Gridcoin for Windows using WSL

This guide describes how to build the Gridcoin Windows client (64-bit) using the Windows Subsystem for Linux (WSL). This method replaces the deprecated MSYS2 build process.

It utilizes the `build_targets.sh` script to automate dependency installation (MinGW-w64, Qt6, Boost) and the cross-compilation process.

## Prerequisites

1.  **Windows 10 or Windows 11**
2.  **WSL Installed:** If you have not installed WSL yet, open PowerShell as Administrator and run:
    ```powershell
    wsl --install
    ```
    *This will install Ubuntu by default, which is recommended for this build process.*
3.  **Restart your computer** if prompted.

## 1. Get the Source Code

Open your Linux terminal (e.g., "Ubuntu" from the Start Menu) and run the following commands to clone the repository:

> **Clone inside the WSL filesystem** — use a path under your Linux home such as `~/Gridcoin-Research`,
> not a Windows drive under `/mnt/c` and not a OneDrive-synced folder. A checkout made with Git for
> Windows arrives with CRLF line endings: this repository's `.gitattributes` marks every path
> `text=auto`, and for such a path the working-tree ending follows `core.eol`, whose default `native`
> means CRLF on Windows — regardless of whether `core.autocrlf` is `true` or `false`, and there is no
> per-path `eol=lf` override. CRLF breaks the repository's own POSIX shell scripts: `build_targets.sh`
> fails immediately with `/usr/bin/env: 'bash\r': No such file or directory`, and the bundled BerkeleyDB
> script `src/bdb53/dist/configure` fails with `Syntax error: newline unexpected` (the exact wording
> varies by shell). Cloning from inside WSL avoids the conversion, but a build over `/mnt` is still
> considerably slower than on ext4.

```bash
# Update package lists and install git (if not already present)
sudo apt update && sudo apt install -y git

# Clone the Gridcoin repository
git clone [https://github.com/gridcoin-community/Gridcoin-Research.git](https://github.com/gridcoin-community/Gridcoin-Research.git)

# Enter the directory
cd Gridcoin-Research

# Checkout the master branch (or a specific tag/branch)
git checkout master
````


## 1a. Ubuntu 22.04 Compatibility (_Important_)

If you are running **Ubuntu 22.04 (Jammy Jellyfish)**, you must manually install a newer MinGW toolchain. The default package provided by Ubuntu 22.04 is too old (GCC 10 / MinGW Headers 8.0.0) and lacks the Direct3D 12 definitions required by Qt6.

**First, remove any existing system MinGW packages to prevent conflicts:**
```bash
sudo apt-get remove g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64
```

**Do not attempt to use `apt install` for MinGW on Ubuntu 22.04.**

Instead, use the following commands to install the **xPack** standalone toolchain (GCC 13.2.0 + MinGW 11 + POSIX Threads):

```bash

# 1. Create a directory for the toolchain and change to that directory
mkdir -p ~/toolchains && cd ~/toolchains

# 2. Download the xPack standalone toolchain (Linux Host -> Windows Target)
wget [https://github.com/xpack-dev-tools/mingw-w64-gcc-xpack/releases/download/v13.2.0-1/xpack-mingw-w64-gcc-13.2.0-1-linux-x64.tar.gz](https://github.com/xpack-dev-tools/mingw-w64-gcc-xpack/releases/download/v13.2.0-1/xpack-mingw-w64-gcc-13.2.0-1-linux-x64.tar.gz)

# 3. Extract the archive
tar -xf xpack-mingw-w64-gcc-13.2.0-1-linux-x64.tar.gz

# 4. Rename for simplicity
mv xpack-mingw-w64-gcc-13.2.0-1 mingw64-posix

# 5. Add the toolchain to your PATH (Run this every time you open a terminal, or add to ~/.bashrc)
export PATH="$HOME/toolchains/mingw64-posix/bin:$PATH"

# 6. Verify the installation
# The version should be 13.2.0 and the Thread model must be 'posix'
x86_64-w64-mingw32-g++ -v

# 7. Change back to your Gridcoin repo directory
cd ~/Gridcoin-Research
```

Once this is set up, proceed to **Step 2**. The `build_targets.sh` script will automatically detect this custom toolchain and skip the system dependency installation for the compiler.

## 2\. Build the Windows Executable

We use the `build_targets.sh` helper script. This script handles:

1.  Detecting your WSL environment.
2.  Installing necessary build tools (MinGW, CMake, etc.).
3.  Building the "Depends" system (static libraries for Qt6, Boost, OpenSSL, etc.).
4.  Compiling Gridcoin.

Run the following command:

```bash
./build_targets.sh TARGET=win64 BUILD_TYPE=RelWithDebInfo USE_CCACHE=true
```

### Build Options

  * **`TARGET=win64`**: (Required) Tells the script to cross-compile for Windows.
  * **`BUILD_TYPE`**:
      * `RelWithDebInfo` (Recommended): Optimized for performance but includes debug symbols.
      * `Release`: Fully optimized, smaller binary, no debug info.
      * `Debug`: Unoptimized, very large binaries. **Do not use if building the installer.**
  * **`USE_CCACHE=true`**: (Recommended) Speeds up subsequent builds significantly.
  * **`CLEAN_BUILD=true`**: (Optional) Forces a complete wipe and rebuild of the Gridcoin source (does not wipe the dependencies).
  * **`PARALLEL=<int>`**: (Optional) Limit the number of CPU cores used.

### Performance Notes

#### Nested Virtualization

If you are running WSL inside a Virtual Machine (e.g., VMware Workstation, VirtualBox), you may experience system instability or "freezes" during the linking phase due to nested virtualization I/O overhead.

**Solution:** Reduce the CPU count of the VM to 2, or limit the build parallelism using `PARALLEL=2`.

#### Initial Build

The first time you run this, it will take a significant amount of time to compile the dependencies (Qt, Boost, etc.). Subsequent builds will be much faster.

## 3\. Verify the Build

Once the script completes successfully, your Windows executable is located here:

```bash
./build_win64/bin/gridcoinresearchd.exe
```

Because you are on WSL, you can run this directly from the Linux terminal to verify the version:

```bash
./build_win64/bin/gridcoinresearchd.exe --version
```

## 4\. Create the Installer

If you wish to create the installable package, use CPack (part of CMake) after the build completes.

**⚠️ WARNING: Debug Builds**
Do not attempt to build the installer if you used `BUILD_TYPE=Debug`. The resulting executables may be too large for the NSIS compressor to handle, and the process will fail. Use `RelWithDebInfo` or `Release`.

```bash
cpack -G NSIS64 --config build_win64/CPackConfig.cmake -B build_win64
```

The resulting installer will be generated in the `build_win64` directory with the format:
`gridcoin-<release>-win64-setup.exe` *(e.g., `gridcoin-x.y.z.w-win64-setup.exe`)*

## 5\. Accessing Files from Windows

To copy the `.exe` or installer to your standard Windows file system, you can use the `explorer.exe` command from within the Linux terminal:

```bash
explorer.exe build_win64
```

This will open a Windows File Explorer window directly mapped to your build directory.
