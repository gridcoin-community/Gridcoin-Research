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

    int RebootGridcoin()
    {
        return 1;
    }

    void CheckUpgrade()
    {
        return;
    }

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

    void UpdateConfirm(std::string txid)
    {
        return;
    }

}
