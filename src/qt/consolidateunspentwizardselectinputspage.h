#ifndef BITCOIN_QT_CONSOLIDATEUNSPENTWIZARDSELECTINPUTSPAGE_H
#define BITCOIN_QT_CONSOLIDATEUNSPENTWIZARDSELECTINPUTSPAGE_H

#include "walletmodel.h"
#include "amount.h"

#include <QWizard>

namespace Ui {
    class ConsolidateUnspentWizardSelectInputsPage;
}

class CoinSelectionModel;

//!
//! \brief The consolidation wizard's input-selection page over the shared
//! windowed CoinSelectionModel (#3183, VIEW_COIN_WIZARD) — the former
//! copy-paste of CoinControlDialog's QTreeWidget plumbing is gone. The
//! summary/fee pipeline (computeCoinControlSummary), the registerField
//! wiring, and the InputStatus warning state machine are unchanged; the
//! address-list / default-address signals are re-expressed over the group
//! directory's server-side aggregates.
//!
class ConsolidateUnspentWizardSelectInputsPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ConsolidateUnspentWizardSelectInputsPage(QWidget *parent = nullptr);
    ~ConsolidateUnspentWizardSelectInputsPage();

    void setModel(WalletModel*);
    void setCoinControl(interfaces::WalletCoinControl* coinControl);
    void setPayAmounts(QList<qint64> *payAmounts);

signals:
    void setAddressListSignal(std::map<QString, QString>);
    void setDefaultAddressSignal(QString);
    void updateFieldsSignal();

public slots:
    void updateLabels();

private:
    Ui::ConsolidateUnspentWizardSelectInputsPage *ui;
    interfaces::WalletCoinControl *coinControl{nullptr};
    QList<qint64> *payAmounts{nullptr};
    WalletModel *model{nullptr};
    CoinSelectionModel *m_selection_model{nullptr};
    int sortColumn;
    Qt::SortOrder sortOrder;
    size_t m_InputSelectionLimit{0};
    Qt::CheckState m_ToState = Qt::Checked;
    bool m_FilterMode = true;
    bool m_FilterValueValid = false;
    //! Set when the filter's input cap culled the selection; cleared by any
    //! user-driven selection mutation (the WARNING-vs-STOP boundary at
    //! exactly m_InputSelectionLimit selected inputs depends on it).
    bool m_InputSelectionLimitedByFilter = false;
    //! True while the filter's own bulk mutation runs, so its
    //! selectionChanged does not clear the flag it just set.
    bool m_ApplyingFilter = false;

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
    void headerSectionClicked(int);
    void buttonSelectAllClicked();
    void maxMinOutputValueChanged();
    void buttonFilterModeClicked();
    void buttonFilterClicked();
    void SetOutputWarningStop(InputStatus input_status);
    void modelSelectionChanged();
    void modelLoadingFinished();
};

#endif // BITCOIN_QT_CONSOLIDATEUNSPENTWIZARDSELECTINPUTSPAGE_H
