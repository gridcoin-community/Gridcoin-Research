/********************************************************************************
** Form generated from reading UI file 'consolidateunspentwizardselectinputspage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONSOLIDATEUNSPENTWIZARDSELECTINPUTSPAGE_H
#define UI_CONSOLIDATEUNSPENTWIZARDSELECTINPUTSPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>
#include "bitcoinamountfield.h"
#include "coincontroltreewidget.h"

QT_BEGIN_NAMESPACE

class Ui_ConsolidateUnspentWizardSelectInputsPage
{
public:
    QVBoxLayout *verticalLayout_2;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *instructionsHorizontalLayout;
    QLabel *selectInputsIntroLabel;
    QHBoxLayout *treeHorizontalLayout;
    QPushButton *selectAllPushButton;
    QRadioButton *treeModeRadioButton;
    QRadioButton *listModeRadioButton;
    QSpacerItem *treehorizontalSpacer;
    QHBoxLayout *filterHorizontalLayout;
    QLabel *filterLabel;
    QPushButton *filterModePushButton;
    BitcoinAmountField *maxMinOutputValue;
    QPushButton *filterPushButton;
    QSpacerItem *filterHorizontalSpacer;
    CoinControlTreeWidget *treeWidget;
    QHBoxLayout *summaryHorizontalLayout;
    QLabel *outputLimitWarningIconLabel;
    QLabel *outputLimitStopIconLabel;
    QSpacerItem *summaryHorizontalSpacer_1;
    QLabel *quantityTextLabel;
    QLabel *quantityLabel;
    QSpacerItem *summaryHorizontalSpacer_2;
    QLabel *feeTextLabel;
    QLabel *feeLabel;
    QSpacerItem *summaryHorizontalSpacer_3;
    QLabel *afterFeeTextLabel;
    QLabel *afterFeeLabel;
    QCheckBox *isCompleteCheckBox;

    void setupUi(QWizardPage *ConsolidateUnspentWizardSelectInputsPage)
    {
        if (ConsolidateUnspentWizardSelectInputsPage->objectName().isEmpty())
            ConsolidateUnspentWizardSelectInputsPage->setObjectName("ConsolidateUnspentWizardSelectInputsPage");
        ConsolidateUnspentWizardSelectInputsPage->resize(900, 700);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ConsolidateUnspentWizardSelectInputsPage->sizePolicy().hasHeightForWidth());
        ConsolidateUnspentWizardSelectInputsPage->setSizePolicy(sizePolicy);
        verticalLayout_2 = new QVBoxLayout(ConsolidateUnspentWizardSelectInputsPage);
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        instructionsHorizontalLayout = new QHBoxLayout();
        instructionsHorizontalLayout->setObjectName("instructionsHorizontalLayout");
        selectInputsIntroLabel = new QLabel(ConsolidateUnspentWizardSelectInputsPage);
        selectInputsIntroLabel->setObjectName("selectInputsIntroLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(selectInputsIntroLabel->sizePolicy().hasHeightForWidth());
        selectInputsIntroLabel->setSizePolicy(sizePolicy1);
        selectInputsIntroLabel->setWordWrap(true);

        instructionsHorizontalLayout->addWidget(selectInputsIntroLabel);


        verticalLayout->addLayout(instructionsHorizontalLayout);

        treeHorizontalLayout = new QHBoxLayout();
        treeHorizontalLayout->setObjectName("treeHorizontalLayout");
        selectAllPushButton = new QPushButton(ConsolidateUnspentWizardSelectInputsPage);
        selectAllPushButton->setObjectName("selectAllPushButton");

        treeHorizontalLayout->addWidget(selectAllPushButton);

        treeModeRadioButton = new QRadioButton(ConsolidateUnspentWizardSelectInputsPage);
        treeModeRadioButton->setObjectName("treeModeRadioButton");
        treeModeRadioButton->setChecked(true);

        treeHorizontalLayout->addWidget(treeModeRadioButton);

        listModeRadioButton = new QRadioButton(ConsolidateUnspentWizardSelectInputsPage);
        listModeRadioButton->setObjectName("listModeRadioButton");
        listModeRadioButton->setChecked(false);

        treeHorizontalLayout->addWidget(listModeRadioButton);

        treehorizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        treeHorizontalLayout->addItem(treehorizontalSpacer);


        verticalLayout->addLayout(treeHorizontalLayout);

        filterHorizontalLayout = new QHBoxLayout();
        filterHorizontalLayout->setObjectName("filterHorizontalLayout");
        filterLabel = new QLabel(ConsolidateUnspentWizardSelectInputsPage);
        filterLabel->setObjectName("filterLabel");

        filterHorizontalLayout->addWidget(filterLabel);

        filterModePushButton = new QPushButton(ConsolidateUnspentWizardSelectInputsPage);
        filterModePushButton->setObjectName("filterModePushButton");

        filterHorizontalLayout->addWidget(filterModePushButton);

        maxMinOutputValue = new BitcoinAmountField(ConsolidateUnspentWizardSelectInputsPage);
        maxMinOutputValue->setObjectName("maxMinOutputValue");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(maxMinOutputValue->sizePolicy().hasHeightForWidth());
        maxMinOutputValue->setSizePolicy(sizePolicy2);
        maxMinOutputValue->setMinimumSize(QSize(0, 0));

        filterHorizontalLayout->addWidget(maxMinOutputValue);

        filterPushButton = new QPushButton(ConsolidateUnspentWizardSelectInputsPage);
        filterPushButton->setObjectName("filterPushButton");

        filterHorizontalLayout->addWidget(filterPushButton);

        filterHorizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        filterHorizontalLayout->addItem(filterHorizontalSpacer);


        verticalLayout->addLayout(filterHorizontalLayout);

        treeWidget = new CoinControlTreeWidget(ConsolidateUnspentWizardSelectInputsPage);
        treeWidget->headerItem()->setText(0, QString());
        treeWidget->headerItem()->setText(6, QString());
        treeWidget->headerItem()->setText(7, QString());
        treeWidget->headerItem()->setText(8, QString());
        treeWidget->headerItem()->setText(9, QString());
        treeWidget->setObjectName("treeWidget");
        treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        treeWidget->setSortingEnabled(false);
        treeWidget->setColumnCount(10);
        treeWidget->header()->setProperty("showSortIndicator", QVariant(true));
        treeWidget->header()->setStretchLastSection(false);

        verticalLayout->addWidget(treeWidget);

        summaryHorizontalLayout = new QHBoxLayout();
        summaryHorizontalLayout->setObjectName("summaryHorizontalLayout");
        outputLimitWarningIconLabel = new QLabel(ConsolidateUnspentWizardSelectInputsPage);
        outputLimitWarningIconLabel->setObjectName("outputLimitWarningIconLabel");
        outputLimitWarningIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/warning")));
        outputLimitWarningIconLabel->setScaledContents(true);

        summaryHorizontalLayout->addWidget(outputLimitWarningIconLabel);

        outputLimitStopIconLabel = new QLabel(ConsolidateUnspentWizardSelectInputsPage);
        outputLimitStopIconLabel->setObjectName("outputLimitStopIconLabel");
        outputLimitStopIconLabel->setMaximumSize(QSize(64, 64));
        outputLimitStopIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/white_and_red_x")));
        outputLimitStopIconLabel->setScaledContents(true);

        summaryHorizontalLayout->addWidget(outputLimitStopIconLabel);

        summaryHorizontalSpacer_1 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        summaryHorizontalLayout->addItem(summaryHorizontalSpacer_1);

        quantityTextLabel = new QLabel(ConsolidateUnspentWizardSelectInputsPage);
        quantityTextLabel->setObjectName("quantityTextLabel");

        summaryHorizontalLayout->addWidget(quantityTextLabel);

        quantityLabel = new QLabel(ConsolidateUnspentWizardSelectInputsPage);
        quantityLabel->setObjectName("quantityLabel");

        summaryHorizontalLayout->addWidget(quantityLabel);

        summaryHorizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        summaryHorizontalLayout->addItem(summaryHorizontalSpacer_2);

        feeTextLabel = new QLabel(ConsolidateUnspentWizardSelectInputsPage);
        feeTextLabel->setObjectName("feeTextLabel");

        summaryHorizontalLayout->addWidget(feeTextLabel);

        feeLabel = new QLabel(ConsolidateUnspentWizardSelectInputsPage);
        feeLabel->setObjectName("feeLabel");

        summaryHorizontalLayout->addWidget(feeLabel);

        summaryHorizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        summaryHorizontalLayout->addItem(summaryHorizontalSpacer_3);

        afterFeeTextLabel = new QLabel(ConsolidateUnspentWizardSelectInputsPage);
        afterFeeTextLabel->setObjectName("afterFeeTextLabel");

        summaryHorizontalLayout->addWidget(afterFeeTextLabel);

        afterFeeLabel = new QLabel(ConsolidateUnspentWizardSelectInputsPage);
        afterFeeLabel->setObjectName("afterFeeLabel");

        summaryHorizontalLayout->addWidget(afterFeeLabel);

        isCompleteCheckBox = new QCheckBox(ConsolidateUnspentWizardSelectInputsPage);
        isCompleteCheckBox->setObjectName("isCompleteCheckBox");

        summaryHorizontalLayout->addWidget(isCompleteCheckBox);


        verticalLayout->addLayout(summaryHorizontalLayout);


        verticalLayout_2->addLayout(verticalLayout);


        retranslateUi(ConsolidateUnspentWizardSelectInputsPage);

        QMetaObject::connectSlotsByName(ConsolidateUnspentWizardSelectInputsPage);
    } // setupUi

    void retranslateUi(QWizardPage *ConsolidateUnspentWizardSelectInputsPage)
    {
        ConsolidateUnspentWizardSelectInputsPage->setWindowTitle(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "WizardPage", nullptr));
        selectInputsIntroLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Step 1: Select the inputs to be consolidated. Remember that the inputs to the consolidation are your unspent outputs (UTXOs) in your wallet.", nullptr));
        selectAllPushButton->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Select All", nullptr));
        treeModeRadioButton->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Tree Mode", nullptr));
        listModeRadioButton->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "List Mode", nullptr));
        filterLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Select inputs", nullptr));
#if QT_CONFIG(tooltip)
        filterModePushButton->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        filterModePushButton->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "<=", nullptr));
#if QT_CONFIG(tooltip)
        filterPushButton->setToolTip(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Filters the already selected inputs.", nullptr));
#endif // QT_CONFIG(tooltip)
        filterPushButton->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Filter", nullptr));
        QTreeWidgetItem *___qtreewidgetitem = treeWidget->headerItem();
        ___qtreewidgetitem->setText(5, QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Confirmations", nullptr));
        ___qtreewidgetitem->setText(4, QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Date", nullptr));
        ___qtreewidgetitem->setText(3, QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Address", nullptr));
        ___qtreewidgetitem->setText(2, QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Label", nullptr));
        ___qtreewidgetitem->setText(1, QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Amount", nullptr));
#if QT_CONFIG(tooltip)
        ___qtreewidgetitem->setToolTip(5, QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Confirmed", nullptr));
#endif // QT_CONFIG(tooltip)
        outputLimitWarningIconLabel->setText(QString());
        outputLimitStopIconLabel->setText(QString());
        quantityTextLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Quantity", nullptr));
        quantityLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "99999", nullptr));
        feeTextLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "Fee", nullptr));
        feeLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "99.9999", nullptr));
        afterFeeTextLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "After Fee Amount", nullptr));
        afterFeeLabel->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "999999999.9999", nullptr));
        isCompleteCheckBox->setText(QCoreApplication::translate("ConsolidateUnspentWizardSelectInputsPage", "isComplete", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ConsolidateUnspentWizardSelectInputsPage: public Ui_ConsolidateUnspentWizardSelectInputsPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONSOLIDATEUNSPENTWIZARDSELECTINPUTSPAGE_H
