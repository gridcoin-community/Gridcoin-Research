#include <string>
#include "grcrestarter.h"

// Old VB based NeuralNet.
double qtPushGridcoinDiagnosticData(std::string data);
void qtUpdateConfirm(std::string txid);
int RestartClient();
int RebootClient();
void CheckForUpgrade();
int DownloadBlocks();
// While transitioning to dotnet the NeuralNet implementation has been split
// into 3 implementations; Win32 with Qt, Win32 without Qt and the rest.
// After the transition both Win32 implementations can be removed.
namespace Restarter
{
    // Win32 with Qt enabled.
    double PushGridcoinDiagnosticData(std::string)
    {
        return qtPushGridcoinDiagnosticData(std::string)
    }

    int RestartGridcoin()
    {
        return RestartClient();
    }

    int RebootGridcoin()
    {
        return int RebootClient();
    }

    void CheckUpgrade()
    {
        return void CheckForUpgrade();
    }

    int DownloadGridcoinBlocks()
    {
        return int DownloadBlocks();
    }

    int ReindexGridcoinWallet()
    {
        return int ReindexWallet();
    }

    int CreateGRidcoinRestorePoint()
    {
        return int CreateRestorePoint();
    }

    void UpdateConfirm(std::string txid)
    {
        qtUpdateConfirm(std::string txid);
    }

}
