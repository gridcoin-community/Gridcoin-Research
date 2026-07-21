/********************************************************************************
** Form generated from reading UI file 'sendcoinsentry.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SENDCOINSENTRY_H
#define UI_SENDCOINSENTRY_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include "bitcoinamountfield.h"
#include "qvalidatedlineedit.h"

QT_BEGIN_NAMESPACE

class Ui_SendCoinsEntry
{
public:
    QGridLayout *gridLayout;
    QHBoxLayout *horizontalLayout_2;
    QValidatedLineEdit *addAsLabel;
    QLabel *payToLabel;
    QHBoxLayout *payToLayout;
    QValidatedLineEdit *payTo;
    QToolButton *addressBookButton;
    QToolButton *pasteButton;
    QToolButton *deleteButton;
    QLabel *messageLabel;
    BitcoinAmountField *payAmount;
    QCheckBox *subtractFee;
    QLabel *labelTextLabel;
    QLabel *amountLabel;
    QHBoxLayout *horizontalLayout_7;
    QValidatedLineEdit *messageText;

    void setupUi(QFrame *SendCoinsEntry)
    {
        if (SendCoinsEntry->objectName().isEmpty())
            SendCoinsEntry->setObjectName("SendCoinsEntry");
        SendCoinsEntry->resize(731, 186);
        gridLayout = new QGridLayout(SendCoinsEntry);
        gridLayout->setSpacing(12);
        gridLayout->setObjectName("gridLayout");
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setSpacing(0);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        addAsLabel = new QValidatedLineEdit(SendCoinsEntry);
        addAsLabel->setObjectName("addAsLabel");
        addAsLabel->setEnabled(true);

        horizontalLayout_2->addWidget(addAsLabel);


        gridLayout->addLayout(horizontalLayout_2, 4, 1, 1, 1);

        payToLabel = new QLabel(SendCoinsEntry);
        payToLabel->setObjectName("payToLabel");
        payToLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(payToLabel, 3, 0, 1, 1);

        payToLayout = new QHBoxLayout();
        payToLayout->setSpacing(0);
        payToLayout->setObjectName("payToLayout");
        payTo = new QValidatedLineEdit(SendCoinsEntry);
        payTo->setObjectName("payTo");
        payTo->setMaxLength(34);

        payToLayout->addWidget(payTo);

        addressBookButton = new QToolButton(SendCoinsEntry);
        addressBookButton->setObjectName("addressBookButton");

        payToLayout->addWidget(addressBookButton);

        pasteButton = new QToolButton(SendCoinsEntry);
        pasteButton->setObjectName("pasteButton");
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/editpaste"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        pasteButton->setIcon(icon);

        payToLayout->addWidget(pasteButton);

        deleteButton = new QToolButton(SendCoinsEntry);
        deleteButton->setObjectName("deleteButton");
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        deleteButton->setIcon(icon1);

        payToLayout->addWidget(deleteButton);


        gridLayout->addLayout(payToLayout, 3, 1, 1, 1);

        messageLabel = new QLabel(SendCoinsEntry);
        messageLabel->setObjectName("messageLabel");
        messageLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(messageLabel, 5, 0, 1, 1);

        payAmount = new BitcoinAmountField(SendCoinsEntry);
        payAmount->setObjectName("payAmount");

        gridLayout->addWidget(payAmount, 6, 1, 1, 1);

        subtractFee = new QCheckBox(SendCoinsEntry);
        subtractFee->setObjectName("subtractFee");

        gridLayout->addWidget(subtractFee, 7, 1, 1, 1);

        labelTextLabel = new QLabel(SendCoinsEntry);
        labelTextLabel->setObjectName("labelTextLabel");
        labelTextLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(labelTextLabel, 4, 0, 1, 1);

        amountLabel = new QLabel(SendCoinsEntry);
        amountLabel->setObjectName("amountLabel");
        amountLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(amountLabel, 6, 0, 1, 1);

        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setSpacing(0);
        horizontalLayout_7->setObjectName("horizontalLayout_7");
        messageText = new QValidatedLineEdit(SendCoinsEntry);
        messageText->setObjectName("messageText");
        messageText->setEnabled(true);

        horizontalLayout_7->addWidget(messageText);


        gridLayout->addLayout(horizontalLayout_7, 5, 1, 1, 1);

#if QT_CONFIG(shortcut)
        payToLabel->setBuddy(payTo);
        messageLabel->setBuddy(messageText);
        labelTextLabel->setBuddy(addAsLabel);
        amountLabel->setBuddy(payAmount);
#endif // QT_CONFIG(shortcut)

        retranslateUi(SendCoinsEntry);

        QMetaObject::connectSlotsByName(SendCoinsEntry);
    } // setupUi

    void retranslateUi(QFrame *SendCoinsEntry)
    {
        SendCoinsEntry->setWindowTitle(QCoreApplication::translate("SendCoinsEntry", "Form", nullptr));
#if QT_CONFIG(tooltip)
        addAsLabel->setToolTip(QCoreApplication::translate("SendCoinsEntry", "Enter a label for this address to add it to your address book", nullptr));
#endif // QT_CONFIG(tooltip)
        addAsLabel->setPlaceholderText(QCoreApplication::translate("SendCoinsEntry", "Enter a label for this address to add it to your address book", nullptr));
        payToLabel->setText(QCoreApplication::translate("SendCoinsEntry", "Pay &To:", nullptr));
#if QT_CONFIG(tooltip)
        payTo->setToolTip(QCoreApplication::translate("SendCoinsEntry", "The address to send the payment to  (e.g. Sjz75uKHzUQJnSdzvpiigEGxseKkDhQToX)", nullptr));
#endif // QT_CONFIG(tooltip)
        payTo->setPlaceholderText(QCoreApplication::translate("SendCoinsEntry", "Enter a Gridcoin address (e.g. S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB)", nullptr));
#if QT_CONFIG(tooltip)
        addressBookButton->setToolTip(QCoreApplication::translate("SendCoinsEntry", "Choose address from address book", nullptr));
#endif // QT_CONFIG(tooltip)
        addressBookButton->setText(QString());
#if QT_CONFIG(shortcut)
        addressBookButton->setShortcut(QCoreApplication::translate("SendCoinsEntry", "Alt+A", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        pasteButton->setToolTip(QCoreApplication::translate("SendCoinsEntry", "Paste address from clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        pasteButton->setText(QString());
#if QT_CONFIG(shortcut)
        pasteButton->setShortcut(QCoreApplication::translate("SendCoinsEntry", "Alt+P", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        deleteButton->setToolTip(QCoreApplication::translate("SendCoinsEntry", "Remove this recipient", nullptr));
#endif // QT_CONFIG(tooltip)
        deleteButton->setText(QString());
        messageLabel->setText(QCoreApplication::translate("SendCoinsEntry", "Message:", nullptr));
#if QT_CONFIG(tooltip)
        subtractFee->setToolTip(QCoreApplication::translate("SendCoinsEntry", "The transaction fee will be deducted from the amount sent", nullptr));
#endif // QT_CONFIG(tooltip)
        subtractFee->setText(QCoreApplication::translate("SendCoinsEntry", "Subtract fee from amount", nullptr));
        labelTextLabel->setText(QCoreApplication::translate("SendCoinsEntry", "&Label:", nullptr));
        amountLabel->setText(QCoreApplication::translate("SendCoinsEntry", "A&mount:", nullptr));
#if QT_CONFIG(tooltip)
        messageText->setToolTip(QCoreApplication::translate("SendCoinsEntry", "Send Custom Message to a Gridcoin Recipient", nullptr));
#endif // QT_CONFIG(tooltip)
    } // retranslateUi

};

namespace Ui {
    class SendCoinsEntry: public Ui_SendCoinsEntry {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SENDCOINSENTRY_H
