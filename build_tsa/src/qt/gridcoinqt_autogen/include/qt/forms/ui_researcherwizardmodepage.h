/********************************************************************************
** Form generated from reading UI file 'researcherwizardmodepage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDMODEPAGE_H
#define UI_RESEARCHERWIZARDMODEPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCommandLinkButton>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>
#include "clicklabel.h"

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardModePage
{
public:
    QVBoxLayout *verticalLayout;
    QSpacerItem *headerSpacer;
    QLabel *gridcoinIconLabel;
    QLabel *titleLabel;
    QSpacerItem *verticalSpacer;
    QGridLayout *modeLayout;
    QRadioButton *soloRadioButton;
    QRadioButton *poolRadioButton;
    ClickLabel *soloIconLabel;
    ClickLabel *poolIconLabel;
    ClickLabel *noncruncherIconLabel;
    QRadioButton *noncruncherRadioButton;
    QCommandLinkButton *detailLinkButton;
    QSpacerItem *footerSpacer;
    QButtonGroup *modeButtonGroup;

    void setupUi(QWizardPage *ResearcherWizardModePage)
    {
        if (ResearcherWizardModePage->objectName().isEmpty())
            ResearcherWizardModePage->setObjectName("ResearcherWizardModePage");
        ResearcherWizardModePage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardModePage->sizePolicy().hasHeightForWidth());
        ResearcherWizardModePage->setSizePolicy(sizePolicy);
        verticalLayout = new QVBoxLayout(ResearcherWizardModePage);
        verticalLayout->setSpacing(20);
        verticalLayout->setObjectName("verticalLayout");
        headerSpacer = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout->addItem(headerSpacer);

        gridcoinIconLabel = new QLabel(ResearcherWizardModePage);
        gridcoinIconLabel->setObjectName("gridcoinIconLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(gridcoinIconLabel->sizePolicy().hasHeightForWidth());
        gridcoinIconLabel->setSizePolicy(sizePolicy1);
        gridcoinIconLabel->setMinimumSize(QSize(64, 64));
        gridcoinIconLabel->setMaximumSize(QSize(64, 64));
        gridcoinIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/gridcoin")));
        gridcoinIconLabel->setScaledContents(true);

        verticalLayout->addWidget(gridcoinIconLabel, 0, Qt::AlignHCenter|Qt::AlignTop);

        titleLabel = new QLabel(ResearcherWizardModePage);
        titleLabel->setObjectName("titleLabel");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setMargin(1);

        verticalLayout->addWidget(titleLabel, 0, Qt::AlignHCenter|Qt::AlignTop);

        verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        modeLayout = new QGridLayout();
        modeLayout->setObjectName("modeLayout");
        soloRadioButton = new QRadioButton(ResearcherWizardModePage);
        modeButtonGroup = new QButtonGroup(ResearcherWizardModePage);
        modeButtonGroup->setObjectName("modeButtonGroup");
        modeButtonGroup->addButton(soloRadioButton);
        soloRadioButton->setObjectName("soloRadioButton");

        modeLayout->addWidget(soloRadioButton, 1, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolRadioButton = new QRadioButton(ResearcherWizardModePage);
        modeButtonGroup->addButton(poolRadioButton);
        poolRadioButton->setObjectName("poolRadioButton");

        modeLayout->addWidget(poolRadioButton, 1, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        soloIconLabel = new ClickLabel(ResearcherWizardModePage);
        soloIconLabel->setObjectName("soloIconLabel");
        soloIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/ic_solo_inactive")));
        soloIconLabel->setScaledContents(true);

        modeLayout->addWidget(soloIconLabel, 0, 0, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        poolIconLabel = new ClickLabel(ResearcherWizardModePage);
        poolIconLabel->setObjectName("poolIconLabel");
        poolIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/ic_pool_inactive")));
        poolIconLabel->setScaledContents(true);

        modeLayout->addWidget(poolIconLabel, 0, 1, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherIconLabel = new ClickLabel(ResearcherWizardModePage);
        noncruncherIconLabel->setObjectName("noncruncherIconLabel");
        noncruncherIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/ic_noncruncher_inactive")));
        noncruncherIconLabel->setScaledContents(true);

        modeLayout->addWidget(noncruncherIconLabel, 0, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);

        noncruncherRadioButton = new QRadioButton(ResearcherWizardModePage);
        modeButtonGroup->addButton(noncruncherRadioButton);
        noncruncherRadioButton->setObjectName("noncruncherRadioButton");

        modeLayout->addWidget(noncruncherRadioButton, 1, 2, 1, 1, Qt::AlignHCenter|Qt::AlignVCenter);


        verticalLayout->addLayout(modeLayout);

        detailLinkButton = new QCommandLinkButton(ResearcherWizardModePage);
        detailLinkButton->setObjectName("detailLinkButton");

        verticalLayout->addWidget(detailLinkButton, 0, Qt::AlignHCenter);

        footerSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(footerSpacer);


        retranslateUi(ResearcherWizardModePage);

        QMetaObject::connectSlotsByName(ResearcherWizardModePage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardModePage)
    {
        ResearcherWizardModePage->setWindowTitle(QCoreApplication::translate("ResearcherWizardModePage", "Select Researcher Mode", nullptr));
        ResearcherWizardModePage->setTitle(QString());
        ResearcherWizardModePage->setSubTitle(QString());
        gridcoinIconLabel->setText(QString());
        titleLabel->setText(QCoreApplication::translate("ResearcherWizardModePage", "How would you like to participate?", nullptr));
        soloRadioButton->setText(QCoreApplication::translate("ResearcherWizardModePage", "Solo", nullptr));
        poolRadioButton->setText(QCoreApplication::translate("ResearcherWizardModePage", "Pool", nullptr));
        soloIconLabel->setText(QString());
        poolIconLabel->setText(QString());
        noncruncherIconLabel->setText(QString());
        noncruncherRadioButton->setText(QCoreApplication::translate("ResearcherWizardModePage", "Non-cruncher", nullptr));
        detailLinkButton->setText(QCoreApplication::translate("ResearcherWizardModePage", "Help me choose...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardModePage: public Ui_ResearcherWizardModePage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDMODEPAGE_H
