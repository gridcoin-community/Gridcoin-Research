// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//! gridcoin-service.exe: sets up, checks and removes the Windows service that runs the Gridcoin
//! core under its own virtual account. See ToolUsage (winsvc/scm.cpp) and winsvc/setup.h.

#include "winsvc/scm.h"
#include "winsvc/setup.h"

#include "util/system.h"

#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    try {
        // On Windows: fs::path::string() in UTF-8, as every message here writes it.
        SetupEnvironment();
        // Written at once, so that output through the C library keeps its place among the lines
        // written straight to the handles (winsvc/setup.cpp), whatever stdout is.
        std::setvbuf(stdout, nullptr, _IONBF, 0);
#ifdef WIN32
        // The arguments as UTF-8, from the wide command line.
        util::WinCmdLineArgs utf8_args;
        const auto [utf8_argc, utf8_argv] = utf8_args.get();
        const std::vector<std::string> args(utf8_argv + 1, utf8_argv + utf8_argc);
#else
        const std::vector<std::string> args(argv + 1, argv + argc);
#endif

        std::string error;
        const std::optional<winsvc::ToolOptions> options = winsvc::ParseToolArgs(args, error);
        if (!options) {
            std::cerr << "gridcoin-service.exe: " << error << "\n\n" << winsvc::ToolUsage();
            return winsvc::EXIT_USAGE;
        }

        switch (options->verb) {
        case winsvc::Verb::Help:
            std::cout << winsvc::ToolUsage();
            return winsvc::EXIT_OK;
#ifdef WIN32
        case winsvc::Verb::Install:
            return winsvc::Install(*options);
        case winsvc::Verb::Uninstall:
            return winsvc::Uninstall(*options);
        case winsvc::Verb::Status:
            return winsvc::Status(*options);
#else
        default:
            std::cerr << "gridcoin-service.exe: this program sets up a Windows service and runs on Windows only\n";
            return winsvc::EXIT_REFUSED;
#endif
        }
    } catch (const std::exception& e) {
        std::cerr << "gridcoin-service.exe: " << e.what() << "\n";
    }
    return winsvc::EXIT_REFUSED;
}
