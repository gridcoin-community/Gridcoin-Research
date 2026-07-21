/********************************************************************************
** Form generated from reading UI file 'researcherwizardpoolsummarypage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDPOOLSUMMARYPAGE_H
#define UI_RESEARCHERWIZARDPOOLSUMMARYPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardPoolSummaryPage
{
public:
    QVBoxLayout *poolSummaryPageLayout;
    QSpacerItem *headerSpacer;
    QLabel *poolIconLabel;
    QLabel *headerLabel;
    QHBoxLayout *horizontalLayout;
    QWidget *infoGridLayoutWidget;
    QGridLayout *infoGridLayout;
    QLabel *boincPathLabelLabel;
    QLabel *boincPathLabel;
    QLabel *poolStatusLabelLabel;
    QHBoxLayout *poolStatusHorizontalLayout;
    QLabel *poolStatusLabel;
    QLabel *poolStatusIconLabel;
    QPushButton *refreshButton;
    QTableView *projectTableView;

    void setupUi(QWizardPage *ResearcherWizardPoolSummaryPage)
    {
        if (ResearcherWizardPoolSummaryPage->objectName().isEmpty())
            ResearcherWizardPoolSummaryPage->setObjectName("ResearcherWizardPoolSummaryPage");
        ResearcherWizardPoolSummaryPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardPoolSummaryPage->sizePolicy().hasHeightForWidth());
        ResearcherWizardPoolSummaryPage->setSizePolicy(sizePolicy);
        poolSummaryPageLayout = new QVBoxLayout(ResearcherWizardPoolSummaryPage);
        poolSummaryPageLayout->setObjectName("poolSummaryPageLayout");
        headerSpacer = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        poolSummaryPageLayout->addItem(headerSpacer);

        poolIconLabel = new QLabel(ResearcherWizardPoolSummaryPage);
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

        poolSummaryPageLayout->addWidget(poolIconLabel, 0, Qt::AlignHCenter);

        headerLabel = new QLabel(ResearcherWizardPoolSummaryPage);
        headerLabel->setObjectName("headerLabel");
        headerLabel->setAlignment(Qt::AlignCenter);
        headerLabel->setMargin(16);

        poolSummaryPageLayout->addWidget(headerLabel);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        infoGridLayoutWidget = new QWidget(ResearcherWizardPoolSummaryPage);
        infoGridLayoutWidget->setObjectName("infoGridLayoutWidget");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(infoGridLayoutWidget->sizePolicy().hasHeightForWidth());
        infoGridLayoutWidget->setSizePolicy(sizePolicy2);
        infoGridLayout = new QGridLayout(infoGridLayoutWidget);
        infoGridLayout->setObjectName("infoGridLayout");
        infoGridLayout->setContentsMargins(0, 0, 0, 0);
        boincPathLabelLabel = new QLabel(infoGridLayoutWidget);
        boincPathLabelLabel->setObjectName("boincPathLabelLabel");

        infoGridLayout->addWidget(boincPathLabelLabel, 0, 0, 1, 1);

        boincPathLabel = new QLabel(infoGridLayoutWidget);
        boincPathLabel->setObjectName("boincPathLabel");
        boincPathLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        boincPathLabel->setText(QString::fromUtf8(""));
        boincPathLabel->setWordWrap(true);
        boincPathLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        infoGridLayout->addWidget(boincPathLabel, 0, 1, 1, 1);

        poolStatusLabelLabel = new QLabel(infoGridLayoutWidget);
        poolStatusLabelLabel->setObjectName("poolStatusLabelLabel");

        infoGridLayout->addWidget(poolStatusLabelLabel, 1, 0, 1, 1);

        poolStatusHorizontalLayout = new QHBoxLayout();
        poolStatusHorizontalLayout->setObjectName("poolStatusHorizontalLayout");
        poolStatusLabel = new QLabel(infoGridLayoutWidget);
        poolStatusLabel->setObjectName("poolStatusLabel");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Preferred);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(poolStatusLabel->sizePolicy().hasHeightForWidth());
        poolStatusLabel->setSizePolicy(sizePolicy3);
        poolStatusLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        poolStatusLabel->setText(QString::fromUtf8(""));
        poolStatusLabel->setWordWrap(false);
        poolStatusLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        poolStatusHorizontalLayout->addWidget(poolStatusLabel);

        poolStatusIconLabel = new QLabel(infoGridLayoutWidget);
        poolStatusIconLabel->setObjectName("poolStatusIconLabel");
        sizePolicy1.setHeightForWidth(poolStatusIconLabel->sizePolicy().hasHeightForWidth());
        poolStatusIconLabel->setSizePolicy(sizePolicy1);
        poolStatusIconLabel->setMinimumSize(QSize(16, 16));
        poolStatusIconLabel->setMaximumSize(QSize(16, 16));
        poolStatusIconLabel->setText(QString::fromUtf8(""));
        poolStatusIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/white_and_red_x")));
        poolStatusIconLabel->setScaledContents(true);
        poolStatusIconLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        poolStatusIconLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        poolStatusHorizontalLayout->addWidget(poolStatusIconLabel, 0, Qt::AlignLeft|Qt::AlignTop);


        infoGridLayout->addLayout(poolStatusHorizontalLayout, 1, 1, 1, 1);

        infoGridLayout->setColumnStretch(1, 1);

        horizontalLayout->addWidget(infoGridLayoutWidget);

        refreshButton = new QPushButton(ResearcherWizardPoolSummaryPage);
        refreshButton->setObjectName("refreshButton");
        QSizePolicy sizePolicy4(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(refreshButton->sizePolicy().hasHeightForWidth());
        refreshButton->setSizePolicy(sizePolicy4);
        refreshButton->setFlat(false);

        horizontalLayout->addWidget(refreshButton);


        poolSummaryPageLayout->addLayout(horizontalLayout);

        projectTableView = new QTableView(ResearcherWizardPoolSummaryPage);
        projectTableView->setObjectName("projectTableView");
        projectTableView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        projectTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        projectTableView->setProperty("showDropIndicator", QVariant(false));
        projectTableView->setDragEnabled(false);
        projectTableView->setDragDropMode(QAbstractItemView::NoDragDrop);
        projectTableView->setDefaultDropAction(Qt::IgnoreAction);
        projectTableView->setAlternatingRowColors(true);
        projectTableView->setSelectionMode(QAbstractItemView::SingleSelection);
        projectTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        projectTableView->setShowGrid(false);
        projectTableView->setGridStyle(Qt::SolidLine);
        projectTableView->setSortingEnabled(true);
        projectTableView->setCornerButtonEnabled(false);
        projectTableView->horizontalHeader()->setHighlightSections(false);
        projectTableView->horizontalHeader()->setProperty("showSortIndicator", QVariant(true));
        projectTableView->horizontalHeader()->setStretchLastSection(true);
        projectTableView->verticalHeader()->setVisible(false);
        projectTableView->verticalHeader()->setProperty("showSortIndicator", QVariant(true));

        poolSummaryPageLayout->addWidget(projectTableView);


        retranslateUi(ResearcherWizardPoolSummaryPage);

        QMetaObject::connectSlotsByName(ResearcherWizardPoolSummaryPage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardPoolSummaryPage)
    {
        ResearcherWizardPoolSummaryPage->setWindowTitle(QCoreApplication::translate("ResearcherWizardPoolSummaryPage", "BOINC CPID Detection", nullptr));
        poolIconLabel->setText(QString());
        headerLabel->setText(QCoreApplication::translate("ResearcherWizardPoolSummaryPage", "Pool Mode", nullptr));
        boincPathLabelLabel->setText(QCoreApplication::translate("ResearcherWizardPoolSummaryPage", "BOINC Folder:", nullptr));
#if QT_CONFIG(tooltip)
        boincPathLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        poolStatusLabelLabel->setText(QCoreApplication::translate("ResearcherWizardPoolSummaryPage", "Pool Status:", nullptr));
#if QT_CONFIG(tooltip)
        poolStatusLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        refreshButton->setToolTip(QCoreApplication::translate("ResearcherWizardPoolSummaryPage", "Re-scan the BOINC projects on your computer.", nullptr));
#endif // QT_CONFIG(tooltip)
        refreshButton->setText(QCoreApplication::translate("ResearcherWizardPoolSummaryPage", "&Refresh", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardPoolSummaryPage: public Ui_ResearcherWizardPoolSummaryPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDPOOLSUMMARYPAGE_H
