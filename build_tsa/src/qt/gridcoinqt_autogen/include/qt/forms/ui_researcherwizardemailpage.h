/********************************************************************************
** Form generated from reading UI file 'researcherwizardemailpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDEMAILPAGE_H
#define UI_RESEARCHERWIZARDEMAILPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardEmailPage
{
public:
    QVBoxLayout *verticalLayout;
    QSpacerItem *headerSpacer;
    QFormLayout *formLayout;
    QLabel *emailAddressLabel;
    QLineEdit *emailAddressLineEdit;
    QLabel *emailNoteLabelLabel;
    QLabel *emailNoteLabel;

    void setupUi(QWizardPage *ResearcherWizardEmailPage)
    {
        if (ResearcherWizardEmailPage->objectName().isEmpty())
            ResearcherWizardEmailPage->setObjectName("ResearcherWizardEmailPage");
        ResearcherWizardEmailPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardEmailPage->sizePolicy().hasHeightForWidth());
        ResearcherWizardEmailPage->setSizePolicy(sizePolicy);
        verticalLayout = new QVBoxLayout(ResearcherWizardEmailPage);
        verticalLayout->setObjectName("verticalLayout");
        headerSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout->addItem(headerSpacer);

        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        formLayout->setFormAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        emailAddressLabel = new QLabel(ResearcherWizardEmailPage);
        emailAddressLabel->setObjectName("emailAddressLabel");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, emailAddressLabel);

        emailAddressLineEdit = new QLineEdit(ResearcherWizardEmailPage);
        emailAddressLineEdit->setObjectName("emailAddressLineEdit");
        emailAddressLineEdit->setPlaceholderText(QString::fromUtf8("gridcoin@example.com"));
        emailAddressLineEdit->setClearButtonEnabled(true);

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, emailAddressLineEdit);

        emailNoteLabelLabel = new QLabel(ResearcherWizardEmailPage);
        emailNoteLabelLabel->setObjectName("emailNoteLabelLabel");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, emailNoteLabelLabel);

        emailNoteLabel = new QLabel(ResearcherWizardEmailPage);
        emailNoteLabel->setObjectName("emailNoteLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(emailNoteLabel->sizePolicy().hasHeightForWidth());
        emailNoteLabel->setSizePolicy(sizePolicy1);

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, emailNoteLabel);


        verticalLayout->addLayout(formLayout);


        retranslateUi(ResearcherWizardEmailPage);

        QMetaObject::connectSlotsByName(ResearcherWizardEmailPage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardEmailPage)
    {
        ResearcherWizardEmailPage->setWindowTitle(QCoreApplication::translate("ResearcherWizardEmailPage", "BOINC Email Address", nullptr));
        ResearcherWizardEmailPage->setTitle(QCoreApplication::translate("ResearcherWizardEmailPage", "BOINC Email Address", nullptr));
        ResearcherWizardEmailPage->setSubTitle(QCoreApplication::translate("ResearcherWizardEmailPage", "Enter the email address that you use for your BOINC project accounts. Gridcoin uses this email address to find BOINC projects on your computer.", nullptr));
        emailAddressLabel->setText(QCoreApplication::translate("ResearcherWizardEmailPage", "Email Address:", nullptr));
        emailNoteLabelLabel->setText(QString());
        emailNoteLabel->setText(QCoreApplication::translate("ResearcherWizardEmailPage", "The wallet will never transmit your email address.", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardEmailPage: public Ui_ResearcherWizardEmailPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDEMAILPAGE_H
