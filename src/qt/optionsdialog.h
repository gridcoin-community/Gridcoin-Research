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

public slots:
    void resizeSideStakeTableColumns(const bool& neighbor_pair_adjust = false, const int& index = 0,
                            const int& old_size = 0, const int& new_size = 0);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

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
    void handleSideStakeDescriptionInvalid();

    void refreshSideStakeTableModel();

    void tabWidgetSelectionChanged(int index);

signals:
    void proxyIpValid(QValidatedLineEdit *object, bool fValid);
    void stakingEfficiencyValid(QValidatedLineEdit *object, bool fValid);
    void minStakeSplitValueValid(QValidatedLineEdit *object, bool fValid);
    void pollExpireNotifyValid(QValidatedLineEdit *object, bool fValid);
    void sidestakeAllocationInvalid();
    void sidestakeDescriptionInvalid();

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

    std::vector<int> m_table_column_sizes;
    bool m_init_column_sizes_set;
    bool m_resize_columns_in_progress;

    enum SideStakeTableColumnWidths
    {
        ADDRESS_COLUMN_WIDTH = 200,
        ALLOCATION_COLUMN_WIDTH = 60,
        DESCRIPTION_COLUMN_WIDTH = 150,
        STATUS_COLUMN_WIDTH = 50
    };

private slots:
    void sidestakeSelectionChanged();
    void updateSideStakeTableView();

    /** Resize address book table columns based on incoming signal */
    void sidestakeTableSectionResized(int index, int old_size, int new_size);

};

#endif // BITCOIN_QT_OPTIONSDIALOG_H
