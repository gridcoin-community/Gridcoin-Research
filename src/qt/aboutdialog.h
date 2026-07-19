#ifndef BITCOIN_QT_ABOUTDIALOG_H
#define BITCOIN_QT_ABOUTDIALOG_H

#include <QDialog>
#include <QFutureWatcher>

#include <string>

namespace Ui {
    class AboutDialog;
}
class ClientModel;

//! Result of the off-thread GitHub version check (see
//! AboutDialog::handlePressVersionInfoButton). upgrade_type is carried as an
//! int so the header needs no gridcoin/upgrade.h; the .cpp casts it back to
//! GRC::Upgrade::UpgradeType.
struct AboutVersionInfo
{
    std::string version;
    std::string details;
    int upgrade_type{0};
};

/** "About" dialog box */
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog();

    void setModel(ClientModel *model);
private:
    Ui::AboutDialog *ui;
    //! Watches the background version-check fetch so the blocking libcurl GET
    //! never runs on the GUI thread.
    QFutureWatcher<AboutVersionInfo> m_version_check_watcher;

private slots:
    void on_buttonBox_accepted();
    void handlePressVersionInfoButton();
    void versionCheckFinished();
};

#endif // BITCOIN_QT_ABOUTDIALOG_H
