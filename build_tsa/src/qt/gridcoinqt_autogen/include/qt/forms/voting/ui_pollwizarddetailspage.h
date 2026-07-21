/********************************************************************************
** Form generated from reading UI file 'pollwizarddetailspage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLWIZARDDETAILSPAGE_H
#define UI_POLLWIZARDDETAILSPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>
#include "voting/additionalfieldstableview.h"

QT_BEGIN_NAMESPACE

class Ui_PollWizardDetailsPage
{
public:
    QVBoxLayout *pageLayout;
    QLabel *pageTitleLabel;
    QFrame *headerLine;
    QLabel *pollTypeAlert;
    QLabel *errorLabel;
    QFormLayout *formLayout;
    QLabel *pollTypeTextLabel;
    QHBoxLayout *pollTypeLayout;
    QLabel *pollTypeLabel;
    QSpacerItem *pollTypeSpacer;
    QLabel *durationLabel;
    QSpinBox *durationField;
    QLabel *titleLabel;
    QLineEdit *titleField;
    QLabel *questionLabel;
    QLineEdit *questionField;
    QLabel *urlLabel;
    QVBoxLayout *urlLayout;
    QLineEdit *urlField;
    QLabel *urlDescriptionLabel;
    QLabel *weightTypeLabel;
    QComboBox *weightTypeList;
    QLabel *responseTypeLabel;
    QComboBox *responseTypeList;
    QLabel *choicesLabel;
    QVBoxLayout *choicesLayout;
    QLabel *standardChoicesLabel;
    QFrame *choicesFrame;
    QVBoxLayout *choicesListLayout;
    QListView *choicesList;
    QFrame *choicesButtonFrame;
    QHBoxLayout *choicesButtonsLayout;
    QToolButton *addChoiceButton;
    QToolButton *removeChoiceButton;
    QToolButton *editChoiceButton;
    QSpacerItem *choicesButtonSpacer;
    AdditionalFieldsTableView *additionalFieldsTableView;
    QLabel *additionalFieldsLabel;

    void setupUi(QWizardPage *PollWizardDetailsPage)
    {
        if (PollWizardDetailsPage->objectName().isEmpty())
            PollWizardDetailsPage->setObjectName("PollWizardDetailsPage");
        PollWizardDetailsPage->resize(630, 753);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(PollWizardDetailsPage->sizePolicy().hasHeightForWidth());
        PollWizardDetailsPage->setSizePolicy(sizePolicy);
        pageLayout = new QVBoxLayout(PollWizardDetailsPage);
        pageLayout->setSpacing(9);
        pageLayout->setObjectName("pageLayout");
        pageLayout->setContentsMargins(16, 16, 16, 16);
        pageTitleLabel = new QLabel(PollWizardDetailsPage);
        pageTitleLabel->setObjectName("pageTitleLabel");

        pageLayout->addWidget(pageTitleLabel);

        headerLine = new QFrame(PollWizardDetailsPage);
        headerLine->setObjectName("headerLine");
        headerLine->setFrameShape(QFrame::Shape::HLine);
        headerLine->setFrameShadow(QFrame::Shadow::Sunken);

        pageLayout->addWidget(headerLine);

        pollTypeAlert = new QLabel(PollWizardDetailsPage);
        pollTypeAlert->setObjectName("pollTypeAlert");

        pageLayout->addWidget(pollTypeAlert);

        errorLabel = new QLabel(PollWizardDetailsPage);
        errorLabel->setObjectName("errorLabel");
        errorLabel->setWordWrap(true);

        pageLayout->addWidget(errorLabel);

        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        formLayout->setLabelAlignment(Qt::AlignRight|Qt::AlignTop|Qt::AlignTrailing);
        formLayout->setHorizontalSpacing(12);
        formLayout->setVerticalSpacing(12);
        pollTypeTextLabel = new QLabel(PollWizardDetailsPage);
        pollTypeTextLabel->setObjectName("pollTypeTextLabel");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, pollTypeTextLabel);

        pollTypeLayout = new QHBoxLayout();
        pollTypeLayout->setObjectName("pollTypeLayout");
        pollTypeLabel = new QLabel(PollWizardDetailsPage);
        pollTypeLabel->setObjectName("pollTypeLabel");

        pollTypeLayout->addWidget(pollTypeLabel, 0, Qt::AlignVCenter);

        pollTypeSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        pollTypeLayout->addItem(pollTypeSpacer);

        durationLabel = new QLabel(PollWizardDetailsPage);
        durationLabel->setObjectName("durationLabel");

        pollTypeLayout->addWidget(durationLabel, 0, Qt::AlignVCenter);

        durationField = new QSpinBox(PollWizardDetailsPage);
        durationField->setObjectName("durationField");

        pollTypeLayout->addWidget(durationField, 0, Qt::AlignVCenter);


        formLayout->setLayout(0, QFormLayout::ItemRole::FieldRole, pollTypeLayout);

        titleLabel = new QLabel(PollWizardDetailsPage);
        titleLabel->setObjectName("titleLabel");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, titleLabel);

        titleField = new QLineEdit(PollWizardDetailsPage);
        titleField->setObjectName("titleField");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, titleField);

        questionLabel = new QLabel(PollWizardDetailsPage);
        questionLabel->setObjectName("questionLabel");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, questionLabel);

        questionField = new QLineEdit(PollWizardDetailsPage);
        questionField->setObjectName("questionField");

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, questionField);

        urlLabel = new QLabel(PollWizardDetailsPage);
        urlLabel->setObjectName("urlLabel");

        formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, urlLabel);

        urlLayout = new QVBoxLayout();
        urlLayout->setSpacing(3);
        urlLayout->setObjectName("urlLayout");
        urlField = new QLineEdit(PollWizardDetailsPage);
        urlField->setObjectName("urlField");

        urlLayout->addWidget(urlField);

        urlDescriptionLabel = new QLabel(PollWizardDetailsPage);
        urlDescriptionLabel->setObjectName("urlDescriptionLabel");
        urlDescriptionLabel->setWordWrap(true);

        urlLayout->addWidget(urlDescriptionLabel);


        formLayout->setLayout(3, QFormLayout::ItemRole::FieldRole, urlLayout);

        weightTypeLabel = new QLabel(PollWizardDetailsPage);
        weightTypeLabel->setObjectName("weightTypeLabel");

        formLayout->setWidget(5, QFormLayout::ItemRole::LabelRole, weightTypeLabel);

        weightTypeList = new QComboBox(PollWizardDetailsPage);
        weightTypeList->setObjectName("weightTypeList");

        formLayout->setWidget(5, QFormLayout::ItemRole::FieldRole, weightTypeList);

        responseTypeLabel = new QLabel(PollWizardDetailsPage);
        responseTypeLabel->setObjectName("responseTypeLabel");

        formLayout->setWidget(6, QFormLayout::ItemRole::LabelRole, responseTypeLabel);

        responseTypeList = new QComboBox(PollWizardDetailsPage);
        responseTypeList->setObjectName("responseTypeList");

        formLayout->setWidget(6, QFormLayout::ItemRole::FieldRole, responseTypeList);

        choicesLabel = new QLabel(PollWizardDetailsPage);
        choicesLabel->setObjectName("choicesLabel");

        formLayout->setWidget(7, QFormLayout::ItemRole::LabelRole, choicesLabel);

        choicesLayout = new QVBoxLayout();
        choicesLayout->setObjectName("choicesLayout");
        choicesLayout->setContentsMargins(-1, -1, 0, 0);
        standardChoicesLabel = new QLabel(PollWizardDetailsPage);
        standardChoicesLabel->setObjectName("standardChoicesLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(standardChoicesLabel->sizePolicy().hasHeightForWidth());
        standardChoicesLabel->setSizePolicy(sizePolicy1);
        standardChoicesLabel->setWordWrap(true);

        choicesLayout->addWidget(standardChoicesLabel);

        choicesFrame = new QFrame(PollWizardDetailsPage);
        choicesFrame->setObjectName("choicesFrame");
        sizePolicy.setHeightForWidth(choicesFrame->sizePolicy().hasHeightForWidth());
        choicesFrame->setSizePolicy(sizePolicy);
        choicesListLayout = new QVBoxLayout(choicesFrame);
        choicesListLayout->setSpacing(0);
        choicesListLayout->setObjectName("choicesListLayout");
        choicesListLayout->setContentsMargins(0, 0, 0, 0);
        choicesList = new QListView(choicesFrame);
        choicesList->setObjectName("choicesList");
        choicesList->setAcceptDrops(true);
        choicesList->setDragEnabled(true);
        choicesList->setDragDropMode(QAbstractItemView::InternalMove);

        choicesListLayout->addWidget(choicesList);

        choicesButtonFrame = new QFrame(choicesFrame);
        choicesButtonFrame->setObjectName("choicesButtonFrame");
        choicesButtonsLayout = new QHBoxLayout(choicesButtonFrame);
        choicesButtonsLayout->setSpacing(0);
        choicesButtonsLayout->setObjectName("choicesButtonsLayout");
        choicesButtonsLayout->setContentsMargins(0, 0, 0, 0);
        addChoiceButton = new QToolButton(choicesButtonFrame);
        addChoiceButton->setObjectName("addChoiceButton");
        addChoiceButton->setProperty("firstChild", QVariant(true));

        choicesButtonsLayout->addWidget(addChoiceButton);

        removeChoiceButton = new QToolButton(choicesButtonFrame);
        removeChoiceButton->setObjectName("removeChoiceButton");

        choicesButtonsLayout->addWidget(removeChoiceButton);

        editChoiceButton = new QToolButton(choicesButtonFrame);
        editChoiceButton->setObjectName("editChoiceButton");

        choicesButtonsLayout->addWidget(editChoiceButton);

        choicesButtonSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        choicesButtonsLayout->addItem(choicesButtonSpacer);


        choicesListLayout->addWidget(choicesButtonFrame);


        choicesLayout->addWidget(choicesFrame);


        formLayout->setLayout(7, QFormLayout::ItemRole::FieldRole, choicesLayout);

        additionalFieldsTableView = new AdditionalFieldsTableView(PollWizardDetailsPage);
        additionalFieldsTableView->setObjectName("additionalFieldsTableView");

        formLayout->setWidget(4, QFormLayout::ItemRole::FieldRole, additionalFieldsTableView);

        additionalFieldsLabel = new QLabel(PollWizardDetailsPage);
        additionalFieldsLabel->setObjectName("additionalFieldsLabel");

        formLayout->setWidget(4, QFormLayout::ItemRole::LabelRole, additionalFieldsLabel);


        pageLayout->addLayout(formLayout);

        pageLayout->setStretch(4, 1);
#if QT_CONFIG(shortcut)
        durationLabel->setBuddy(durationField);
        titleLabel->setBuddy(titleField);
        questionLabel->setBuddy(questionField);
        urlLabel->setBuddy(urlField);
        weightTypeLabel->setBuddy(weightTypeList);
        responseTypeLabel->setBuddy(responseTypeList);
#endif // QT_CONFIG(shortcut)
        QWidget::setTabOrder(durationField, titleField);
        QWidget::setTabOrder(titleField, questionField);
        QWidget::setTabOrder(questionField, urlField);
        QWidget::setTabOrder(urlField, weightTypeList);
        QWidget::setTabOrder(weightTypeList, responseTypeList);

        retranslateUi(PollWizardDetailsPage);

        QMetaObject::connectSlotsByName(PollWizardDetailsPage);
    } // setupUi

    void retranslateUi(QWizardPage *PollWizardDetailsPage)
    {
        pageTitleLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "Poll Details", nullptr));
        pollTypeAlert->setText(QCoreApplication::translate("PollWizardDetailsPage", "Some fields are locked for the selected poll type.", nullptr));
        pollTypeTextLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "Poll Type:", nullptr));
        durationLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "Duration:", nullptr));
        durationField->setSuffix(QCoreApplication::translate("PollWizardDetailsPage", " days", nullptr));
        titleLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "Title:", nullptr));
        questionLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "Question:", nullptr));
        urlLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "Discussion URL:", nullptr));
        urlDescriptionLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "A link to the main discussion thread on GitHub or Reddit.", nullptr));
        weightTypeLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "Weight Type:", nullptr));
        responseTypeLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "Response Type:", nullptr));
        choicesLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "Choices:", nullptr));
        standardChoicesLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "A poll with a yes/no/abstain response type cannot include any additional custom choices.", nullptr));
        additionalFieldsLabel->setText(QCoreApplication::translate("PollWizardDetailsPage", "Additional Fields:", nullptr));
        (void)PollWizardDetailsPage;
    } // retranslateUi

};

namespace Ui {
    class PollWizardDetailsPage: public Ui_PollWizardDetailsPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLWIZARDDETAILSPAGE_H
