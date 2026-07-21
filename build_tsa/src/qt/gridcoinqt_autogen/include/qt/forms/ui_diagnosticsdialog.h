/********************************************************************************
** Form generated from reading UI file 'diagnosticsdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DIAGNOSTICSDIALOG_H
#define UI_DIAGNOSTICSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_DiagnosticsDialog
{
public:
    QVBoxLayout *verticalLayout_2;
    QGridLayout *gridLayout;
    QLabel *checkETTSLabel;
    QLabel *verifyTCPPortLabel;
    QLabel *overallResultLabel;
    QLabel *checkConnectionCountLabel;
    QLabel *checkETTSResultLabel;
    QLabel *verifyTCPPortResultLabel;
    QLabel *verifyWalletIsSyncedLabel;
    QLabel *verifyCPIDValidLabel;
    QLabel *overallResultResultLabel;
    QLabel *verifyClockResultLabel;
    QLabel *verifyCPIDIsActiveResultLabel;
    QLabel *verifyCPIDHasRACResultLabel;
    QLabel *verifyCPIDIsActiveLabel;
    QLabel *verifyClockLabel;
    QLabel *verifyBoincPathLabel;
    QLabel *verifyWalletIsSyncedResultLabel;
    QLabel *checkOutboundConnectionCountResultLabel;
    QLabel *verifyBoincPathResultLabel;
    QLabel *diagnosticsLabel;
    QLabel *checkDifficultyResultLabel;
    QLabel *checkConnectionCountResultLabel;
    QLabel *checkOutboundConnectionCountLabel;
    QLabel *checkyDifficultyLabel;
    QLabel *verifyCPIDHasRACLabel;
    QLabel *verifyCPIDValidResultLabel;
    QLabel *checkClientVersionLabel;
    QLabel *checkClientVersionResultLabel;
    QHBoxLayout *horizontalLayout;
    QPushButton *closeButton;
    QPushButton *testButton;

    void setupUi(QDialog *DiagnosticsDialog)
    {
        if (DiagnosticsDialog->objectName().isEmpty())
            DiagnosticsDialog->setObjectName("DiagnosticsDialog");
        DiagnosticsDialog->resize(820, 660);
        verticalLayout_2 = new QVBoxLayout(DiagnosticsDialog);
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalLayout_2->setContentsMargins(20, 20, 20, 20);
        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        gridLayout->setSizeConstraint(QLayout::SetMinimumSize);
        gridLayout->setHorizontalSpacing(0);
        gridLayout->setVerticalSpacing(8);
        gridLayout->setContentsMargins(0, 0, 0, 0);
        checkETTSLabel = new QLabel(DiagnosticsDialog);
        checkETTSLabel->setObjectName("checkETTSLabel");

        gridLayout->addWidget(checkETTSLabel, 15, 0, 1, 1);

        verifyTCPPortLabel = new QLabel(DiagnosticsDialog);
        verifyTCPPortLabel->setObjectName("verifyTCPPortLabel");

        gridLayout->addWidget(verifyTCPPortLabel, 7, 0, 1, 1);

        overallResultLabel = new QLabel(DiagnosticsDialog);
        overallResultLabel->setObjectName("overallResultLabel");
        QFont font;
        font.setBold(true);
        overallResultLabel->setFont(font);

        gridLayout->addWidget(overallResultLabel, 28, 0, 1, 1);

        checkConnectionCountLabel = new QLabel(DiagnosticsDialog);
        checkConnectionCountLabel->setObjectName("checkConnectionCountLabel");

        gridLayout->addWidget(checkConnectionCountLabel, 3, 0, 1, 1);

        checkETTSResultLabel = new QLabel(DiagnosticsDialog);
        checkETTSResultLabel->setObjectName("checkETTSResultLabel");
        QSizePolicy sizePolicy(QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(checkETTSResultLabel->sizePolicy().hasHeightForWidth());
        checkETTSResultLabel->setSizePolicy(sizePolicy);
        checkETTSResultLabel->setMinimumSize(QSize(200, 0));
        checkETTSResultLabel->setWordWrap(true);
        checkETTSResultLabel->setIndent(10);

        gridLayout->addWidget(checkETTSResultLabel, 15, 1, 1, 1);

        verifyTCPPortResultLabel = new QLabel(DiagnosticsDialog);
        verifyTCPPortResultLabel->setObjectName("verifyTCPPortResultLabel");
        sizePolicy.setHeightForWidth(verifyTCPPortResultLabel->sizePolicy().hasHeightForWidth());
        verifyTCPPortResultLabel->setSizePolicy(sizePolicy);
        verifyTCPPortResultLabel->setMinimumSize(QSize(200, 0));
        verifyTCPPortResultLabel->setWordWrap(true);
        verifyTCPPortResultLabel->setIndent(10);

        gridLayout->addWidget(verifyTCPPortResultLabel, 7, 1, 1, 1);

        verifyWalletIsSyncedLabel = new QLabel(DiagnosticsDialog);
        verifyWalletIsSyncedLabel->setObjectName("verifyWalletIsSyncedLabel");

        gridLayout->addWidget(verifyWalletIsSyncedLabel, 1, 0, 1, 1);

        verifyCPIDValidLabel = new QLabel(DiagnosticsDialog);
        verifyCPIDValidLabel->setObjectName("verifyCPIDValidLabel");

        gridLayout->addWidget(verifyCPIDValidLabel, 12, 0, 1, 1);

        overallResultResultLabel = new QLabel(DiagnosticsDialog);
        overallResultResultLabel->setObjectName("overallResultResultLabel");
        overallResultResultLabel->setFont(font);
        overallResultResultLabel->setWordWrap(true);
        overallResultResultLabel->setIndent(10);

        gridLayout->addWidget(overallResultResultLabel, 28, 1, 1, 1);

        verifyClockResultLabel = new QLabel(DiagnosticsDialog);
        verifyClockResultLabel->setObjectName("verifyClockResultLabel");
        sizePolicy.setHeightForWidth(verifyClockResultLabel->sizePolicy().hasHeightForWidth());
        verifyClockResultLabel->setSizePolicy(sizePolicy);
        verifyClockResultLabel->setMinimumSize(QSize(200, 0));
        verifyClockResultLabel->setWordWrap(true);
        verifyClockResultLabel->setIndent(10);

        gridLayout->addWidget(verifyClockResultLabel, 5, 1, 1, 1);

        verifyCPIDIsActiveResultLabel = new QLabel(DiagnosticsDialog);
        verifyCPIDIsActiveResultLabel->setObjectName("verifyCPIDIsActiveResultLabel");
        sizePolicy.setHeightForWidth(verifyCPIDIsActiveResultLabel->sizePolicy().hasHeightForWidth());
        verifyCPIDIsActiveResultLabel->setSizePolicy(sizePolicy);
        verifyCPIDIsActiveResultLabel->setMinimumSize(QSize(200, 0));
        verifyCPIDIsActiveResultLabel->setWordWrap(true);
        verifyCPIDIsActiveResultLabel->setIndent(10);

        gridLayout->addWidget(verifyCPIDIsActiveResultLabel, 14, 1, 1, 1);

        verifyCPIDHasRACResultLabel = new QLabel(DiagnosticsDialog);
        verifyCPIDHasRACResultLabel->setObjectName("verifyCPIDHasRACResultLabel");
        sizePolicy.setHeightForWidth(verifyCPIDHasRACResultLabel->sizePolicy().hasHeightForWidth());
        verifyCPIDHasRACResultLabel->setSizePolicy(sizePolicy);
        verifyCPIDHasRACResultLabel->setMinimumSize(QSize(200, 0));
        verifyCPIDHasRACResultLabel->setWordWrap(true);
        verifyCPIDHasRACResultLabel->setIndent(10);

        gridLayout->addWidget(verifyCPIDHasRACResultLabel, 13, 1, 1, 1);

        verifyCPIDIsActiveLabel = new QLabel(DiagnosticsDialog);
        verifyCPIDIsActiveLabel->setObjectName("verifyCPIDIsActiveLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(verifyCPIDIsActiveLabel->sizePolicy().hasHeightForWidth());
        verifyCPIDIsActiveLabel->setSizePolicy(sizePolicy1);
        verifyCPIDIsActiveLabel->setMinimumSize(QSize(0, 0));

        gridLayout->addWidget(verifyCPIDIsActiveLabel, 14, 0, 1, 1);

        verifyClockLabel = new QLabel(DiagnosticsDialog);
        verifyClockLabel->setObjectName("verifyClockLabel");

        gridLayout->addWidget(verifyClockLabel, 5, 0, 1, 1);

        verifyBoincPathLabel = new QLabel(DiagnosticsDialog);
        verifyBoincPathLabel->setObjectName("verifyBoincPathLabel");

        gridLayout->addWidget(verifyBoincPathLabel, 11, 0, 1, 1);

        verifyWalletIsSyncedResultLabel = new QLabel(DiagnosticsDialog);
        verifyWalletIsSyncedResultLabel->setObjectName("verifyWalletIsSyncedResultLabel");
        sizePolicy.setHeightForWidth(verifyWalletIsSyncedResultLabel->sizePolicy().hasHeightForWidth());
        verifyWalletIsSyncedResultLabel->setSizePolicy(sizePolicy);
        verifyWalletIsSyncedResultLabel->setMinimumSize(QSize(200, 0));
        verifyWalletIsSyncedResultLabel->setWordWrap(true);
        verifyWalletIsSyncedResultLabel->setIndent(10);

        gridLayout->addWidget(verifyWalletIsSyncedResultLabel, 1, 1, 1, 1);

        checkOutboundConnectionCountResultLabel = new QLabel(DiagnosticsDialog);
        checkOutboundConnectionCountResultLabel->setObjectName("checkOutboundConnectionCountResultLabel");
        sizePolicy.setHeightForWidth(checkOutboundConnectionCountResultLabel->sizePolicy().hasHeightForWidth());
        checkOutboundConnectionCountResultLabel->setSizePolicy(sizePolicy);
        checkOutboundConnectionCountResultLabel->setMinimumSize(QSize(200, 0));
        checkOutboundConnectionCountResultLabel->setWordWrap(true);
        checkOutboundConnectionCountResultLabel->setIndent(10);

        gridLayout->addWidget(checkOutboundConnectionCountResultLabel, 4, 1, 1, 1);

        verifyBoincPathResultLabel = new QLabel(DiagnosticsDialog);
        verifyBoincPathResultLabel->setObjectName("verifyBoincPathResultLabel");
        sizePolicy.setHeightForWidth(verifyBoincPathResultLabel->sizePolicy().hasHeightForWidth());
        verifyBoincPathResultLabel->setSizePolicy(sizePolicy);
        verifyBoincPathResultLabel->setMinimumSize(QSize(200, 0));
        verifyBoincPathResultLabel->setWordWrap(true);
        verifyBoincPathResultLabel->setMargin(0);
        verifyBoincPathResultLabel->setIndent(10);

        gridLayout->addWidget(verifyBoincPathResultLabel, 11, 1, 1, 1);

        diagnosticsLabel = new QLabel(DiagnosticsDialog);
        diagnosticsLabel->setObjectName("diagnosticsLabel");
        diagnosticsLabel->setFont(font);

        gridLayout->addWidget(diagnosticsLabel, 0, 0, 1, 1);

        checkDifficultyResultLabel = new QLabel(DiagnosticsDialog);
        checkDifficultyResultLabel->setObjectName("checkDifficultyResultLabel");
        sizePolicy.setHeightForWidth(checkDifficultyResultLabel->sizePolicy().hasHeightForWidth());
        checkDifficultyResultLabel->setSizePolicy(sizePolicy);
        checkDifficultyResultLabel->setMinimumSize(QSize(200, 0));
        checkDifficultyResultLabel->setWordWrap(true);
        checkDifficultyResultLabel->setIndent(10);

        gridLayout->addWidget(checkDifficultyResultLabel, 8, 1, 1, 1);

        checkConnectionCountResultLabel = new QLabel(DiagnosticsDialog);
        checkConnectionCountResultLabel->setObjectName("checkConnectionCountResultLabel");
        sizePolicy.setHeightForWidth(checkConnectionCountResultLabel->sizePolicy().hasHeightForWidth());
        checkConnectionCountResultLabel->setSizePolicy(sizePolicy);
        checkConnectionCountResultLabel->setMinimumSize(QSize(200, 0));
        checkConnectionCountResultLabel->setWordWrap(true);
        checkConnectionCountResultLabel->setIndent(10);

        gridLayout->addWidget(checkConnectionCountResultLabel, 3, 1, 1, 1);

        checkOutboundConnectionCountLabel = new QLabel(DiagnosticsDialog);
        checkOutboundConnectionCountLabel->setObjectName("checkOutboundConnectionCountLabel");

        gridLayout->addWidget(checkOutboundConnectionCountLabel, 4, 0, 1, 1);

        checkyDifficultyLabel = new QLabel(DiagnosticsDialog);
        checkyDifficultyLabel->setObjectName("checkyDifficultyLabel");

        gridLayout->addWidget(checkyDifficultyLabel, 8, 0, 1, 1);

        verifyCPIDHasRACLabel = new QLabel(DiagnosticsDialog);
        verifyCPIDHasRACLabel->setObjectName("verifyCPIDHasRACLabel");

        gridLayout->addWidget(verifyCPIDHasRACLabel, 13, 0, 1, 1);

        verifyCPIDValidResultLabel = new QLabel(DiagnosticsDialog);
        verifyCPIDValidResultLabel->setObjectName("verifyCPIDValidResultLabel");
        sizePolicy.setHeightForWidth(verifyCPIDValidResultLabel->sizePolicy().hasHeightForWidth());
        verifyCPIDValidResultLabel->setSizePolicy(sizePolicy);
        verifyCPIDValidResultLabel->setMinimumSize(QSize(200, 0));
        verifyCPIDValidResultLabel->setWordWrap(true);
        verifyCPIDValidResultLabel->setMargin(0);
        verifyCPIDValidResultLabel->setIndent(10);

        gridLayout->addWidget(verifyCPIDValidResultLabel, 12, 1, 1, 1);

        checkClientVersionLabel = new QLabel(DiagnosticsDialog);
        checkClientVersionLabel->setObjectName("checkClientVersionLabel");

        gridLayout->addWidget(checkClientVersionLabel, 9, 0, 1, 1);

        checkClientVersionResultLabel = new QLabel(DiagnosticsDialog);
        checkClientVersionResultLabel->setObjectName("checkClientVersionResultLabel");
        sizePolicy.setHeightForWidth(checkClientVersionResultLabel->sizePolicy().hasHeightForWidth());
        checkClientVersionResultLabel->setSizePolicy(sizePolicy);
        checkClientVersionResultLabel->setMinimumSize(QSize(200, 0));
        checkClientVersionResultLabel->setWordWrap(true);
        checkClientVersionResultLabel->setIndent(10);

        gridLayout->addWidget(checkClientVersionResultLabel, 9, 1, 1, 1);


        verticalLayout_2->addLayout(gridLayout);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(610, -1, 0, 0);
        closeButton = new QPushButton(DiagnosticsDialog);
        closeButton->setObjectName("closeButton");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(closeButton->sizePolicy().hasHeightForWidth());
        closeButton->setSizePolicy(sizePolicy2);

        horizontalLayout->addWidget(closeButton);

        testButton = new QPushButton(DiagnosticsDialog);
        testButton->setObjectName("testButton");
        sizePolicy2.setHeightForWidth(testButton->sizePolicy().hasHeightForWidth());
        testButton->setSizePolicy(sizePolicy2);

        horizontalLayout->addWidget(testButton);


        verticalLayout_2->addLayout(horizontalLayout);


        retranslateUi(DiagnosticsDialog);
        QObject::connect(closeButton, &QPushButton::clicked, DiagnosticsDialog, qOverload<>(&QDialog::close));

        closeButton->setDefault(false);


        QMetaObject::connectSlotsByName(DiagnosticsDialog);
    } // setupUi

    void retranslateUi(QDialog *DiagnosticsDialog)
    {
        DiagnosticsDialog->setWindowTitle(QCoreApplication::translate("DiagnosticsDialog", "Diagnostics", nullptr));
        checkETTSLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Check estimated time to stake ", nullptr));
        verifyTCPPortLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Verify outbound port works", nullptr));
        overallResultLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Overall Result", nullptr));
        checkConnectionCountLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Check total connections", nullptr));
        checkETTSResultLabel->setText(QString());
        verifyTCPPortResultLabel->setText(QString());
        verifyWalletIsSyncedLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Verify wallet is synced", nullptr));
        verifyCPIDValidLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Verify CPID is valid", nullptr));
        overallResultResultLabel->setText(QString());
        verifyClockResultLabel->setText(QString());
        verifyCPIDIsActiveResultLabel->setText(QString());
        verifyCPIDHasRACResultLabel->setText(QString());
        verifyCPIDIsActiveLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Verify CPID has active beacon", nullptr));
        verifyClockLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Verify clock", nullptr));
        verifyBoincPathLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Verify BOINC path", nullptr));
        verifyWalletIsSyncedResultLabel->setText(QString());
        checkOutboundConnectionCountResultLabel->setText(QString());
        verifyBoincPathResultLabel->setText(QString());
        diagnosticsLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Diagnostics", nullptr));
        checkDifficultyResultLabel->setText(QString());
        checkConnectionCountResultLabel->setText(QString());
        checkOutboundConnectionCountLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Check outbound connections", nullptr));
        checkyDifficultyLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Check difficulty", nullptr));
        verifyCPIDHasRACLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Verify CPID has RAC", nullptr));
        verifyCPIDValidResultLabel->setText(QString());
        checkClientVersionLabel->setText(QCoreApplication::translate("DiagnosticsDialog", "Check client version", nullptr));
        checkClientVersionResultLabel->setText(QString());
        closeButton->setText(QCoreApplication::translate("DiagnosticsDialog", "Close", nullptr));
        testButton->setText(QCoreApplication::translate("DiagnosticsDialog", "Test", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DiagnosticsDialog: public Ui_DiagnosticsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DIAGNOSTICSDIALOG_H
