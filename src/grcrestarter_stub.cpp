#include <string>
#include "grcrestarter.h"

namespace Restarter
{
    double PushGridcoinDiagnosticData(std::string)
    {
        return 0;
    }

    int RestartGridcoin()
    {
        return 0;
    }

    bool IsUpgradeAvailable()
    {
        return false;
    }

    void UpgradeClient()
    {}

    int DownloadGridcoinBlocks()
    {
        return -1;
    }

    int ReindexGridcoinWallet()
    {
        return 0;
    }

    int CreateGridcoinRestorePoint()
    {
        return -1;
    }
}
