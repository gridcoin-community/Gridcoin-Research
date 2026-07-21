/********************************************************************************
** Form generated from reading UI file 'editsidestakedialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_EDITSIDESTAKEDIALOG_H
#define UI_EDITSIDESTAKEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_EditSideStakeDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFormLayout *formLayout;
    QSpacerItem *verticalSpacer_4;
    QLabel *addressLabel;
    QLineEdit *addressLineEdit;
    QSpacerItem *verticalSpacer_2;
    QLabel *allocationLabel;
    QLineEdit *allocationLineEdit;
    QSpacerItem *verticalSpacer;
    QLabel *descriptionLabel;
    QLineEdit *descriptionLineEdit;
    QSpacerItem *verticalSpacer_3;
    QLineEdit *statusLineEdit;
    QLabel *statusLabel;
    QSpacerItem *verticalSpacer_5;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *EditSideStakeDialog)
    {
        if (EditSideStakeDialog->objectName().isEmpty())
            EditSideStakeDialog->setObjectName("EditSideStakeDialog");
        EditSideStakeDialog->resize(400, 300);
        verticalLayout = new QVBoxLayout(EditSideStakeDialog);
        verticalLayout->setObjectName("verticalLayout");
        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        verticalSpacer_4 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        formLayout->setItem(0, QFormLayout::ItemRole::FieldRole, verticalSpacer_4);

        addressLabel = new QLabel(EditSideStakeDialog);
        addressLabel->setObjectName("addressLabel");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, addressLabel);

        addressLineEdit = new QLineEdit(EditSideStakeDialog);
        addressLineEdit->setObjectName("addressLineEdit");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, addressLineEdit);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        formLayout->setItem(2, QFormLayout::ItemRole::FieldRole, verticalSpacer_2);

        allocationLabel = new QLabel(EditSideStakeDialog);
        allocationLabel->setObjectName("allocationLabel");

        formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, allocationLabel);

        allocationLineEdit = new QLineEdit(EditSideStakeDialog);
        allocationLineEdit->setObjectName("allocationLineEdit");

        formLayout->setWidget(3, QFormLayout::ItemRole::FieldRole, allocationLineEdit);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        formLayout->setItem(4, QFormLayout::ItemRole::FieldRole, verticalSpacer);

        descriptionLabel = new QLabel(EditSideStakeDialog);
        descriptionLabel->setObjectName("descriptionLabel");

        formLayout->setWidget(5, QFormLayout::ItemRole::LabelRole, descriptionLabel);

        descriptionLineEdit = new QLineEdit(EditSideStakeDialog);
        descriptionLineEdit->setObjectName("descriptionLineEdit");

        formLayout->setWidget(5, QFormLayout::ItemRole::FieldRole, descriptionLineEdit);

        verticalSpacer_3 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        formLayout->setItem(6, QFormLayout::ItemRole::FieldRole, verticalSpacer_3);

        statusLineEdit = new QLineEdit(EditSideStakeDialog);
        statusLineEdit->setObjectName("statusLineEdit");

        formLayout->setWidget(7, QFormLayout::ItemRole::FieldRole, statusLineEdit);

        statusLabel = new QLabel(EditSideStakeDialog);
        statusLabel->setObjectName("statusLabel");

        formLayout->setWidget(7, QFormLayout::ItemRole::LabelRole, statusLabel);

        verticalSpacer_5 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        formLayout->setItem(8, QFormLayout::ItemRole::FieldRole, verticalSpacer_5);


        verticalLayout->addLayout(formLayout);

        buttonBox = new QDialogButtonBox(EditSideStakeDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(EditSideStakeDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, EditSideStakeDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, EditSideStakeDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(EditSideStakeDialog);
    } // setupUi

    void retranslateUi(QDialog *EditSideStakeDialog)
    {
        EditSideStakeDialog->setWindowTitle(QCoreApplication::translate("EditSideStakeDialog", "Add or Edit SideStake", nullptr));
        addressLabel->setText(QCoreApplication::translate("EditSideStakeDialog", "Address", nullptr));
        allocationLabel->setText(QCoreApplication::translate("EditSideStakeDialog", "Allocation", nullptr));
        descriptionLabel->setText(QCoreApplication::translate("EditSideStakeDialog", "Description", nullptr));
        statusLabel->setText(QCoreApplication::translate("EditSideStakeDialog", "Status", nullptr));
    } // retranslateUi

};

namespace Ui {
    class EditSideStakeDialog: public Ui_EditSideStakeDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_EDITSIDESTAKEDIALOG_H
