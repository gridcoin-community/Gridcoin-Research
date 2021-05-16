#ifndef CONSOLIDATEUNSPENTWIZARDSELECTDESTINATIONPAGE_H
#define CONSOLIDATEUNSPENTWIZARDSELECTDESTINATIONPAGE_H

#include <QWizard>

namespace Ui {
    class ConsolidateUnspentWizardSelectDestinationPage;
}

class ConsolidateUnspentWizardSelectDestinationPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit ConsolidateUnspentWizardSelectDestinationPage(QWidget *parent = nullptr);
    ~ConsolidateUnspentWizardSelectDestinationPage();

    void initializePage();

signals:
    void updateFieldsSignal();

public slots:
    void SetAddressList(std::map<QString, QString> addressList);
    void setDefaultAddressSelection(QString address);

private:
    Ui::ConsolidateUnspentWizardSelectDestinationPage *ui;

    std::pair<QString, QString> m_selectedDestinationAddress;

private slots:
    void addressSelectionChanged();
};

#endif // CONSOLIDATEUNSPENTWIZARDSELECTDESTINATIONPAGE_H
