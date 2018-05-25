#pragma once

#include <string>

namespace Restarter
{
    int RestartGridcoin();

    double PushGridcoinDiagnosticData(std::string);

    bool IsUpgradeAvailable();

    void UpgradeClient();

    int DownloadGridcoinBlocks();

    int ReindexGridcoinWallet();

    int CreateGridcoinRestorePoint();
}
