/********************************************************************************
** Form generated from reading UI file 'editaddressdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_EDITADDRESSDIALOG_H
#define UI_EDITADDRESSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_EditAddressDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFormLayout *formLayout;
    QLabel *labelTextLabel;
    QLineEdit *labelEdit;
    QLabel *addressTextLabel;
    QLineEdit *addressEdit;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *EditAddressDialog)
    {
        if (EditAddressDialog->objectName().isEmpty())
            EditAddressDialog->setObjectName("EditAddressDialog");
        EditAddressDialog->resize(460, 125);
        verticalLayout = new QVBoxLayout(EditAddressDialog);
        verticalLayout->setObjectName("verticalLayout");
        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        labelTextLabel = new QLabel(EditAddressDialog);
        labelTextLabel->setObjectName("labelTextLabel");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, labelTextLabel);

        labelEdit = new QLineEdit(EditAddressDialog);
        labelEdit->setObjectName("labelEdit");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, labelEdit);

        addressTextLabel = new QLabel(EditAddressDialog);
        addressTextLabel->setObjectName("addressTextLabel");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, addressTextLabel);

        addressEdit = new QLineEdit(EditAddressDialog);
        addressEdit->setObjectName("addressEdit");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, addressEdit);


        verticalLayout->addLayout(formLayout);

        buttonBox = new QDialogButtonBox(EditAddressDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);

#if QT_CONFIG(shortcut)
        labelTextLabel->setBuddy(labelEdit);
        addressTextLabel->setBuddy(addressEdit);
#endif // QT_CONFIG(shortcut)

        retranslateUi(EditAddressDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, EditAddressDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, EditAddressDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(EditAddressDialog);
    } // setupUi

    void retranslateUi(QDialog *EditAddressDialog)
    {
        EditAddressDialog->setWindowTitle(QCoreApplication::translate("EditAddressDialog", "Edit Address", nullptr));
        labelTextLabel->setText(QCoreApplication::translate("EditAddressDialog", "&Label", nullptr));
#if QT_CONFIG(tooltip)
        labelEdit->setToolTip(QCoreApplication::translate("EditAddressDialog", "The label associated with this address book entry", nullptr));
#endif // QT_CONFIG(tooltip)
        addressTextLabel->setText(QCoreApplication::translate("EditAddressDialog", "&Address", nullptr));
#if QT_CONFIG(tooltip)
        addressEdit->setToolTip(QCoreApplication::translate("EditAddressDialog", "The address associated with this address book entry. This can only be modified for sending addresses.", nullptr));
#endif // QT_CONFIG(tooltip)
    } // retranslateUi

};

namespace Ui {
    class EditAddressDialog: public Ui_EditAddressDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_EDITADDRESSDIALOG_H
