/********************************************************************************
** Form generated from reading UI file 'askpassphrasedialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ASKPASSPHRASEDIALOG_H
#define UI_ASKPASSPHRASEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_AskPassphraseDialog
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *warningLabel;
    QFormLayout *formLayout;
    QLabel *oldPassphraseLabel;
    QLineEdit *oldPassphraseEdit;
    QLabel *newPassphraseLabel;
    QLineEdit *newPassphraseEdit;
    QLabel *repeatNewPassphraseLabel;
    QLineEdit *repeatNewPassphraseEdit;
    QLabel *capsLabel;
    QCheckBox *stakingCheckBox;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *AskPassphraseDialog)
    {
        if (AskPassphraseDialog->objectName().isEmpty())
            AskPassphraseDialog->setObjectName("AskPassphraseDialog");
        AskPassphraseDialog->resize(600, 215);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Minimum);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(AskPassphraseDialog->sizePolicy().hasHeightForWidth());
        AskPassphraseDialog->setSizePolicy(sizePolicy);
        AskPassphraseDialog->setMinimumSize(QSize(550, 0));
        verticalLayout = new QVBoxLayout(AskPassphraseDialog);
        verticalLayout->setObjectName("verticalLayout");
        warningLabel = new QLabel(AskPassphraseDialog);
        warningLabel->setObjectName("warningLabel");
        warningLabel->setTextFormat(Qt::RichText);
        warningLabel->setWordWrap(true);

        verticalLayout->addWidget(warningLabel);

        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        oldPassphraseLabel = new QLabel(AskPassphraseDialog);
        oldPassphraseLabel->setObjectName("oldPassphraseLabel");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, oldPassphraseLabel);

        oldPassphraseEdit = new QLineEdit(AskPassphraseDialog);
        oldPassphraseEdit->setObjectName("oldPassphraseEdit");
        oldPassphraseEdit->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, oldPassphraseEdit);

        newPassphraseLabel = new QLabel(AskPassphraseDialog);
        newPassphraseLabel->setObjectName("newPassphraseLabel");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, newPassphraseLabel);

        newPassphraseEdit = new QLineEdit(AskPassphraseDialog);
        newPassphraseEdit->setObjectName("newPassphraseEdit");
        newPassphraseEdit->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, newPassphraseEdit);

        repeatNewPassphraseLabel = new QLabel(AskPassphraseDialog);
        repeatNewPassphraseLabel->setObjectName("repeatNewPassphraseLabel");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, repeatNewPassphraseLabel);

        repeatNewPassphraseEdit = new QLineEdit(AskPassphraseDialog);
        repeatNewPassphraseEdit->setObjectName("repeatNewPassphraseEdit");
        repeatNewPassphraseEdit->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, repeatNewPassphraseEdit);

        capsLabel = new QLabel(AskPassphraseDialog);
        capsLabel->setObjectName("capsLabel");
        capsLabel->setAlignment(Qt::AlignCenter);

        formLayout->setWidget(3, QFormLayout::ItemRole::FieldRole, capsLabel);

        stakingCheckBox = new QCheckBox(AskPassphraseDialog);
        stakingCheckBox->setObjectName("stakingCheckBox");
        stakingCheckBox->setEnabled(true);
        stakingCheckBox->setVisible(false);

        formLayout->setWidget(4, QFormLayout::ItemRole::LabelRole, stakingCheckBox);


        verticalLayout->addLayout(formLayout);

        buttonBox = new QDialogButtonBox(AskPassphraseDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(AskPassphraseDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, AskPassphraseDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, AskPassphraseDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(AskPassphraseDialog);
    } // setupUi

    void retranslateUi(QDialog *AskPassphraseDialog)
    {
        AskPassphraseDialog->setWindowTitle(QCoreApplication::translate("AskPassphraseDialog", "Passphrase Dialog", nullptr));
        oldPassphraseLabel->setText(QCoreApplication::translate("AskPassphraseDialog", "Enter passphrase", nullptr));
        newPassphraseLabel->setText(QCoreApplication::translate("AskPassphraseDialog", "New passphrase", nullptr));
        repeatNewPassphraseLabel->setText(QCoreApplication::translate("AskPassphraseDialog", "Repeat new passphrase", nullptr));
        capsLabel->setText(QString());
#if QT_CONFIG(tooltip)
        stakingCheckBox->setToolTip(QCoreApplication::translate("AskPassphraseDialog", "Serves to disable the trivial sendmoney when OS account compromised. Provides no real security.", nullptr));
#endif // QT_CONFIG(tooltip)
        stakingCheckBox->setText(QCoreApplication::translate("AskPassphraseDialog", "For staking only", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AskPassphraseDialog: public Ui_AskPassphraseDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ASKPASSPHRASEDIALOG_H
