/********************************************************************************
** Form generated from reading UI file 'pollwizardprojectpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLWIZARDPROJECTPAGE_H
#define UI_POLLWIZARDPROJECTPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_PollWizardProjectPage
{
public:
    QVBoxLayout *pageLayout;
    QLabel *pageTitleLabel;
    QLineEdit *addRemoveStateLineEdit;
    QFrame *headerLine;
    QRadioButton *addRadioButton;
    QRadioButton *removeRadioButton;
    QWidget *addWidget;
    QVBoxLayout *addLayout;
    QFrame *addTopLine;
    QLabel *requirementsLabel;
    QHBoxLayout *linkLayout;
    QLabel *linkIconLabel;
    QLabel *wikiLinkLabel;
    QFrame *addLine;
    QFormLayout *formLayout;
    QLabel *projectNameLabel;
    QLineEdit *projectNameField;
    QCheckBox *criteriaCheckbox;
    QLineEdit *projectUrlField;
    QLabel *projectUrlLabel;
    QWidget *removeWidget;
    QVBoxLayout *removeLayout;
    QFrame *removeTopLine;
    QLabel *removeLabel;
    QListView *projectsList;
    QSpacerItem *bottomSpacer;

    void setupUi(QWizardPage *PollWizardProjectPage)
    {
        if (PollWizardProjectPage->objectName().isEmpty())
            PollWizardProjectPage->setObjectName("PollWizardProjectPage");
        PollWizardProjectPage->resize(664, 597);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(PollWizardProjectPage->sizePolicy().hasHeightForWidth());
        PollWizardProjectPage->setSizePolicy(sizePolicy);
        pageLayout = new QVBoxLayout(PollWizardProjectPage);
        pageLayout->setSpacing(9);
        pageLayout->setObjectName("pageLayout");
        pageLayout->setContentsMargins(16, 16, 16, 16);
        pageTitleLabel = new QLabel(PollWizardProjectPage);
        pageTitleLabel->setObjectName("pageTitleLabel");

        pageLayout->addWidget(pageTitleLabel);

        addRemoveStateLineEdit = new QLineEdit(PollWizardProjectPage);
        addRemoveStateLineEdit->setObjectName("addRemoveStateLineEdit");

        pageLayout->addWidget(addRemoveStateLineEdit);

        headerLine = new QFrame(PollWizardProjectPage);
        headerLine->setObjectName("headerLine");
        headerLine->setFrameShape(QFrame::Shape::HLine);
        headerLine->setFrameShadow(QFrame::Shadow::Sunken);

        pageLayout->addWidget(headerLine);

        addRadioButton = new QRadioButton(PollWizardProjectPage);
        addRadioButton->setObjectName("addRadioButton");

        pageLayout->addWidget(addRadioButton);

        removeRadioButton = new QRadioButton(PollWizardProjectPage);
        removeRadioButton->setObjectName("removeRadioButton");

        pageLayout->addWidget(removeRadioButton);

        addWidget = new QWidget(PollWizardProjectPage);
        addWidget->setObjectName("addWidget");
        sizePolicy.setHeightForWidth(addWidget->sizePolicy().hasHeightForWidth());
        addWidget->setSizePolicy(sizePolicy);
        addLayout = new QVBoxLayout(addWidget);
        addLayout->setSpacing(9);
        addLayout->setObjectName("addLayout");
        addLayout->setContentsMargins(0, 0, 0, 0);
        addTopLine = new QFrame(addWidget);
        addTopLine->setObjectName("addTopLine");
        addTopLine->setFrameShape(QFrame::Shape::HLine);
        addTopLine->setFrameShadow(QFrame::Shadow::Sunken);

        addLayout->addWidget(addTopLine);

        requirementsLabel = new QLabel(addWidget);
        requirementsLabel->setObjectName("requirementsLabel");
        requirementsLabel->setWordWrap(true);

        addLayout->addWidget(requirementsLabel);

        linkLayout = new QHBoxLayout();
        linkLayout->setObjectName("linkLayout");
        linkLayout->setContentsMargins(-1, 0, -1, -1);
        linkIconLabel = new QLabel(addWidget);
        linkIconLabel->setObjectName("linkIconLabel");

        linkLayout->addWidget(linkIconLabel);

        wikiLinkLabel = new QLabel(addWidget);
        wikiLinkLabel->setObjectName("wikiLinkLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(1);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(wikiLinkLabel->sizePolicy().hasHeightForWidth());
        wikiLinkLabel->setSizePolicy(sizePolicy1);
        wikiLinkLabel->setText(QString::fromUtf8("<a href=\"https://gridcoin.us/wiki/whitelist-process\">https://gridcoin.us/wiki/whitelist-process</a>"));
        wikiLinkLabel->setOpenExternalLinks(true);
        wikiLinkLabel->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard|Qt::LinksAccessibleByMouse);

        linkLayout->addWidget(wikiLinkLabel);


        addLayout->addLayout(linkLayout);

        addLine = new QFrame(addWidget);
        addLine->setObjectName("addLine");
        addLine->setFrameShape(QFrame::Shape::HLine);
        addLine->setFrameShadow(QFrame::Shadow::Sunken);

        addLayout->addWidget(addLine);

        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        formLayout->setContentsMargins(-1, 20, -1, -1);
        projectNameLabel = new QLabel(addWidget);
        projectNameLabel->setObjectName("projectNameLabel");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, projectNameLabel);

        projectNameField = new QLineEdit(addWidget);
        projectNameField->setObjectName("projectNameField");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, projectNameField);

        criteriaCheckbox = new QCheckBox(addWidget);
        criteriaCheckbox->setObjectName("criteriaCheckbox");

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, criteriaCheckbox);

        projectUrlField = new QLineEdit(addWidget);
        projectUrlField->setObjectName("projectUrlField");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, projectUrlField);

        projectUrlLabel = new QLabel(addWidget);
        projectUrlLabel->setObjectName("projectUrlLabel");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, projectUrlLabel);


        addLayout->addLayout(formLayout);


        pageLayout->addWidget(addWidget);

        removeWidget = new QWidget(PollWizardProjectPage);
        removeWidget->setObjectName("removeWidget");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(1);
        sizePolicy2.setHeightForWidth(removeWidget->sizePolicy().hasHeightForWidth());
        removeWidget->setSizePolicy(sizePolicy2);
        removeLayout = new QVBoxLayout(removeWidget);
        removeLayout->setSpacing(9);
        removeLayout->setObjectName("removeLayout");
        removeLayout->setContentsMargins(0, 0, 0, 0);
        removeTopLine = new QFrame(removeWidget);
        removeTopLine->setObjectName("removeTopLine");
        removeTopLine->setFrameShape(QFrame::Shape::HLine);
        removeTopLine->setFrameShadow(QFrame::Shadow::Sunken);

        removeLayout->addWidget(removeTopLine);

        removeLabel = new QLabel(removeWidget);
        removeLabel->setObjectName("removeLabel");

        removeLayout->addWidget(removeLabel);

        projectsList = new QListView(removeWidget);
        projectsList->setObjectName("projectsList");
        projectsList->setEditTriggers(QAbstractItemView::NoEditTriggers);

        removeLayout->addWidget(projectsList);


        pageLayout->addWidget(removeWidget);

        bottomSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        pageLayout->addItem(bottomSpacer);

#if QT_CONFIG(shortcut)
        projectNameLabel->setBuddy(projectNameField);
#endif // QT_CONFIG(shortcut)

        retranslateUi(PollWizardProjectPage);

        QMetaObject::connectSlotsByName(PollWizardProjectPage);
    } // setupUi

    void retranslateUi(QWizardPage *PollWizardProjectPage)
    {
        pageTitleLabel->setText(QCoreApplication::translate("PollWizardProjectPage", "Project Listing Proposal", nullptr));
        addRadioButton->setText(QCoreApplication::translate("PollWizardProjectPage", "Add an unlisted project", nullptr));
        removeRadioButton->setText(QCoreApplication::translate("PollWizardProjectPage", "Remove a listed project", nullptr));
        requirementsLabel->setText(QCoreApplication::translate("PollWizardProjectPage", "Proposals must follow community guidelines for validation. Please review the wiki and verify that the prerequisites have been fulfilled:", nullptr));
        projectNameLabel->setText(QCoreApplication::translate("PollWizardProjectPage", "Project Name:", nullptr));
        criteriaCheckbox->setText(QCoreApplication::translate("PollWizardProjectPage", "This project satisfies the Gridcoin listing criteria.", nullptr));
        projectUrlLabel->setText(QCoreApplication::translate("PollWizardProjectPage", "Project URL", nullptr));
        removeLabel->setText(QCoreApplication::translate("PollWizardProjectPage", "Choose a project to delist:", nullptr));
        (void)PollWizardProjectPage;
    } // retranslateUi

};

namespace Ui {
    class PollWizardProjectPage: public Ui_PollWizardProjectPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLWIZARDPROJECTPAGE_H
