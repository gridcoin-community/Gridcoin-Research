#pragma once

#include <string>

namespace Restarter
{
    int RestartGridcoin();

    double PushGridcoinDiagnosticData(std::string);

    void CheckUpgrade();

    int DownloadGridcoinBlocks();

    int ReindexGridcoinWallet();

    int CreateGridcoinRestorePoint();
}
