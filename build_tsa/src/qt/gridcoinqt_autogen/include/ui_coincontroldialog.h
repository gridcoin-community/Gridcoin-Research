/********************************************************************************
** Form generated from reading UI file 'coincontroldialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_COINCONTROLDIALOG_H
#define UI_COINCONTROLDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include "bitcoinamountfield.h"
#include "coincontroltreewidget.h"

QT_BEGIN_NAMESPACE

class Ui_CoinControlDialog
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayoutTop;
    QFormLayout *formLayoutCoinControlQuantity;
    QLabel *coinControlQuantityTextLabel;
    QLabel *coinControlQuantityLabel;
    QLabel *coinControlBytesTextLabel;
    QLabel *coinControlBytesLabel;
    QFormLayout *formLayoutCoinControlAmount;
    QLabel *coinControlAmountTextLabel;
    QLabel *coinControlAmountLabel;
    QFormLayout *formLayoutCoinControlFee;
    QLabel *coinControlFeeTextLabel;
    QLabel *coinControlFeeLabel;
    QLabel *coinControlLowOutputTextLabel;
    QLabel *coinControlLowOutputLabel;
    QFormLayout *formLayoutCoinControlChange;
    QLabel *coinControlAfterFeeTextLabel;
    QLabel *coinControlAfterFeeLabel;
    QLabel *coinControlChangeTextLabel;
    QLabel *coinControlChangeLabel;
    QHBoxLayout *treeHorizontalLayout;
    QPushButton *selectAllPushButton;
    QRadioButton *treeModeRadioButton;
    QRadioButton *listModeRadioButton;
    QSpacerItem *treeHorizontalSpacer;
    QHBoxLayout *filterHorizontalLayout;
    QLabel *filterLabel;
    QPushButton *filterModePushButton;
    BitcoinAmountField *maxMinOutputValue;
    QPushButton *filterPushButton;
    QPushButton *consolidateButton;
    QLabel *consolidateSendReadyLabel;
    QSpacerItem *filterHorizontalSpacer;
    CoinControlTreeWidget *treeWidget;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *CoinControlDialog)
    {
        if (CoinControlDialog->objectName().isEmpty())
            CoinControlDialog->setObjectName("CoinControlDialog");
        CoinControlDialog->resize(960, 550);
        verticalLayout = new QVBoxLayout(CoinControlDialog);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayoutTop = new QHBoxLayout();
        horizontalLayoutTop->setObjectName("horizontalLayoutTop");
        horizontalLayoutTop->setContentsMargins(-1, 0, -1, 10);
        formLayoutCoinControlQuantity = new QFormLayout();
        formLayoutCoinControlQuantity->setObjectName("formLayoutCoinControlQuantity");
        formLayoutCoinControlQuantity->setHorizontalSpacing(10);
        formLayoutCoinControlQuantity->setVerticalSpacing(10);
        formLayoutCoinControlQuantity->setContentsMargins(6, -1, 6, -1);
        coinControlQuantityTextLabel = new QLabel(CoinControlDialog);
        coinControlQuantityTextLabel->setObjectName("coinControlQuantityTextLabel");

        formLayoutCoinControlQuantity->setWidget(0, QFormLayout::ItemRole::LabelRole, coinControlQuantityTextLabel);

        coinControlQuantityLabel = new QLabel(CoinControlDialog);
        coinControlQuantityLabel->setObjectName("coinControlQuantityLabel");
        coinControlQuantityLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlQuantityLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlQuantityLabel->setText(QString::fromUtf8("0"));
        coinControlQuantityLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControlQuantity->setWidget(0, QFormLayout::ItemRole::FieldRole, coinControlQuantityLabel);

        coinControlBytesTextLabel = new QLabel(CoinControlDialog);
        coinControlBytesTextLabel->setObjectName("coinControlBytesTextLabel");

        formLayoutCoinControlQuantity->setWidget(1, QFormLayout::ItemRole::LabelRole, coinControlBytesTextLabel);

        coinControlBytesLabel = new QLabel(CoinControlDialog);
        coinControlBytesLabel->setObjectName("coinControlBytesLabel");
        coinControlBytesLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlBytesLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlBytesLabel->setText(QString::fromUtf8("0"));
        coinControlBytesLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControlQuantity->setWidget(1, QFormLayout::ItemRole::FieldRole, coinControlBytesLabel);


        horizontalLayoutTop->addLayout(formLayoutCoinControlQuantity);

        formLayoutCoinControlAmount = new QFormLayout();
        formLayoutCoinControlAmount->setObjectName("formLayoutCoinControlAmount");
        formLayoutCoinControlAmount->setHorizontalSpacing(10);
        formLayoutCoinControlAmount->setVerticalSpacing(10);
        formLayoutCoinControlAmount->setContentsMargins(6, -1, 6, -1);
        coinControlAmountTextLabel = new QLabel(CoinControlDialog);
        coinControlAmountTextLabel->setObjectName("coinControlAmountTextLabel");

        formLayoutCoinControlAmount->setWidget(0, QFormLayout::ItemRole::LabelRole, coinControlAmountTextLabel);

        coinControlAmountLabel = new QLabel(CoinControlDialog);
        coinControlAmountLabel->setObjectName("coinControlAmountLabel");
        coinControlAmountLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlAmountLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlAmountLabel->setText(QString::fromUtf8("0.00 GRC"));
        coinControlAmountLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControlAmount->setWidget(0, QFormLayout::ItemRole::FieldRole, coinControlAmountLabel);


        horizontalLayoutTop->addLayout(formLayoutCoinControlAmount);

        formLayoutCoinControlFee = new QFormLayout();
        formLayoutCoinControlFee->setObjectName("formLayoutCoinControlFee");
        formLayoutCoinControlFee->setHorizontalSpacing(10);
        formLayoutCoinControlFee->setVerticalSpacing(10);
        formLayoutCoinControlFee->setContentsMargins(6, -1, 6, -1);
        coinControlFeeTextLabel = new QLabel(CoinControlDialog);
        coinControlFeeTextLabel->setObjectName("coinControlFeeTextLabel");

        formLayoutCoinControlFee->setWidget(0, QFormLayout::ItemRole::LabelRole, coinControlFeeTextLabel);

        coinControlFeeLabel = new QLabel(CoinControlDialog);
        coinControlFeeLabel->setObjectName("coinControlFeeLabel");
        coinControlFeeLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlFeeLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlFeeLabel->setText(QString::fromUtf8("0.00 GRC"));
        coinControlFeeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControlFee->setWidget(0, QFormLayout::ItemRole::FieldRole, coinControlFeeLabel);

        coinControlLowOutputTextLabel = new QLabel(CoinControlDialog);
        coinControlLowOutputTextLabel->setObjectName("coinControlLowOutputTextLabel");
        coinControlLowOutputTextLabel->setEnabled(false);

        formLayoutCoinControlFee->setWidget(1, QFormLayout::ItemRole::LabelRole, coinControlLowOutputTextLabel);

        coinControlLowOutputLabel = new QLabel(CoinControlDialog);
        coinControlLowOutputLabel->setObjectName("coinControlLowOutputLabel");
        coinControlLowOutputLabel->setEnabled(false);
        coinControlLowOutputLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlLowOutputLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlLowOutputLabel->setText(QString::fromUtf8("no"));
        coinControlLowOutputLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControlFee->setWidget(1, QFormLayout::ItemRole::FieldRole, coinControlLowOutputLabel);


        horizontalLayoutTop->addLayout(formLayoutCoinControlFee);

        formLayoutCoinControlChange = new QFormLayout();
        formLayoutCoinControlChange->setObjectName("formLayoutCoinControlChange");
        formLayoutCoinControlChange->setHorizontalSpacing(10);
        formLayoutCoinControlChange->setVerticalSpacing(10);
        formLayoutCoinControlChange->setContentsMargins(6, -1, 6, -1);
        coinControlAfterFeeTextLabel = new QLabel(CoinControlDialog);
        coinControlAfterFeeTextLabel->setObjectName("coinControlAfterFeeTextLabel");

        formLayoutCoinControlChange->setWidget(0, QFormLayout::ItemRole::LabelRole, coinControlAfterFeeTextLabel);

        coinControlAfterFeeLabel = new QLabel(CoinControlDialog);
        coinControlAfterFeeLabel->setObjectName("coinControlAfterFeeLabel");
        coinControlAfterFeeLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlAfterFeeLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlAfterFeeLabel->setText(QString::fromUtf8("0.00 GRC"));
        coinControlAfterFeeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControlChange->setWidget(0, QFormLayout::ItemRole::FieldRole, coinControlAfterFeeLabel);

        coinControlChangeTextLabel = new QLabel(CoinControlDialog);
        coinControlChangeTextLabel->setObjectName("coinControlChangeTextLabel");
        coinControlChangeTextLabel->setEnabled(false);

        formLayoutCoinControlChange->setWidget(1, QFormLayout::ItemRole::LabelRole, coinControlChangeTextLabel);

        coinControlChangeLabel = new QLabel(CoinControlDialog);
        coinControlChangeLabel->setObjectName("coinControlChangeLabel");
        coinControlChangeLabel->setEnabled(false);
        coinControlChangeLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        coinControlChangeLabel->setContextMenuPolicy(Qt::ActionsContextMenu);
        coinControlChangeLabel->setText(QString::fromUtf8("0.00 GRC"));
        coinControlChangeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControlChange->setWidget(1, QFormLayout::ItemRole::FieldRole, coinControlChangeLabel);


        horizontalLayoutTop->addLayout(formLayoutCoinControlChange);


        verticalLayout->addLayout(horizontalLayoutTop);

        treeHorizontalLayout = new QHBoxLayout();
        treeHorizontalLayout->setSpacing(14);
        treeHorizontalLayout->setObjectName("treeHorizontalLayout");
        selectAllPushButton = new QPushButton(CoinControlDialog);
        selectAllPushButton->setObjectName("selectAllPushButton");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(selectAllPushButton->sizePolicy().hasHeightForWidth());
        selectAllPushButton->setSizePolicy(sizePolicy);

        treeHorizontalLayout->addWidget(selectAllPushButton);

        treeModeRadioButton = new QRadioButton(CoinControlDialog);
        treeModeRadioButton->setObjectName("treeModeRadioButton");
        sizePolicy.setHeightForWidth(treeModeRadioButton->sizePolicy().hasHeightForWidth());
        treeModeRadioButton->setSizePolicy(sizePolicy);
        treeModeRadioButton->setChecked(true);

        treeHorizontalLayout->addWidget(treeModeRadioButton);

        listModeRadioButton = new QRadioButton(CoinControlDialog);
        listModeRadioButton->setObjectName("listModeRadioButton");
        sizePolicy.setHeightForWidth(listModeRadioButton->sizePolicy().hasHeightForWidth());
        listModeRadioButton->setSizePolicy(sizePolicy);

        treeHorizontalLayout->addWidget(listModeRadioButton);

        treeHorizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        treeHorizontalLayout->addItem(treeHorizontalSpacer);


        verticalLayout->addLayout(treeHorizontalLayout);

        filterHorizontalLayout = new QHBoxLayout();
        filterHorizontalLayout->setObjectName("filterHorizontalLayout");
        filterLabel = new QLabel(CoinControlDialog);
        filterLabel->setObjectName("filterLabel");

        filterHorizontalLayout->addWidget(filterLabel);

        filterModePushButton = new QPushButton(CoinControlDialog);
        filterModePushButton->setObjectName("filterModePushButton");

        filterHorizontalLayout->addWidget(filterModePushButton);

        maxMinOutputValue = new BitcoinAmountField(CoinControlDialog);
        maxMinOutputValue->setObjectName("maxMinOutputValue");
        sizePolicy.setHeightForWidth(maxMinOutputValue->sizePolicy().hasHeightForWidth());
        maxMinOutputValue->setSizePolicy(sizePolicy);
        maxMinOutputValue->setMinimumSize(QSize(0, 0));

        filterHorizontalLayout->addWidget(maxMinOutputValue);

        filterPushButton = new QPushButton(CoinControlDialog);
        filterPushButton->setObjectName("filterPushButton");

        filterHorizontalLayout->addWidget(filterPushButton);

        consolidateButton = new QPushButton(CoinControlDialog);
        consolidateButton->setObjectName("consolidateButton");

        filterHorizontalLayout->addWidget(consolidateButton);

        consolidateSendReadyLabel = new QLabel(CoinControlDialog);
        consolidateSendReadyLabel->setObjectName("consolidateSendReadyLabel");

        filterHorizontalLayout->addWidget(consolidateSendReadyLabel);

        filterHorizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        filterHorizontalLayout->addItem(filterHorizontalSpacer);

        filterHorizontalLayout->setStretch(2, 3);

        verticalLayout->addLayout(filterHorizontalLayout);

        treeWidget = new CoinControlTreeWidget(CoinControlDialog);
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

        buttonBox = new QDialogButtonBox(CoinControlDialog);
        buttonBox->setObjectName("buttonBox");
        sizePolicy.setHeightForWidth(buttonBox->sizePolicy().hasHeightForWidth());
        buttonBox->setSizePolicy(sizePolicy);
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(CoinControlDialog);

        QMetaObject::connectSlotsByName(CoinControlDialog);
    } // setupUi

    void retranslateUi(QDialog *CoinControlDialog)
    {
        CoinControlDialog->setWindowTitle(QCoreApplication::translate("CoinControlDialog", "Coin Control", nullptr));
        coinControlQuantityTextLabel->setText(QCoreApplication::translate("CoinControlDialog", "Quantity:", nullptr));
        coinControlBytesTextLabel->setText(QCoreApplication::translate("CoinControlDialog", "Bytes:", nullptr));
        coinControlAmountTextLabel->setText(QCoreApplication::translate("CoinControlDialog", "Amount:", nullptr));
        coinControlFeeTextLabel->setText(QCoreApplication::translate("CoinControlDialog", "Fee:", nullptr));
        coinControlLowOutputTextLabel->setText(QCoreApplication::translate("CoinControlDialog", "Low Output:", nullptr));
        coinControlAfterFeeTextLabel->setText(QCoreApplication::translate("CoinControlDialog", "After Fee:", nullptr));
        coinControlChangeTextLabel->setText(QCoreApplication::translate("CoinControlDialog", "Change:", nullptr));
#if QT_CONFIG(tooltip)
        selectAllPushButton->setToolTip(QCoreApplication::translate("CoinControlDialog", "Toggles between selecting all and selecting none.", nullptr));
#endif // QT_CONFIG(tooltip)
        selectAllPushButton->setText(QCoreApplication::translate("CoinControlDialog", "Select All", nullptr));
        treeModeRadioButton->setText(QCoreApplication::translate("CoinControlDialog", "Tree &mode", nullptr));
        listModeRadioButton->setText(QCoreApplication::translate("CoinControlDialog", "&List mode", nullptr));
        filterLabel->setText(QCoreApplication::translate("CoinControlDialog", "Select inputs", nullptr));
#if QT_CONFIG(tooltip)
        filterModePushButton->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        filterModePushButton->setText(QCoreApplication::translate("CoinControlDialog", "<=", nullptr));
#if QT_CONFIG(tooltip)
        filterPushButton->setToolTip(QCoreApplication::translate("CoinControlDialog", "Filters the already selected inputs.", nullptr));
#endif // QT_CONFIG(tooltip)
        filterPushButton->setText(QCoreApplication::translate("CoinControlDialog", "Filter", nullptr));
#if QT_CONFIG(tooltip)
        consolidateButton->setToolTip(QCoreApplication::translate("CoinControlDialog", "Pushing this button after making a input selection either manually or with the filter will present a destination address list where you specify a single address as the destination for the consolidated output. The send (Pay To) entry will be filled in with this address and you can finish the consolidation by pressing the send button.", nullptr));
#endif // QT_CONFIG(tooltip)
        consolidateButton->setText(QCoreApplication::translate("CoinControlDialog", "Consolidate", nullptr));
#if QT_CONFIG(tooltip)
        consolidateSendReadyLabel->setToolTip(QCoreApplication::translate("CoinControlDialog", "The consolidation transaction is ready to send to self. Please press the ok button to go to the send dialog.", nullptr));
#endif // QT_CONFIG(tooltip)
        consolidateSendReadyLabel->setText(QCoreApplication::translate("CoinControlDialog", "Ready to consolidate", nullptr));
        QTreeWidgetItem *___qtreewidgetitem = treeWidget->headerItem();
        ___qtreewidgetitem->setText(5, QCoreApplication::translate("CoinControlDialog", "Confirmations", nullptr));
        ___qtreewidgetitem->setText(4, QCoreApplication::translate("CoinControlDialog", "Date", nullptr));
        ___qtreewidgetitem->setText(3, QCoreApplication::translate("CoinControlDialog", "Address", nullptr));
        ___qtreewidgetitem->setText(2, QCoreApplication::translate("CoinControlDialog", "Label", nullptr));
        ___qtreewidgetitem->setText(1, QCoreApplication::translate("CoinControlDialog", "Amount", nullptr));
#if QT_CONFIG(tooltip)
        ___qtreewidgetitem->setToolTip(5, QCoreApplication::translate("CoinControlDialog", "Confirmed", nullptr));
#endif // QT_CONFIG(tooltip)
    } // retranslateUi

};

namespace Ui {
    class CoinControlDialog: public Ui_CoinControlDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_COINCONTROLDIALOG_H
