/********************************************************************************
** Form generated from reading UI file 'signverifymessagedialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SIGNVERIFYMESSAGEDIALOG_H
#define UI_SIGNVERIFYMESSAGEDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "qvalidatedlineedit.h"

QT_BEGIN_NAMESPACE

class Ui_SignVerifyMessageDialog
{
public:
    QVBoxLayout *verticalLayout;
    QTabWidget *tabWidget;
    QWidget *tabSignMessage;
    QVBoxLayout *verticalLayout_SM;
    QLabel *infoLabel_SM;
    QHBoxLayout *horizontalLayout_1_SM;
    QValidatedLineEdit *addressInEdit_SM;
    QToolButton *addressBookButton_SM;
    QToolButton *pasteButton_SM;
    QPlainTextEdit *messageInEdit_SM;
    QHBoxLayout *horizontalLayout_2_SM;
    QLineEdit *signatureOutEdit_SM;
    QToolButton *copySignatureButton_SM;
    QHBoxLayout *horizontalLayout_3_SM;
    QPushButton *signMessageButton_SM;
    QPushButton *clearButton_SM;
    QSpacerItem *horizontalSpacer_1_SM;
    QLabel *statusLabel_SM;
    QSpacerItem *horizontalSpacer_2_SM;
    QWidget *tabVerifyMessage;
    QVBoxLayout *verticalLayout_VM;
    QLabel *infoLabel_VM;
    QHBoxLayout *horizontalLayout_1_VM;
    QValidatedLineEdit *addressInEdit_VM;
    QToolButton *addressBookButton_VM;
    QPlainTextEdit *messageInEdit_VM;
    QValidatedLineEdit *signatureInEdit_VM;
    QHBoxLayout *horizontalLayout_2_VM;
    QPushButton *verifyMessageButton_VM;
    QPushButton *clearButton_VM;
    QSpacerItem *horizontalSpacer_1_VM;
    QLabel *statusLabel_VM;
    QSpacerItem *horizontalSpacer_2_VM;

    void setupUi(QDialog *SignVerifyMessageDialog)
    {
        if (SignVerifyMessageDialog->objectName().isEmpty())
            SignVerifyMessageDialog->setObjectName("SignVerifyMessageDialog");
        SignVerifyMessageDialog->resize(700, 380);
        SignVerifyMessageDialog->setModal(true);
        verticalLayout = new QVBoxLayout(SignVerifyMessageDialog);
        verticalLayout->setObjectName("verticalLayout");
        tabWidget = new QTabWidget(SignVerifyMessageDialog);
        tabWidget->setObjectName("tabWidget");
        tabSignMessage = new QWidget();
        tabSignMessage->setObjectName("tabSignMessage");
        verticalLayout_SM = new QVBoxLayout(tabSignMessage);
        verticalLayout_SM->setObjectName("verticalLayout_SM");
        infoLabel_SM = new QLabel(tabSignMessage);
        infoLabel_SM->setObjectName("infoLabel_SM");
        infoLabel_SM->setTextFormat(Qt::PlainText);
        infoLabel_SM->setWordWrap(true);

        verticalLayout_SM->addWidget(infoLabel_SM);

        horizontalLayout_1_SM = new QHBoxLayout();
        horizontalLayout_1_SM->setSpacing(5);
        horizontalLayout_1_SM->setObjectName("horizontalLayout_1_SM");
        addressInEdit_SM = new QValidatedLineEdit(tabSignMessage);
        addressInEdit_SM->setObjectName("addressInEdit_SM");
        addressInEdit_SM->setMaxLength(34);

        horizontalLayout_1_SM->addWidget(addressInEdit_SM);

        addressBookButton_SM = new QToolButton(tabSignMessage);
        addressBookButton_SM->setObjectName("addressBookButton_SM");
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/address-book_light"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        addressBookButton_SM->setIcon(icon);

        horizontalLayout_1_SM->addWidget(addressBookButton_SM);

        pasteButton_SM = new QToolButton(tabSignMessage);
        pasteButton_SM->setObjectName("pasteButton_SM");
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/editpaste"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        pasteButton_SM->setIcon(icon1);

        horizontalLayout_1_SM->addWidget(pasteButton_SM);


        verticalLayout_SM->addLayout(horizontalLayout_1_SM);

        messageInEdit_SM = new QPlainTextEdit(tabSignMessage);
        messageInEdit_SM->setObjectName("messageInEdit_SM");

        verticalLayout_SM->addWidget(messageInEdit_SM);

        horizontalLayout_2_SM = new QHBoxLayout();
        horizontalLayout_2_SM->setSpacing(5);
        horizontalLayout_2_SM->setObjectName("horizontalLayout_2_SM");
        signatureOutEdit_SM = new QLineEdit(tabSignMessage);
        signatureOutEdit_SM->setObjectName("signatureOutEdit_SM");
        signatureOutEdit_SM->setReadOnly(true);

        horizontalLayout_2_SM->addWidget(signatureOutEdit_SM);

        copySignatureButton_SM = new QToolButton(tabSignMessage);
        copySignatureButton_SM->setObjectName("copySignatureButton_SM");
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/editcopy"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        copySignatureButton_SM->setIcon(icon2);

        horizontalLayout_2_SM->addWidget(copySignatureButton_SM);


        verticalLayout_SM->addLayout(horizontalLayout_2_SM);

        horizontalLayout_3_SM = new QHBoxLayout();
        horizontalLayout_3_SM->setObjectName("horizontalLayout_3_SM");
        signMessageButton_SM = new QPushButton(tabSignMessage);
        signMessageButton_SM->setObjectName("signMessageButton_SM");
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icons/edit"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        signMessageButton_SM->setIcon(icon3);
        signMessageButton_SM->setAutoDefault(false);

        horizontalLayout_3_SM->addWidget(signMessageButton_SM);

        clearButton_SM = new QPushButton(tabSignMessage);
        clearButton_SM->setObjectName("clearButton_SM");
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        clearButton_SM->setIcon(icon4);
        clearButton_SM->setAutoDefault(false);

        horizontalLayout_3_SM->addWidget(clearButton_SM);

        horizontalSpacer_1_SM = new QSpacerItem(40, 48, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3_SM->addItem(horizontalSpacer_1_SM);

        statusLabel_SM = new QLabel(tabSignMessage);
        statusLabel_SM->setObjectName("statusLabel_SM");
        statusLabel_SM->setWordWrap(true);

        horizontalLayout_3_SM->addWidget(statusLabel_SM);

        horizontalSpacer_2_SM = new QSpacerItem(40, 48, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3_SM->addItem(horizontalSpacer_2_SM);


        verticalLayout_SM->addLayout(horizontalLayout_3_SM);

        tabWidget->addTab(tabSignMessage, QString());
        tabVerifyMessage = new QWidget();
        tabVerifyMessage->setObjectName("tabVerifyMessage");
        verticalLayout_VM = new QVBoxLayout(tabVerifyMessage);
        verticalLayout_VM->setObjectName("verticalLayout_VM");
        infoLabel_VM = new QLabel(tabVerifyMessage);
        infoLabel_VM->setObjectName("infoLabel_VM");
        infoLabel_VM->setTextFormat(Qt::PlainText);
        infoLabel_VM->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        infoLabel_VM->setWordWrap(true);

        verticalLayout_VM->addWidget(infoLabel_VM);

        horizontalLayout_1_VM = new QHBoxLayout();
        horizontalLayout_1_VM->setSpacing(5);
        horizontalLayout_1_VM->setObjectName("horizontalLayout_1_VM");
        addressInEdit_VM = new QValidatedLineEdit(tabVerifyMessage);
        addressInEdit_VM->setObjectName("addressInEdit_VM");
        addressInEdit_VM->setMaxLength(34);

        horizontalLayout_1_VM->addWidget(addressInEdit_VM);

        addressBookButton_VM = new QToolButton(tabVerifyMessage);
        addressBookButton_VM->setObjectName("addressBookButton_VM");
        addressBookButton_VM->setIcon(icon);

        horizontalLayout_1_VM->addWidget(addressBookButton_VM);


        verticalLayout_VM->addLayout(horizontalLayout_1_VM);

        messageInEdit_VM = new QPlainTextEdit(tabVerifyMessage);
        messageInEdit_VM->setObjectName("messageInEdit_VM");

        verticalLayout_VM->addWidget(messageInEdit_VM);

        signatureInEdit_VM = new QValidatedLineEdit(tabVerifyMessage);
        signatureInEdit_VM->setObjectName("signatureInEdit_VM");

        verticalLayout_VM->addWidget(signatureInEdit_VM);

        horizontalLayout_2_VM = new QHBoxLayout();
        horizontalLayout_2_VM->setObjectName("horizontalLayout_2_VM");
        verifyMessageButton_VM = new QPushButton(tabVerifyMessage);
        verifyMessageButton_VM->setObjectName("verifyMessageButton_VM");
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/transaction_0"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        verifyMessageButton_VM->setIcon(icon5);
        verifyMessageButton_VM->setAutoDefault(false);

        horizontalLayout_2_VM->addWidget(verifyMessageButton_VM);

        clearButton_VM = new QPushButton(tabVerifyMessage);
        clearButton_VM->setObjectName("clearButton_VM");
        clearButton_VM->setIcon(icon4);
        clearButton_VM->setAutoDefault(false);

        horizontalLayout_2_VM->addWidget(clearButton_VM);

        horizontalSpacer_1_VM = new QSpacerItem(40, 48, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_2_VM->addItem(horizontalSpacer_1_VM);

        statusLabel_VM = new QLabel(tabVerifyMessage);
        statusLabel_VM->setObjectName("statusLabel_VM");
        statusLabel_VM->setWordWrap(true);

        horizontalLayout_2_VM->addWidget(statusLabel_VM);

        horizontalSpacer_2_VM = new QSpacerItem(40, 48, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_2_VM->addItem(horizontalSpacer_2_VM);


        verticalLayout_VM->addLayout(horizontalLayout_2_VM);

        tabWidget->addTab(tabVerifyMessage, QString());

        verticalLayout->addWidget(tabWidget);


        retranslateUi(SignVerifyMessageDialog);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(SignVerifyMessageDialog);
    } // setupUi

    void retranslateUi(QDialog *SignVerifyMessageDialog)
    {
        SignVerifyMessageDialog->setWindowTitle(QCoreApplication::translate("SignVerifyMessageDialog", "Signatures - Sign / Verify a Message", nullptr));
        infoLabel_SM->setText(QCoreApplication::translate("SignVerifyMessageDialog", "You can sign messages with your addresses to prove you own them. Be careful not to sign anything vague, as phishing attacks may try to trick you into signing your identity over to them. Only sign fully-detailed statements you agree to.", nullptr));
#if QT_CONFIG(tooltip)
        addressInEdit_SM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "The address to sign the message with (e.g. Sjz75uKHzUQJnSdzvpiigEGxseKkDhQToX)", nullptr));
#endif // QT_CONFIG(tooltip)
        addressInEdit_SM->setPlaceholderText(QCoreApplication::translate("SignVerifyMessageDialog", "Enter a Gridcoin address (e.g. S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB)", nullptr));
#if QT_CONFIG(tooltip)
        addressBookButton_SM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "Choose an address from the address book", nullptr));
#endif // QT_CONFIG(tooltip)
        addressBookButton_SM->setText(QString());
#if QT_CONFIG(shortcut)
        addressBookButton_SM->setShortcut(QCoreApplication::translate("SignVerifyMessageDialog", "Alt+A", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        pasteButton_SM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "Paste address from clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        pasteButton_SM->setText(QString());
#if QT_CONFIG(shortcut)
        pasteButton_SM->setShortcut(QCoreApplication::translate("SignVerifyMessageDialog", "Alt+P", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        messageInEdit_SM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "Enter the message you want to sign here", nullptr));
#endif // QT_CONFIG(tooltip)
        signatureOutEdit_SM->setPlaceholderText(QCoreApplication::translate("SignVerifyMessageDialog", "Click \"Sign Message\" to generate signature", nullptr));
#if QT_CONFIG(tooltip)
        copySignatureButton_SM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "Copy the current signature to the system clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        copySignatureButton_SM->setText(QString());
#if QT_CONFIG(tooltip)
        signMessageButton_SM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "Sign the message to prove you own this Gridcoin address", nullptr));
#endif // QT_CONFIG(tooltip)
        signMessageButton_SM->setText(QCoreApplication::translate("SignVerifyMessageDialog", "Sign &Message", nullptr));
#if QT_CONFIG(tooltip)
        clearButton_SM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "Reset all sign message fields", nullptr));
#endif // QT_CONFIG(tooltip)
        clearButton_SM->setText(QCoreApplication::translate("SignVerifyMessageDialog", "Clear &All", nullptr));
        statusLabel_SM->setText(QString());
        tabWidget->setTabText(tabWidget->indexOf(tabSignMessage), QCoreApplication::translate("SignVerifyMessageDialog", "&Sign Message", nullptr));
        infoLabel_VM->setText(QCoreApplication::translate("SignVerifyMessageDialog", "Enter the signing address, message (ensure you copy line breaks, spaces, tabs, etc. exactly) and signature below to verify the message. Be careful not to read more into the signature than what is in the signed message itself, to avoid being tricked by a man-in-the-middle attack.", nullptr));
#if QT_CONFIG(tooltip)
        addressInEdit_VM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "The address the message was signed with (e.g. Sjz75uKHzUQJnSdzvpiigEGxseKkDhQToX)", nullptr));
#endif // QT_CONFIG(tooltip)
        addressInEdit_VM->setPlaceholderText(QCoreApplication::translate("SignVerifyMessageDialog", "Enter a Gridcoin address (e.g. S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB)", nullptr));
#if QT_CONFIG(tooltip)
        addressBookButton_VM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "Choose an address from the address book", nullptr));
#endif // QT_CONFIG(tooltip)
        addressBookButton_VM->setText(QString());
#if QT_CONFIG(shortcut)
        addressBookButton_VM->setShortcut(QCoreApplication::translate("SignVerifyMessageDialog", "Alt+A", nullptr));
#endif // QT_CONFIG(shortcut)
        signatureInEdit_VM->setPlaceholderText(QCoreApplication::translate("SignVerifyMessageDialog", "Enter Gridcoin signature", nullptr));
#if QT_CONFIG(tooltip)
        verifyMessageButton_VM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "Verify the message to ensure it was signed with the specified Gridcoin address", nullptr));
#endif // QT_CONFIG(tooltip)
        verifyMessageButton_VM->setText(QCoreApplication::translate("SignVerifyMessageDialog", "&Verify Message", nullptr));
#if QT_CONFIG(tooltip)
        clearButton_VM->setToolTip(QCoreApplication::translate("SignVerifyMessageDialog", "Reset all verify message fields", nullptr));
#endif // QT_CONFIG(tooltip)
        clearButton_VM->setText(QCoreApplication::translate("SignVerifyMessageDialog", "Clear &All", nullptr));
        statusLabel_VM->setText(QString());
        tabWidget->setTabText(tabWidget->indexOf(tabVerifyMessage), QCoreApplication::translate("SignVerifyMessageDialog", "&Verify Message", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SignVerifyMessageDialog: public Ui_SignVerifyMessageDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SIGNVERIFYMESSAGEDIALOG_H
