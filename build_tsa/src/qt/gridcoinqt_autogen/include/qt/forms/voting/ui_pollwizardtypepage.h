/********************************************************************************
** Form generated from reading UI file 'pollwizardtypepage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLWIZARDTYPEPAGE_H
#define UI_POLLWIZARDTYPEPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_PollWizardTypePage
{
public:
    QVBoxLayout *pageLayout;
    QLabel *pageTitleLabel;
    QLabel *wikiBlurbLabel;
    QHBoxLayout *linkLayout;
    QLabel *linkIconLabel;
    QLabel *wikiLinkLabel;
    QFrame *headerLine;
    QLabel *typeTextLabel;
    QGridLayout *typesButtonLayout;
    QSpacerItem *bottomSpacer;

    void setupUi(QWizardPage *PollWizardTypePage)
    {
        if (PollWizardTypePage->objectName().isEmpty())
            PollWizardTypePage->setObjectName("PollWizardTypePage");
        PollWizardTypePage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(PollWizardTypePage->sizePolicy().hasHeightForWidth());
        PollWizardTypePage->setSizePolicy(sizePolicy);
        pageLayout = new QVBoxLayout(PollWizardTypePage);
        pageLayout->setSpacing(9);
        pageLayout->setObjectName("pageLayout");
        pageLayout->setContentsMargins(16, 16, 16, 16);
        pageTitleLabel = new QLabel(PollWizardTypePage);
        pageTitleLabel->setObjectName("pageTitleLabel");

        pageLayout->addWidget(pageTitleLabel);

        wikiBlurbLabel = new QLabel(PollWizardTypePage);
        wikiBlurbLabel->setObjectName("wikiBlurbLabel");
        wikiBlurbLabel->setWordWrap(true);

        pageLayout->addWidget(wikiBlurbLabel);

        linkLayout = new QHBoxLayout();
        linkLayout->setObjectName("linkLayout");
        linkLayout->setContentsMargins(-1, 0, -1, -1);
        linkIconLabel = new QLabel(PollWizardTypePage);
        linkIconLabel->setObjectName("linkIconLabel");

        linkLayout->addWidget(linkIconLabel);

        wikiLinkLabel = new QLabel(PollWizardTypePage);
        wikiLinkLabel->setObjectName("wikiLinkLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(1);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(wikiLinkLabel->sizePolicy().hasHeightForWidth());
        wikiLinkLabel->setSizePolicy(sizePolicy1);
        wikiLinkLabel->setText(QString::fromUtf8("<a href=\"https://gridcoin.us/wiki/voting.html\">https://gridcoin.us/wiki/voting.html</a>"));
        wikiLinkLabel->setOpenExternalLinks(true);
        wikiLinkLabel->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard|Qt::LinksAccessibleByMouse);

        linkLayout->addWidget(wikiLinkLabel);


        pageLayout->addLayout(linkLayout);

        headerLine = new QFrame(PollWizardTypePage);
        headerLine->setObjectName("headerLine");
        headerLine->setFrameShape(QFrame::Shape::HLine);
        headerLine->setFrameShadow(QFrame::Shadow::Sunken);

        pageLayout->addWidget(headerLine);

        typeTextLabel = new QLabel(PollWizardTypePage);
        typeTextLabel->setObjectName("typeTextLabel");

        pageLayout->addWidget(typeTextLabel);

        typesButtonLayout = new QGridLayout();
        typesButtonLayout->setObjectName("typesButtonLayout");

        pageLayout->addLayout(typesButtonLayout);

        bottomSpacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        pageLayout->addItem(bottomSpacer);


        retranslateUi(PollWizardTypePage);

        QMetaObject::connectSlotsByName(PollWizardTypePage);
    } // setupUi

    void retranslateUi(QWizardPage *PollWizardTypePage)
    {
        pageTitleLabel->setText(QCoreApplication::translate("PollWizardTypePage", "Create a Poll", nullptr));
        wikiBlurbLabel->setText(QCoreApplication::translate("PollWizardTypePage", "The Gridcoin community established guidelines for polls with requirements for each type. Please read the wiki for more information:", nullptr));
        typeTextLabel->setText(QCoreApplication::translate("PollWizardTypePage", "Choose a poll type:", nullptr));
        (void)PollWizardTypePage;
    } // retranslateUi

};

namespace Ui {
    class PollWizardTypePage: public Ui_PollWizardTypePage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLWIZARDTYPEPAGE_H
