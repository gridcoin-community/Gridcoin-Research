#include <string>
#include "grcrestarter.h"

// Old VB based NeuralNet.
double qtPushGridcoinDiagnosticData(std::string data);
void qtUpdateConfirm(std::string txid);
int RestartClient();
int RebootClient();
void CheckForUpgrade();
int DownloadBlocks();
int ReindexWallet();
int CreateRestorePoint();
// While transitioning to dotnet the NeuralNet implementation has been split
// into 3 implementations; Win32 with Qt, Win32 without Qt and the rest.
// After the transition both Win32 implementations can be removed.
namespace Restarter
{
    // Win32 with Qt enabled.
    double PushGridcoinDiagnosticData(std::string data)
    {
        return qtPushGridcoinDiagnosticData(data);
    }

    int RestartGridcoin()
    {
        return RestartClient();
    }

    int RebootGridcoin()
    {
        return RebootClient();
    }

    void CheckUpgrade()
    {
        return CheckForUpgrade();
    }

    int DownloadGridcoinBlocks()
    {
        return DownloadBlocks();
    }

    int ReindexGridcoinWallet()
    {
        return ReindexWallet();
    }

    int CreateGridcoinRestorePoint()
    {
        return CreateRestorePoint();
    }

    void UpdateConfirm(std::string txid)
    {
        qtUpdateConfirm(txid);
    }

}
