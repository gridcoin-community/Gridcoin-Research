#ifndef BITCOIN_QT_ABOUTDIALOG_H
#define BITCOIN_QT_ABOUTDIALOG_H

#include <QDialog>
#include <QFutureWatcher>

#include "guiipcinfo.h"

#include <string>

namespace Ui {
    class AboutDialog;
}
class ClientModel;
namespace interfaces { class Node; }

/** "About" dialog box */
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog();

    void setModel(ClientModel *model);

    //! Populate the multiprocess connection section (GUI/node versions, IPC
    //! schema/protocol, socket, node identity) from the connect handshake. Shows
    //! it only when info.active (the -multiprocess split build); hidden otherwise.
    void setIpcConnectionInfo(const GuiIpcInfo& info);
private:
    //! Result of the off-thread GitHub version check (see
    //! handlePressVersionInfoButton). upgrade_type is carried as an int so the
    //! header needs no gridcoin/upgrade.h; the .cpp casts it back to
    //! GRC::Upgrade::UpgradeType.
    struct AboutVersionInfo
    {
        std::string version;
        std::string details;
        int upgrade_type{0};
    };

    Ui::AboutDialog *ui;
    //! Node settings/query surface; set in setModel(). Captured by value into the
    //! background checkForLatestUpdate() worker, so it is NOT process-lifetime:
    //! StartGridcoinQt owns the Node and destroys it at teardown. The destructor
    //! joins that worker, which is what keeps the captured pointer valid for as
    //! long as the worker can dereference it.
    interfaces::Node* m_node = nullptr;
    //! Watches the background version-check fetch so the blocking libcurl GET
    //! never runs on the GUI thread.
    QFutureWatcher<AboutVersionInfo> m_version_check_watcher;

private slots:
    void on_buttonBox_accepted();
    void handlePressVersionInfoButton();
    void versionCheckFinished();
};

#endif // BITCOIN_QT_ABOUTDIALOG_H
