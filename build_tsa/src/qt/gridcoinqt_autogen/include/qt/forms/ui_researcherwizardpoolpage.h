/********************************************************************************
** Form generated from reading UI file 'researcherwizardpoolpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDPOOLPAGE_H
#define UI_RESEARCHERWIZARDPOOLPAGE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardPoolPage
{
public:
    QVBoxLayout *poolPageLayout;
    QSpacerItem *headerSpacer;
    QLabel *poolIconLabel;
    QLabel *headerLabel;
    QLabel *poolParagraphLabel;
    QSpacerItem *poolTableTopSpacer;
    QTableWidget *poolTableWidget;
    QSpacerItem *poolTableBottomSpacer;
    QLabel *addressParagraphLabel;
    QHBoxLayout *addressLayout;
    QPushButton *newAddressButton;
    QLabel *addressLabel;
    QPushButton *copyToClipboardButton;
    QSpacerItem *horizontalSpacer;
    QLabel *startOverLabel;

    void setupUi(QWizardPage *ResearcherWizardPoolPage)
    {
        if (ResearcherWizardPoolPage->objectName().isEmpty())
            ResearcherWizardPoolPage->setObjectName("ResearcherWizardPoolPage");
        ResearcherWizardPoolPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardPoolPage->sizePolicy().hasHeightForWidth());
        ResearcherWizardPoolPage->setSizePolicy(sizePolicy);
        poolPageLayout = new QVBoxLayout(ResearcherWizardPoolPage);
        poolPageLayout->setObjectName("poolPageLayout");
        headerSpacer = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        poolPageLayout->addItem(headerSpacer);

        poolIconLabel = new QLabel(ResearcherWizardPoolPage);
        poolIconLabel->setObjectName("poolIconLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(poolIconLabel->sizePolicy().hasHeightForWidth());
        poolIconLabel->setSizePolicy(sizePolicy1);
        poolIconLabel->setMinimumSize(QSize(64, 64));
        poolIconLabel->setMaximumSize(QSize(64, 64));
        poolIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/ic_pool_active")));
        poolIconLabel->setScaledContents(true);
        poolIconLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        poolPageLayout->addWidget(poolIconLabel, 0, Qt::AlignHCenter);

        headerLabel = new QLabel(ResearcherWizardPoolPage);
        headerLabel->setObjectName("headerLabel");
        headerLabel->setAlignment(Qt::AlignCenter);
        headerLabel->setMargin(16);

        poolPageLayout->addWidget(headerLabel);

        poolParagraphLabel = new QLabel(ResearcherWizardPoolPage);
        poolParagraphLabel->setObjectName("poolParagraphLabel");
        poolParagraphLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        poolParagraphLabel->setWordWrap(true);

        poolPageLayout->addWidget(poolParagraphLabel, 0, Qt::AlignVCenter);

        poolTableTopSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        poolPageLayout->addItem(poolTableTopSpacer);

        poolTableWidget = new QTableWidget(ResearcherWizardPoolPage);
        if (poolTableWidget->columnCount() < 1)
            poolTableWidget->setColumnCount(1);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        poolTableWidget->setHorizontalHeaderItem(0, __qtablewidgetitem);
        if (poolTableWidget->rowCount() < 2)
            poolTableWidget->setRowCount(2);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        poolTableWidget->setVerticalHeaderItem(0, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        poolTableWidget->setVerticalHeaderItem(1, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        __qtablewidgetitem3->setText(QString::fromUtf8("https://grcpool.com/"));
        poolTableWidget->setItem(0, 0, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        __qtablewidgetitem4->setText(QString::fromUtf8("https://grc.arikado.ru/"));
        poolTableWidget->setItem(1, 0, __qtablewidgetitem4);
        poolTableWidget->setObjectName("poolTableWidget");
        poolTableWidget->setMaximumSize(QSize(16777215, 180));
        poolTableWidget->viewport()->setProperty("cursor", QVariant(QCursor(Qt::CursorShape::PointingHandCursor)));
        poolTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        poolTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
        poolTableWidget->setCornerButtonEnabled(false);
        poolTableWidget->horizontalHeader()->setVisible(false);
        poolTableWidget->horizontalHeader()->setStretchLastSection(true);
        poolTableWidget->verticalHeader()->setStretchLastSection(false);

        poolPageLayout->addWidget(poolTableWidget);

        poolTableBottomSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        poolPageLayout->addItem(poolTableBottomSpacer);

        addressParagraphLabel = new QLabel(ResearcherWizardPoolPage);
        addressParagraphLabel->setObjectName("addressParagraphLabel");
        addressParagraphLabel->setWordWrap(true);

        poolPageLayout->addWidget(addressParagraphLabel);

        addressLayout = new QHBoxLayout();
        addressLayout->setObjectName("addressLayout");
        newAddressButton = new QPushButton(ResearcherWizardPoolPage);
        newAddressButton->setObjectName("newAddressButton");
        sizePolicy1.setHeightForWidth(newAddressButton->sizePolicy().hasHeightForWidth());
        newAddressButton->setSizePolicy(sizePolicy1);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/add"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        newAddressButton->setIcon(icon);

        addressLayout->addWidget(newAddressButton, 0, Qt::AlignLeft);

        addressLabel = new QLabel(ResearcherWizardPoolPage);
        addressLabel->setObjectName("addressLabel");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Preferred);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(addressLabel->sizePolicy().hasHeightForWidth());
        addressLabel->setSizePolicy(sizePolicy2);
        QFont font;
        font.setFamilies({QString::fromUtf8("Monospace")});
        addressLabel->setFont(font);
        addressLabel->setText(QString::fromUtf8(""));
        addressLabel->setIndent(16);
        addressLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        addressLayout->addWidget(addressLabel, 0, Qt::AlignLeft);

        copyToClipboardButton = new QPushButton(ResearcherWizardPoolPage);
        copyToClipboardButton->setObjectName("copyToClipboardButton");
        sizePolicy1.setHeightForWidth(copyToClipboardButton->sizePolicy().hasHeightForWidth());
        copyToClipboardButton->setSizePolicy(sizePolicy1);

        addressLayout->addWidget(copyToClipboardButton, 0, Qt::AlignLeft);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        addressLayout->addItem(horizontalSpacer);


        poolPageLayout->addLayout(addressLayout);

        startOverLabel = new QLabel(ResearcherWizardPoolPage);
        startOverLabel->setObjectName("startOverLabel");
        startOverLabel->setAlignment(Qt::AlignCenter);
        startOverLabel->setMargin(4);

        poolPageLayout->addWidget(startOverLabel);


        retranslateUi(ResearcherWizardPoolPage);

        QMetaObject::connectSlotsByName(ResearcherWizardPoolPage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardPoolPage)
    {
        ResearcherWizardPoolPage->setWindowTitle(QCoreApplication::translate("ResearcherWizardPoolPage", "Summary", nullptr));
        poolIconLabel->setText(QString());
        headerLabel->setText(QCoreApplication::translate("ResearcherWizardPoolPage", "Pool Mode", nullptr));
        poolParagraphLabel->setText(QCoreApplication::translate("ResearcherWizardPoolPage", "In this mode, a pool will take care of staking research rewards for you. Your wallet can still earn standard staking rewards on your balance. You do not need a BOINC account, CPID, or beacon. Please choose a pool and follow the instructions on the website to sign up and connect the pool's account manager to BOINC:", nullptr));
        QTableWidgetItem *___qtablewidgetitem = poolTableWidget->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("ResearcherWizardPoolPage", "Website URL", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = poolTableWidget->verticalHeaderItem(0);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("ResearcherWizardPoolPage", "grcpool", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = poolTableWidget->verticalHeaderItem(1);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("ResearcherWizardPoolPage", "Arikado Pool", nullptr));

        const bool __sortingEnabled = poolTableWidget->isSortingEnabled();
        poolTableWidget->setSortingEnabled(false);
        poolTableWidget->setSortingEnabled(__sortingEnabled);

        addressParagraphLabel->setText(QCoreApplication::translate("ResearcherWizardPoolPage", "As you sign up, the pool may ask for a payment address to send earnings to. Press the button below to generate an address.", nullptr));
        newAddressButton->setText(QCoreApplication::translate("ResearcherWizardPoolPage", "New &Address", nullptr));
        copyToClipboardButton->setText(QCoreApplication::translate("ResearcherWizardPoolPage", "&Copy", nullptr));
        startOverLabel->setText(QCoreApplication::translate("ResearcherWizardPoolPage", "Press \"Next\" when you are done.", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardPoolPage: public Ui_ResearcherWizardPoolPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDPOOLPAGE_H
