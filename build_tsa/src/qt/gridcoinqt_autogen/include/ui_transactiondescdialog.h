/********************************************************************************
** Form generated from reading UI file 'transactiondescdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TRANSACTIONDESCDIALOG_H
#define UI_TRANSACTIONDESCDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_TransactionDescDialog
{
public:
    QVBoxLayout *verticalLayout;
    QTextEdit *detailText;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QPushButton *closeButton;

    void setupUi(QDialog *TransactionDescDialog)
    {
        if (TransactionDescDialog->objectName().isEmpty())
            TransactionDescDialog->setObjectName("TransactionDescDialog");
        TransactionDescDialog->resize(770, 400);
        TransactionDescDialog->setMaximumSize(QSize(16777215, 16777215));
        TransactionDescDialog->setLayoutDirection(Qt::LeftToRight);
        verticalLayout = new QVBoxLayout(TransactionDescDialog);
        verticalLayout->setObjectName("verticalLayout");
        detailText = new QTextEdit(TransactionDescDialog);
        detailText->setObjectName("detailText");
        detailText->setReadOnly(true);

        verticalLayout->addWidget(detailText);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        closeButton = new QPushButton(TransactionDescDialog);
        closeButton->setObjectName("closeButton");

        horizontalLayout->addWidget(closeButton);


        verticalLayout->addLayout(horizontalLayout);


        retranslateUi(TransactionDescDialog);
        QObject::connect(closeButton, &QPushButton::clicked, TransactionDescDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(TransactionDescDialog);
    } // setupUi

    void retranslateUi(QDialog *TransactionDescDialog)
    {
        TransactionDescDialog->setWindowTitle(QCoreApplication::translate("TransactionDescDialog", "Transaction details", nullptr));
#if QT_CONFIG(tooltip)
        detailText->setToolTip(QCoreApplication::translate("TransactionDescDialog", "This pane shows a detailed description of the transaction", nullptr));
#endif // QT_CONFIG(tooltip)
        closeButton->setText(QCoreApplication::translate("TransactionDescDialog", "C&lose", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TransactionDescDialog: public Ui_TransactionDescDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TRANSACTIONDESCDIALOG_H
