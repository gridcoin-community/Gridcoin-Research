#ifndef DIAGNOSTICSDIALOG_H
#define DIAGNOSTICSDIALOG_H

#include <QDialog>
#include <QtNetwork>
#include <QtConcurrent>

#include "main.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

extern std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end);
extern std::string GetListOf(std::string datatype);
extern int64_t GetAdjustedTime();
extern std::string GetNeuralNetworkSupermajorityHash(double& out_popularity);
template<typename T>
extern std::string ToString(const T& val);
extern std::string TimestampToHRDate(double dtm);
extern double PreviousBlockAge();
extern double GetDifficulty(const CBlockIndex* blockindex);
extern const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake);
//extern std::string CBlockIndex::GetCPID();
extern std::string RoundToString(double d, int place);
extern std::string GetBoincDataDir();

extern std::map<std::string, int64_t> mvApplicationCacheTimestamp;

namespace Ui {
class DiagnosticsDialog;
}

class DiagnosticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiagnosticsDialog(QWidget *parent = 0);
    ~DiagnosticsDialog();

private:
    Ui::DiagnosticsDialog *ui;
    void GetData();
    int VarifyBoincPath();
    int VerifyCPIDIsInNeuralNetwork();
    int VarifyWalletIsSynced();
    int VarifyCPIDHasRAC();
    int VarifyAddNode();
    int VarifyIsCPIDValid();
    int FindCPID();
    static int VarifyClock(DiagnosticsDialog *dialog);
    static int VarifyTCPPort(DiagnosticsDialog *dialog);
    double GetTotalCPIDRAC(std::string cpid);
    double GetUserRAC(std::string cpid, int *projects);
    std::string KeyValue(std::string key);

    QFutureWatcher<int> watcherClk;
    QFutureWatcher<int> watcherTCP;
    std::string testnet_flag;
    std::string syncData;

private slots:
    void on_testBtn_clicked();
    void clkFinished();
    void TCPFinished();
};

#endif // DIAGNOSTICSDIALOG_H
