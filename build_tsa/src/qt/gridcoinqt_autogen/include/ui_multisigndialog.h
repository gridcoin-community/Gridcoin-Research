/********************************************************************************
** Form generated from reading UI file 'multisigndialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MULTISIGNDIALOG_H
#define UI_MULTISIGNDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_MultisignPSGTDialog
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *infoLabel;
    QLabel *psgtInLabel;
    QPlainTextEdit *psgtInEdit;
    QHBoxLayout *buttonRow;
    QPushButton *inspectButton;
    QPushButton *signButton;
    QPushButton *submitToPoolButton;
    QPushButton *finalizeButton;
    QPushButton *clearButton;
    QSpacerItem *buttonSpacer;
    QLabel *statusLabel;
    QLabel *decodedLabel;
    QPlainTextEdit *decodedView;
    QLabel *combineLabel;
    QHBoxLayout *combineRow;
    QPlainTextEdit *combineInEdit;
    QPushButton *combineButton;
    QLabel *resultLabel;
    QHBoxLayout *resultRow;
    QPlainTextEdit *resultOutEdit;
    QPushButton *copyResultButton;

    void setupUi(QDialog *MultisignPSGTDialog)
    {
        if (MultisignPSGTDialog->objectName().isEmpty())
            MultisignPSGTDialog->setObjectName("MultisignPSGTDialog");
        MultisignPSGTDialog->resize(720, 620);
        MultisignPSGTDialog->setModal(false);
        verticalLayout = new QVBoxLayout(MultisignPSGTDialog);
        verticalLayout->setObjectName("verticalLayout");
        infoLabel = new QLabel(MultisignPSGTDialog);
        infoLabel->setObjectName("infoLabel");
        infoLabel->setTextFormat(Qt::PlainText);
        infoLabel->setWordWrap(true);

        verticalLayout->addWidget(infoLabel);

        psgtInLabel = new QLabel(MultisignPSGTDialog);
        psgtInLabel->setObjectName("psgtInLabel");

        verticalLayout->addWidget(psgtInLabel);

        psgtInEdit = new QPlainTextEdit(MultisignPSGTDialog);
        psgtInEdit->setObjectName("psgtInEdit");

        verticalLayout->addWidget(psgtInEdit);

        buttonRow = new QHBoxLayout();
        buttonRow->setObjectName("buttonRow");
        inspectButton = new QPushButton(MultisignPSGTDialog);
        inspectButton->setObjectName("inspectButton");
        inspectButton->setAutoDefault(false);

        buttonRow->addWidget(inspectButton);

        signButton = new QPushButton(MultisignPSGTDialog);
        signButton->setObjectName("signButton");
        signButton->setAutoDefault(false);

        buttonRow->addWidget(signButton);

        submitToPoolButton = new QPushButton(MultisignPSGTDialog);
        submitToPoolButton->setObjectName("submitToPoolButton");
        submitToPoolButton->setAutoDefault(false);

        buttonRow->addWidget(submitToPoolButton);

        finalizeButton = new QPushButton(MultisignPSGTDialog);
        finalizeButton->setObjectName("finalizeButton");
        finalizeButton->setAutoDefault(false);

        buttonRow->addWidget(finalizeButton);

        clearButton = new QPushButton(MultisignPSGTDialog);
        clearButton->setObjectName("clearButton");
        clearButton->setAutoDefault(false);

        buttonRow->addWidget(clearButton);

        buttonSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buttonRow->addItem(buttonSpacer);


        verticalLayout->addLayout(buttonRow);

        statusLabel = new QLabel(MultisignPSGTDialog);
        statusLabel->setObjectName("statusLabel");
        statusLabel->setWordWrap(true);

        verticalLayout->addWidget(statusLabel);

        decodedLabel = new QLabel(MultisignPSGTDialog);
        decodedLabel->setObjectName("decodedLabel");

        verticalLayout->addWidget(decodedLabel);

        decodedView = new QPlainTextEdit(MultisignPSGTDialog);
        decodedView->setObjectName("decodedView");
        decodedView->setReadOnly(true);

        verticalLayout->addWidget(decodedView);

        combineLabel = new QLabel(MultisignPSGTDialog);
        combineLabel->setObjectName("combineLabel");

        verticalLayout->addWidget(combineLabel);

        combineRow = new QHBoxLayout();
        combineRow->setObjectName("combineRow");
        combineInEdit = new QPlainTextEdit(MultisignPSGTDialog);
        combineInEdit->setObjectName("combineInEdit");
        combineInEdit->setMaximumHeight(80);

        combineRow->addWidget(combineInEdit);

        combineButton = new QPushButton(MultisignPSGTDialog);
        combineButton->setObjectName("combineButton");
        combineButton->setAutoDefault(false);

        combineRow->addWidget(combineButton);


        verticalLayout->addLayout(combineRow);

        resultLabel = new QLabel(MultisignPSGTDialog);
        resultLabel->setObjectName("resultLabel");

        verticalLayout->addWidget(resultLabel);

        resultRow = new QHBoxLayout();
        resultRow->setObjectName("resultRow");
        resultOutEdit = new QPlainTextEdit(MultisignPSGTDialog);
        resultOutEdit->setObjectName("resultOutEdit");
        resultOutEdit->setReadOnly(true);
        resultOutEdit->setMaximumHeight(90);

        resultRow->addWidget(resultOutEdit);

        copyResultButton = new QPushButton(MultisignPSGTDialog);
        copyResultButton->setObjectName("copyResultButton");
        copyResultButton->setAutoDefault(false);

        resultRow->addWidget(copyResultButton);


        verticalLayout->addLayout(resultRow);


        retranslateUi(MultisignPSGTDialog);

        QMetaObject::connectSlotsByName(MultisignPSGTDialog);
    } // setupUi

    void retranslateUi(QDialog *MultisignPSGTDialog)
    {
        MultisignPSGTDialog->setWindowTitle(QCoreApplication::translate("MultisignPSGTDialog", "Multisign (PSGT)", nullptr));
        infoLabel->setText(QCoreApplication::translate("MultisignPSGTDialog", "Load a Partially Signed Gridcoin Transaction (PSGT), inspect it, sign your inputs with this wallet, combine signatures from co-signers, and finalize it to a broadcast-ready raw transaction. Paste the base64 PSGT below.", nullptr));
        psgtInLabel->setText(QCoreApplication::translate("MultisignPSGTDialog", "PSGT (base64):", nullptr));
#if QT_CONFIG(tooltip)
        psgtInEdit->setToolTip(QCoreApplication::translate("MultisignPSGTDialog", "Paste the base64-encoded PSGT here. Operations below update this working PSGT in place.", nullptr));
#endif // QT_CONFIG(tooltip)
        psgtInEdit->setPlaceholderText(QCoreApplication::translate("MultisignPSGTDialog", "Paste a base64 PSGT (e.g. from createpsgt)", nullptr));
#if QT_CONFIG(tooltip)
        inspectButton->setToolTip(QCoreApplication::translate("MultisignPSGTDialog", "Decode the PSGT and show its inputs, outputs and signing status", nullptr));
#endif // QT_CONFIG(tooltip)
        inspectButton->setText(QCoreApplication::translate("MultisignPSGTDialog", "&Inspect", nullptr));
#if QT_CONFIG(tooltip)
        signButton->setToolTip(QCoreApplication::translate("MultisignPSGTDialog", "Sign every input this wallet has keys for, then update the working PSGT", nullptr));
#endif // QT_CONFIG(tooltip)
        signButton->setText(QCoreApplication::translate("MultisignPSGTDialog", "&Sign with wallet", nullptr));
#if QT_CONFIG(tooltip)
        submitToPoolButton->setToolTip(QCoreApplication::translate("MultisignPSGTDialog", "Relay this PSGT to co-signers through the network PSGT pool (requires v15 and at least one of your signatures)", nullptr));
#endif // QT_CONFIG(tooltip)
        submitToPoolButton->setText(QCoreApplication::translate("MultisignPSGTDialog", "Submit to &pool", nullptr));
#if QT_CONFIG(tooltip)
        finalizeButton->setToolTip(QCoreApplication::translate("MultisignPSGTDialog", "If fully signed, extract the completed raw transaction (hex) for broadcast", nullptr));
#endif // QT_CONFIG(tooltip)
        finalizeButton->setText(QCoreApplication::translate("MultisignPSGTDialog", "&Finalize \342\206\222 hex", nullptr));
#if QT_CONFIG(tooltip)
        clearButton->setToolTip(QCoreApplication::translate("MultisignPSGTDialog", "Clear all fields", nullptr));
#endif // QT_CONFIG(tooltip)
        clearButton->setText(QCoreApplication::translate("MultisignPSGTDialog", "Clear &All", nullptr));
        statusLabel->setText(QString());
        decodedLabel->setText(QCoreApplication::translate("MultisignPSGTDialog", "Decoded:", nullptr));
        decodedView->setPlaceholderText(QCoreApplication::translate("MultisignPSGTDialog", "Click \"Inspect\" to decode the PSGT", nullptr));
        combineLabel->setText(QCoreApplication::translate("MultisignPSGTDialog", "Out-of-band: co-signers' PSGTs to combine (one base64 PSGT per line):", nullptr));
#if QT_CONFIG(tooltip)
        combineInEdit->setToolTip(QCoreApplication::translate("MultisignPSGTDialog", "Out-of-band combine: paste one or more co-signers' PSGTs (one per line). Combine merges them with the working PSGT above. This is the manual path, distinct from the future network PSGT pool.", nullptr));
#endif // QT_CONFIG(tooltip)
        combineInEdit->setPlaceholderText(QCoreApplication::translate("MultisignPSGTDialog", "One base64 PSGT per line", nullptr));
#if QT_CONFIG(tooltip)
        combineButton->setToolTip(QCoreApplication::translate("MultisignPSGTDialog", "Merge the co-signers' PSGTs into the working PSGT (out-of-band)", nullptr));
#endif // QT_CONFIG(tooltip)
        combineButton->setText(QCoreApplication::translate("MultisignPSGTDialog", "&Combine", nullptr));
        resultLabel->setText(QCoreApplication::translate("MultisignPSGTDialog", "Result (signed PSGT base64, or finalized transaction hex):", nullptr));
#if QT_CONFIG(tooltip)
        copyResultButton->setToolTip(QCoreApplication::translate("MultisignPSGTDialog", "Copy the result to the clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        copyResultButton->setText(QCoreApplication::translate("MultisignPSGTDialog", "Cop&y", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MultisignPSGTDialog: public Ui_MultisignPSGTDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MULTISIGNDIALOG_H
