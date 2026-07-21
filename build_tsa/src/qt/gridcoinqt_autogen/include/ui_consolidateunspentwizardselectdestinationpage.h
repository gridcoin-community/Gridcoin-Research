/********************************************************************************
** Form generated from reading UI file 'consolidateunspentwizardselectdestinationpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONSOLIDATEUNSPENTWIZARDSELECTDESTINATIONPAGE_H
#define UI_CONSOLIDATEUNSPENTWIZARDSELECTDESTINATIONPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ConsolidateUnspentWizardSelectDestinationPage
{
public:
    QVBoxLayout *verticalLayout_2;
    QVBoxLayout *verticalLayout;
    QLabel *selectDestinationIntroLabel;
    QHBoxLayout *horizontalLayout;
    QTableWidget *addressTableWidget;
    QHBoxLayout *horizontalLayout_2;
    QLabel *selectedLabel;
    QHBoxLayout *horizontalLayout_3;
    QLabel *selectedAddressLabel;
    QSpacerItem *horizontalSpacer;
    QLabel *selectedAddress;
    QCheckBox *isCompleteCheckBox;

    void setupUi(QWizardPage *ConsolidateUnspentWizardSelectDestinationPage)
    {
        if (ConsolidateUnspentWizardSelectDestinationPage->objectName().isEmpty())
            ConsolidateUnspentWizardSelectDestinationPage->setObjectName("ConsolidateUnspentWizardSelectDestinationPage");
        ConsolidateUnspentWizardSelectDestinationPage->resize(900, 700);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ConsolidateUnspentWizardSelectDestinationPage->sizePolicy().hasHeightForWidth());
        ConsolidateUnspentWizardSelectDestinationPage->setSizePolicy(sizePolicy);
        verticalLayout_2 = new QVBoxLayout(ConsolidateUnspentWizardSelectDestinationPage);
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        selectDestinationIntroLabel = new QLabel(ConsolidateUnspentWizardSelectDestinationPage);
        selectDestinationIntroLabel->setObjectName("selectDestinationIntroLabel");
        selectDestinationIntroLabel->setWordWrap(true);

        verticalLayout->addWidget(selectDestinationIntroLabel);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        addressTableWidget = new QTableWidget(ConsolidateUnspentWizardSelectDestinationPage);
        if (addressTableWidget->columnCount() < 2)
            addressTableWidget->setColumnCount(2);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        addressTableWidget->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        addressTableWidget->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        addressTableWidget->setObjectName("addressTableWidget");
        addressTableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        addressTableWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        addressTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        addressTableWidget->setAlternatingRowColors(true);
        addressTableWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        addressTableWidget->horizontalHeader()->setMinimumSectionSize(90);
        addressTableWidget->horizontalHeader()->setStretchLastSection(true);
        addressTableWidget->verticalHeader()->setVisible(false);

        horizontalLayout->addWidget(addressTableWidget);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        selectedLabel = new QLabel(ConsolidateUnspentWizardSelectDestinationPage);
        selectedLabel->setObjectName("selectedLabel");

        horizontalLayout_2->addWidget(selectedLabel);


        verticalLayout->addLayout(horizontalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        selectedAddressLabel = new QLabel(ConsolidateUnspentWizardSelectDestinationPage);
        selectedAddressLabel->setObjectName("selectedAddressLabel");

        horizontalLayout_3->addWidget(selectedAddressLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer);

        selectedAddress = new QLabel(ConsolidateUnspentWizardSelectDestinationPage);
        selectedAddress->setObjectName("selectedAddress");

        horizontalLayout_3->addWidget(selectedAddress);

        horizontalLayout_3->setStretch(2, 1);

        verticalLayout->addLayout(horizontalLayout_3);

        isCompleteCheckBox = new QCheckBox(ConsolidateUnspentWizardSelectDestinationPage);
        isCompleteCheckBox->setObjectName("isCompleteCheckBox");

        verticalLayout->addWidget(isCompleteCheckBox);


        verticalLayout_2->addLayout(verticalLayout);


        retranslateUi(ConsolidateUnspentWizardSelectDestinationPage);

        QMetaObject::connectSlotsByName(ConsolidateUnspentWizardSelectDestinationPage);
    } // setupUi

    void retranslateUi(QWizardPage *ConsolidateUnspentWizardSelectDestinationPage)
    {
        ConsolidateUnspentWizardSelectDestinationPage->setWindowTitle(QCoreApplication::translate("ConsolidateUnspentWizardSelectDestinationPage", "WizardPage", nullptr));
        selectDestinationIntroLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectDestinationPage", "Step 2: Select the destination address for the consolidation transaction. Note that all of the selected inputs will be consolidated to an output on this address. If there is a very small amount of change (due to uncertainty in the fee calculation), it will also be sent to this address. If you selected inputs only from a particular address on the previous page, then that address will already be selected by default.", nullptr));
        QTableWidgetItem *___qtablewidgetitem = addressTableWidget->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectDestinationPage", "Label", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = addressTableWidget->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectDestinationPage", "Address", nullptr));
        selectedLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectDestinationPage", "Currently selected:", nullptr));
        selectedAddressLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectDestinationPage", "Label", nullptr));
        selectedAddress->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectDestinationPage", "Address", nullptr));
        isCompleteCheckBox->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectDestinationPage", "isComplete", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ConsolidateUnspentWizardSelectDestinationPage: public Ui_ConsolidateUnspentWizardSelectDestinationPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONSOLIDATEUNSPENTWIZARDSELECTDESTINATIONPAGE_H
