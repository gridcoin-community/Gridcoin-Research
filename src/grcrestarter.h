#pragma once

#include <string>

namespace Restarter
{
    int RestartGridcoin();

    double PushGridcoinDiagnosticData(std::string);

    int RebootGridcoin();

    void CheckUpgrade();

    int DownloadGridcoinBlocks();

    int ReindexGridcoinWallet();

    int CreateGridcoinRestorePoint();

    void UpdateConfirm(std::string txid);

}
