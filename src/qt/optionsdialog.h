#ifndef BITCOIN_QT_OPTIONSDIALOG_H
#define BITCOIN_QT_OPTIONSDIALOG_H

#include <QDialog>

namespace Ui {
class OptionsDialog;
}
class OptionsModel;
class MonitoredDataMapper;
class QValidatedLineEdit;

/** Preferences dialog. */
class OptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget* parent = nullptr);
    ~OptionsDialog();

    void setModel(OptionsModel *model);
    void setMapper();

protected:
    bool eventFilter(QObject *object, QEvent *event);

private slots:
    /* enable only apply button */
    void enableApplyButton();
    /* disable only apply button */
    void disableApplyButton();
    /* enable apply button and OK button */
    void enableSaveButtons();
    /* disable apply button and OK button */
    void disableSaveButtons();
    /* set apply button and OK button state (enabled / disabled) */
    void setSaveButtonState(bool fState);
    void on_okButton_clicked();
    void on_cancelButton_clicked();
    void on_applyButton_clicked();

    void newSideStakeButton_clicked();
    void editSideStakeButton_clicked();
    void deleteSideStakeButton_clicked();

    void showRestartWarning_Proxy();
    void showRestartWarning_Lang();
    void updateDisplayUnit();
    void updateStyle();
    void hideStartMinimized();
    void hideLimitTxnDisplayDate();
    void hideStakeSplitting();
    void hidePollExpireNotify();
    void hideSideStakeEdit();
    void handleProxyIpValid(QValidatedLineEdit *object, bool fState);
    void handleStakingEfficiencyValid(QValidatedLineEdit *object, bool fState);
    void handleMinStakeSplitValueValid(QValidatedLineEdit *object, bool fState);
    void handlePollExpireNotifyValid(QValidatedLineEdit *object, bool fState);
    void handleSideStakeAllocationInvalid();

    void refreshSideStakeTableModel();

signals:
    void proxyIpValid(QValidatedLineEdit *object, bool fValid);
    void stakingEfficiencyValid(QValidatedLineEdit *object, bool fValid);
    void minStakeSplitValueValid(QValidatedLineEdit *object, bool fValid);
    void pollExpireNotifyValid(QValidatedLineEdit *object, bool fValid);
    void sidestakeAllocationInvalid();

private:
    Ui::OptionsDialog *ui;
    OptionsModel *model;
    MonitoredDataMapper *mapper;
    bool fRestartWarningDisplayed_Proxy;
    bool fRestartWarningDisplayed_Lang;
    bool fProxyIpValid;
    bool fStakingEfficiencyValid;
    bool fMinStakeSplitValueValid;
    bool fPollExpireNotifyValid;

    enum SideStakeTableColumnWidths
    {
        ADDRESS_COLUMN_WIDTH = 200,
        ALLOCATION_COLUMN_WIDTH = 80,
        DESCRIPTION_COLUMN_WIDTH = 130,
        BANSUBNET_COLUMN_WIDTH = 150,
        STATUS_COLUMN_WIDTH = 150
    };

private slots:
    void sidestakeSelectionChanged();
    void updateSideStakeTableView();
};

#endif // BITCOIN_QT_OPTIONSDIALOG_H
