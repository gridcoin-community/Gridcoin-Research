#ifndef CONSOLIDATEUNSPENTWIZARDSELECTINPUTS_H
#define CONSOLIDATEUNSPENTWIZARDSELECTINPUTS_H

#include "walletmodel.h"
#include "amount.h"

#include <QWizard>
#include <QTreeWidgetItem>

namespace Ui {
    class ConsolidateUnspentWizardSelectInputsPage;
}

class CoinControlDialog;

class ConsolidateUnspentWizardSelectInputsPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ConsolidateUnspentWizardSelectInputsPage(QWidget *parent = nullptr);
    ~ConsolidateUnspentWizardSelectInputsPage();

    void setModel(WalletModel*);
    void setCoinControl(CCoinControl* coinControl);
    void setPayAmounts(QList<qint64> *payAmounts);

signals:
    void setAddressListSignal(std::map<QString, QString>);
    void setDefaultAddressSignal(QString);
    void updateFieldsSignal();

public slots:
    void updateLabels();

private:
    Ui::ConsolidateUnspentWizardSelectInputsPage *ui;
    CCoinControl *coinControl;
    QList<qint64> *payAmounts;
    WalletModel *model;
    int sortColumn;
    Qt::SortOrder sortOrder;
    size_t m_InputSelectionLimit;
    Qt::CheckState m_ToState = Qt::Checked;
    bool m_FilterMode = true;
    bool m_FilterValueValid = false;
    bool m_InputSelectionLimitedByFilter = false;
    bool m_ViewItemsChangedViaFilter = false;

    QString strPad(QString, int, QString);
    void sortView(int, Qt::SortOrder);
    void updateView();
    bool filterInputsByValue(const bool& less, const CAmount& inputFilterValue, const unsigned int& inputSelectionLimit);

    enum
    {
        COLUMN_CHECKBOX,
        COLUMN_AMOUNT,
        COLUMN_LABEL,
        COLUMN_ADDRESS,
        COLUMN_DATE,
        COLUMN_CONFIRMATIONS,
        COLUMN_PRIORITY,
        COLUMN_TXHASH,
        COLUMN_VOUT_INDEX,
        COLUMN_AMOUNT_INT64,
        COLUMN_PRIORITY_INT64,
        COLUMN_CHANGE_BOOL
    };

    enum InputStatus
    {
        INSUFFICIENT_OUTPUTS,
        NORMAL,
        WARNING,
        STOP
    };

private slots:
    void treeModeRadioButton(bool);
    void listModeRadioButton(bool);
    void viewItemChanged(QTreeWidgetItem*, int);
    void headerSectionClicked(int);
    void buttonSelectAllClicked();
    void maxMinOutputValueChanged();
    void buttonFilterModeClicked();
    void buttonFilterClicked();
    void SetOutputWarningStop(InputStatus input_status);
};

#endif // CONSOLIDATEUNSPENTWIZARDSELECTINPUTS_H
