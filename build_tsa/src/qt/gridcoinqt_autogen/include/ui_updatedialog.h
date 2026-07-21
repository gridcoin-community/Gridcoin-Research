/********************************************************************************
** Form generated from reading UI file 'updatedialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_UPDATEDIALOG_H
#define UI_UPDATEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_UpdateDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFormLayout *formLayout;
    QLabel *infoIcon;
    QLabel *versionData;
    QTextEdit *versionDetails;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *UpdateDialog)
    {
        if (UpdateDialog->objectName().isEmpty())
            UpdateDialog->setObjectName("UpdateDialog");
        UpdateDialog->resize(609, 430);
        verticalLayout = new QVBoxLayout(UpdateDialog);
        verticalLayout->setObjectName("verticalLayout");
        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        infoIcon = new QLabel(UpdateDialog);
        infoIcon->setObjectName("infoIcon");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, infoIcon);

        versionData = new QLabel(UpdateDialog);
        versionData->setObjectName("versionData");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, versionData);


        verticalLayout->addLayout(formLayout);

        versionDetails = new QTextEdit(UpdateDialog);
        versionDetails->setObjectName("versionDetails");
        versionDetails->setUndoRedoEnabled(false);
        versionDetails->setReadOnly(true);
        versionDetails->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard|Qt::LinksAccessibleByMouse|Qt::TextBrowserInteraction|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        verticalLayout->addWidget(versionDetails);

        buttonBox = new QDialogButtonBox(UpdateDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(UpdateDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, UpdateDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, UpdateDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(UpdateDialog);
    } // setupUi

    void retranslateUi(QDialog *UpdateDialog)
    {
        UpdateDialog->setWindowTitle(QCoreApplication::translate("UpdateDialog", "Dialog", nullptr));
        infoIcon->setText(QCoreApplication::translate("UpdateDialog", "icon", nullptr));
        versionData->setText(QCoreApplication::translate("UpdateDialog", "version", nullptr));
        versionDetails->setPlaceholderText(QCoreApplication::translate("UpdateDialog", "changelog", nullptr));
    } // retranslateUi

};

namespace Ui {
    class UpdateDialog: public Ui_UpdateDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_UPDATEDIALOG_H
