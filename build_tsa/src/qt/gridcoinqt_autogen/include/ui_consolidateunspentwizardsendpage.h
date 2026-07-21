/********************************************************************************
** Form generated from reading UI file 'consolidateunspentwizardsendpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONSOLIDATEUNSPENTWIZARDSENDPAGE_H
#define UI_CONSOLIDATEUNSPENTWIZARDSENDPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ConsolidateUnspentWizardSendPage
{
public:
    QVBoxLayout *verticalLayout_2;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *sendIntroLabel;
    QFormLayout *formLayout_2;
    QLabel *InputQuantityTextLabel;
    QLabel *InputQuantityLabel;
    QLabel *feeTextLabel;
    QLabel *feeLabel;
    QLabel *afterFeeAmountTextLabel;
    QLabel *afterFeeAmountLabel;
    QLabel *destinationAddressTextLabel;
    QLabel *destinationAddressLabel;
    QLabel *destinationAddressLabelTextLabel;
    QLabel *destinationAddressLabelLabel;

    void setupUi(QWizardPage *ConsolidateUnspentWizardSendPage)
    {
        if (ConsolidateUnspentWizardSendPage->objectName().isEmpty())
            ConsolidateUnspentWizardSendPage->setObjectName("ConsolidateUnspentWizardSendPage");
        ConsolidateUnspentWizardSendPage->resize(900, 700);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ConsolidateUnspentWizardSendPage->sizePolicy().hasHeightForWidth());
        ConsolidateUnspentWizardSendPage->setSizePolicy(sizePolicy);
        verticalLayout_2 = new QVBoxLayout(ConsolidateUnspentWizardSendPage);
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        sendIntroLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        sendIntroLabel->setObjectName("sendIntroLabel");
        sendIntroLabel->setWordWrap(true);

        horizontalLayout->addWidget(sendIntroLabel);


        verticalLayout->addLayout(horizontalLayout);

        formLayout_2 = new QFormLayout();
        formLayout_2->setObjectName("formLayout_2");
        InputQuantityTextLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        InputQuantityTextLabel->setObjectName("InputQuantityTextLabel");

        formLayout_2->setWidget(0, QFormLayout::ItemRole::LabelRole, InputQuantityTextLabel);

        InputQuantityLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        InputQuantityLabel->setObjectName("InputQuantityLabel");

        formLayout_2->setWidget(0, QFormLayout::ItemRole::FieldRole, InputQuantityLabel);

        feeTextLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        feeTextLabel->setObjectName("feeTextLabel");

        formLayout_2->setWidget(1, QFormLayout::ItemRole::LabelRole, feeTextLabel);

        feeLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        feeLabel->setObjectName("feeLabel");

        formLayout_2->setWidget(1, QFormLayout::ItemRole::FieldRole, feeLabel);

        afterFeeAmountTextLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        afterFeeAmountTextLabel->setObjectName("afterFeeAmountTextLabel");

        formLayout_2->setWidget(2, QFormLayout::ItemRole::LabelRole, afterFeeAmountTextLabel);

        afterFeeAmountLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        afterFeeAmountLabel->setObjectName("afterFeeAmountLabel");

        formLayout_2->setWidget(2, QFormLayout::ItemRole::FieldRole, afterFeeAmountLabel);

        destinationAddressTextLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        destinationAddressTextLabel->setObjectName("destinationAddressTextLabel");

        formLayout_2->setWidget(4, QFormLayout::ItemRole::LabelRole, destinationAddressTextLabel);

        destinationAddressLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        destinationAddressLabel->setObjectName("destinationAddressLabel");

        formLayout_2->setWidget(4, QFormLayout::ItemRole::FieldRole, destinationAddressLabel);

        destinationAddressLabelTextLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        destinationAddressLabelTextLabel->setObjectName("destinationAddressLabelTextLabel");

        formLayout_2->setWidget(3, QFormLayout::ItemRole::LabelRole, destinationAddressLabelTextLabel);

        destinationAddressLabelLabel = new QLabel(ConsolidateUnspentWizardSendPage);
        destinationAddressLabelLabel->setObjectName("destinationAddressLabelLabel");

        formLayout_2->setWidget(3, QFormLayout::ItemRole::FieldRole, destinationAddressLabelLabel);


        verticalLayout->addLayout(formLayout_2);


        verticalLayout_2->addLayout(verticalLayout);


        retranslateUi(ConsolidateUnspentWizardSendPage);

        QMetaObject::connectSlotsByName(ConsolidateUnspentWizardSendPage);
    } // setupUi

    void retranslateUi(QWizardPage *ConsolidateUnspentWizardSendPage)
    {
        ConsolidateUnspentWizardSendPage->setWindowTitle(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "WizardPage", nullptr));
        sendIntroLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "Step 3: Confirm Consolidation Transaction Details. Transaction will be ready to send when Finish is pressed.", nullptr));
        InputQuantityTextLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "Number of Inputs", nullptr));
        InputQuantityLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "999999", nullptr));
        feeTextLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "Transaction Fee", nullptr));
        feeLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "99.9999", nullptr));
        afterFeeAmountTextLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "Amount", nullptr));
        afterFeeAmountLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "999999999.9999", nullptr));
        destinationAddressTextLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "Destination Address", nullptr));
        destinationAddressLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "address", nullptr));
        destinationAddressLabelTextLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "Destination Address Label", nullptr));
        destinationAddressLabelLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSendPage", "label", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ConsolidateUnspentWizardSendPage: public Ui_ConsolidateUnspentWizardSendPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONSOLIDATEUNSPENTWIZARDSENDPAGE_H
