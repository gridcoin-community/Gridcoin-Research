/********************************************************************************
** Form generated from reading UI file 'consolidateunspentdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONSOLIDATEUNSPENTDIALOG_H
#define UI_CONSOLIDATEUNSPENTDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTableWidget>

QT_BEGIN_NAMESPACE

class Ui_ConsolidateUnspentDialog
{
public:
    QDialogButtonBox *buttonBox;
    QLabel *addressTableWidgetLabel;
    QLabel *outputLimitWarningLabel;
    QTableWidget *addressTableWidget;
    QLabel *outputLimitWarningIconLabel;

    void setupUi(QDialog *ConsolidateUnspentDialog)
    {
        if (ConsolidateUnspentDialog->objectName().isEmpty())
            ConsolidateUnspentDialog->setObjectName("ConsolidateUnspentDialog");
        ConsolidateUnspentDialog->resize(820, 515);
        buttonBox = new QDialogButtonBox(ConsolidateUnspentDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setGeometry(QRect(640, 440, 161, 61));
        buttonBox->setStandardButtons(QDialogButtonBox::Ok);
        addressTableWidgetLabel = new QLabel(ConsolidateUnspentDialog);
        addressTableWidgetLabel->setObjectName("addressTableWidgetLabel");
        addressTableWidgetLabel->setGeometry(QRect(20, 30, 171, 221));
        addressTableWidgetLabel->setWordWrap(true);
        outputLimitWarningLabel = new QLabel(ConsolidateUnspentDialog);
        outputLimitWarningLabel->setObjectName("outputLimitWarningLabel");
        outputLimitWarningLabel->setGeometry(QRect(210, 300, 591, 121));
        outputLimitWarningLabel->setWordWrap(true);
        addressTableWidget = new QTableWidget(ConsolidateUnspentDialog);
        if (addressTableWidget->columnCount() < 2)
            addressTableWidget->setColumnCount(2);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        addressTableWidget->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        addressTableWidget->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        addressTableWidget->setObjectName("addressTableWidget");
        addressTableWidget->setGeometry(QRect(210, 20, 591, 241));
        addressTableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        addressTableWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        addressTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        addressTableWidget->setAlternatingRowColors(true);
        addressTableWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        addressTableWidget->setColumnCount(2);
        addressTableWidget->horizontalHeader()->setMinimumSectionSize(90);
        addressTableWidget->horizontalHeader()->setStretchLastSection(true);
        addressTableWidget->verticalHeader()->setVisible(false);
        outputLimitWarningIconLabel = new QLabel(ConsolidateUnspentDialog);
        outputLimitWarningIconLabel->setObjectName("outputLimitWarningIconLabel");
        outputLimitWarningIconLabel->setGeometry(QRect(50, 310, 111, 101));
        outputLimitWarningIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/warning")));
        outputLimitWarningIconLabel->setScaledContents(true);

        retranslateUi(ConsolidateUnspentDialog);

        QMetaObject::connectSlotsByName(ConsolidateUnspentDialog);
    } // setupUi

    void retranslateUi(QDialog *ConsolidateUnspentDialog)
    {
        ConsolidateUnspentDialog->setWindowTitle(QCoreApplication::translate("ConsolidateUnspentDialog", "Consolidate Unspent Outputs (UTXOs)", nullptr));
        addressTableWidgetLabel->setText(QCoreApplication::translate("ConsolidateUnspentDialog", "Select Destination Address for Consolidation", nullptr));
        outputLimitWarningLabel->setText(QString());
        QTableWidgetItem *___qtablewidgetitem = addressTableWidget->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("ConsolidateUnspentDialog", "Label", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = addressTableWidget->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("ConsolidateUnspentDialog", "Address", nullptr));
        outputLimitWarningIconLabel->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class ConsolidateUnspentDialog: public Ui_ConsolidateUnspentDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONSOLIDATEUNSPENTDIALOG_H
