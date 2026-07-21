/********************************************************************************
** Form generated from reading UI file 'consolidateunspentwizard.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONSOLIDATEUNSPENTWIZARD_H
#define UI_CONSOLIDATEUNSPENTWIZARD_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWizard>
#include "consolidateunspentwizardselectdestinationpage.h"
#include "consolidateunspentwizardselectinputspage.h"
#include "consolidateunspentwizardsendpage.h"

QT_BEGIN_NAMESPACE

class Ui_ConsolidateUnspentWizard
{
public:
    ConsolidateUnspentWizardSelectInputsPage *selectInputsPage;
    ConsolidateUnspentWizardSelectDestinationPage *selectDestinationPage;
    ConsolidateUnspentWizardSendPage *sendPage;

    void setupUi(QWizard *ConsolidateUnspentWizard)
    {
        if (ConsolidateUnspentWizard->objectName().isEmpty())
            ConsolidateUnspentWizard->setObjectName("ConsolidateUnspentWizard");
        ConsolidateUnspentWizard->resize(930, 700);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ConsolidateUnspentWizard->sizePolicy().hasHeightForWidth());
        ConsolidateUnspentWizard->setSizePolicy(sizePolicy);
        ConsolidateUnspentWizard->setSizeGripEnabled(true);
        ConsolidateUnspentWizard->setModal(true);
        ConsolidateUnspentWizard->setWizardStyle(QWizard::ClassicStyle);
        selectInputsPage = new ConsolidateUnspentWizardSelectInputsPage();
        selectInputsPage->setObjectName("selectInputsPage");
        sizePolicy.setHeightForWidth(selectInputsPage->sizePolicy().hasHeightForWidth());
        selectInputsPage->setSizePolicy(sizePolicy);
        ConsolidateUnspentWizard->setPage(0, selectInputsPage);
        selectDestinationPage = new ConsolidateUnspentWizardSelectDestinationPage();
        selectDestinationPage->setObjectName("selectDestinationPage");
        sizePolicy.setHeightForWidth(selectDestinationPage->sizePolicy().hasHeightForWidth());
        selectDestinationPage->setSizePolicy(sizePolicy);
        ConsolidateUnspentWizard->setPage(1, selectDestinationPage);
        sendPage = new ConsolidateUnspentWizardSendPage();
        sendPage->setObjectName("sendPage");
        sizePolicy.setHeightForWidth(sendPage->sizePolicy().hasHeightForWidth());
        sendPage->setSizePolicy(sizePolicy);
        ConsolidateUnspentWizard->setPage(2, sendPage);

        retranslateUi(ConsolidateUnspentWizard);

        QMetaObject::connectSlotsByName(ConsolidateUnspentWizard);
    } // setupUi

    void retranslateUi(QWizard *ConsolidateUnspentWizard)
    {
        ConsolidateUnspentWizard->setWindowTitle(QCoreApplication::translate("ConsolidateUnspentWizard", "Consolidate Unspent Transaction Outputs (UTXOs)", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ConsolidateUnspentWizard: public Ui_ConsolidateUnspentWizard {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONSOLIDATEUNSPENTWIZARD_H
