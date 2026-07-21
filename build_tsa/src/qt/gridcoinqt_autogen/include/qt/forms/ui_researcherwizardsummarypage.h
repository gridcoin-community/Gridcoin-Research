/********************************************************************************
** Form generated from reading UI file 'researcherwizardsummarypage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDSUMMARYPAGE_H
#define UI_RESEARCHERWIZARDSUMMARYPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardSummaryPage
{
public:
    QVBoxLayout *summaryPageVerticalLayout;
    QTabWidget *tabWidget;
    QWidget *summaryTab;
    QVBoxLayout *summaryTabVerticalLayout;
    QSpacerItem *headerTopSpacer;
    QLabel *cpidLabelLabel;
    QLabel *cpidLabel;
    QHBoxLayout *overallStatusLayout;
    QSpacerItem *overallStatusLeftSpacer;
    QLabel *overallStatusIconLabel;
    QLabel *overallStatusLabel;
    QSpacerItem *overallStatusRightSpacer;
    QPushButton *reviewBeaconAuthButton;
    QSpacerItem *headerBottomSpacer;
    QHBoxLayout *summaryTabDetailsLayout;
    QWidget *summaryDetailsWrapper;
    QVBoxLayout *summaryDetailsWrapperLayout;
    QLabel *summaryDetailsIconLabel;
    QFrame *summaryDetailsLine;
    QGridLayout *summaryDetailsLayout;
    QLabel *statusLabelLabel;
    QLabel *statusLabel;
    QLabel *magnitudeLabelLabel;
    QLabel *magnitudeLabel;
    QLabel *accrualLabelLabel;
    QLabel *accrualLabel;
    QSpacerItem *summaryDetailsFooterSpacer;
    QWidget *beaconDetailsWrapper;
    QVBoxLayout *beaconDetailsWrapperLayout;
    QLabel *beaconDetailsIconLabel;
    QFrame *beaconDetailsLine;
    QGridLayout *beaconDetailsLayout;
    QLabel *beaconStatusLabelLabel;
    QLabel *beaconStatusLabel;
    QLabel *beaconAgeLabelLabel;
    QLabel *beaconAgeLabel;
    QLabel *beaconExpiresLabelLabel;
    QLabel *beaconExpiresLabel;
    QLabel *rainAddressLabelLabel;
    QLabel *rainAddressLabel;
    QFrame *beaconButtonsLine;
    QPushButton *renewBeaconButton;
    QSpacerItem *beaconDetailsFooterSpacer;
    QSpacerItem *footerSpacer;
    QWidget *projectsTab;
    QVBoxLayout *projectsTabVerticalLayout;
    QHBoxLayout *headerHorizontalLayout;
    QWidget *infoGridLayoutWidget;
    QGridLayout *infoGridLayout;
    QLabel *emailLabelLabel;
    QLabel *emailLabel;
    QLabel *boincPathLabelLabel;
    QLabel *boincPathLabel;
    QLabel *selectedCpidLabelLabel;
    QHBoxLayout *selectedCpidhorizontalLayout;
    QLabel *selectedCpidLabel;
    QLabel *selectedCpidIconLabel;
    QPushButton *refreshButton;
    QTableView *projectTableView;

    void setupUi(QWizardPage *ResearcherWizardSummaryPage)
    {
        if (ResearcherWizardSummaryPage->objectName().isEmpty())
            ResearcherWizardSummaryPage->setObjectName("ResearcherWizardSummaryPage");
        ResearcherWizardSummaryPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardSummaryPage->sizePolicy().hasHeightForWidth());
        ResearcherWizardSummaryPage->setSizePolicy(sizePolicy);
        summaryPageVerticalLayout = new QVBoxLayout(ResearcherWizardSummaryPage);
        summaryPageVerticalLayout->setObjectName("summaryPageVerticalLayout");
        tabWidget = new QTabWidget(ResearcherWizardSummaryPage);
        tabWidget->setObjectName("tabWidget");
        tabWidget->setStyleSheet(QString::fromUtf8(""));
        summaryTab = new QWidget();
        summaryTab->setObjectName("summaryTab");
        sizePolicy.setHeightForWidth(summaryTab->sizePolicy().hasHeightForWidth());
        summaryTab->setSizePolicy(sizePolicy);
        summaryTabVerticalLayout = new QVBoxLayout(summaryTab);
        summaryTabVerticalLayout->setObjectName("summaryTabVerticalLayout");
        summaryTabVerticalLayout->setContentsMargins(0, -1, 0, 0);
        headerTopSpacer = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        summaryTabVerticalLayout->addItem(headerTopSpacer);

        cpidLabelLabel = new QLabel(summaryTab);
        cpidLabelLabel->setObjectName("cpidLabelLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(cpidLabelLabel->sizePolicy().hasHeightForWidth());
        cpidLabelLabel->setSizePolicy(sizePolicy1);
        cpidLabelLabel->setText(QString::fromUtf8("CPID"));
        cpidLabelLabel->setAlignment(Qt::AlignHCenter|Qt::AlignTop);

        summaryTabVerticalLayout->addWidget(cpidLabelLabel);

        cpidLabel = new QLabel(summaryTab);
        cpidLabel->setObjectName("cpidLabel");
        sizePolicy1.setHeightForWidth(cpidLabel->sizePolicy().hasHeightForWidth());
        cpidLabel->setSizePolicy(sizePolicy1);
        cpidLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        cpidLabel->setText(QString::fromUtf8(""));
        cpidLabel->setAlignment(Qt::AlignHCenter|Qt::AlignTop);
        cpidLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        summaryTabVerticalLayout->addWidget(cpidLabel);

        overallStatusLayout = new QHBoxLayout();
        overallStatusLayout->setObjectName("overallStatusLayout");
        overallStatusLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        overallStatusLayout->addItem(overallStatusLeftSpacer);

        overallStatusIconLabel = new QLabel(summaryTab);
        overallStatusIconLabel->setObjectName("overallStatusIconLabel");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(overallStatusIconLabel->sizePolicy().hasHeightForWidth());
        overallStatusIconLabel->setSizePolicy(sizePolicy2);
        overallStatusIconLabel->setMinimumSize(QSize(16, 16));
        overallStatusIconLabel->setMaximumSize(QSize(16, 16));
        overallStatusIconLabel->setText(QString::fromUtf8(""));
        overallStatusIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/round_green_check")));
        overallStatusIconLabel->setScaledContents(true);
        overallStatusIconLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        overallStatusLayout->addWidget(overallStatusIconLabel, 0, Qt::AlignHCenter|Qt::AlignVCenter);

        overallStatusLabel = new QLabel(summaryTab);
        overallStatusLabel->setObjectName("overallStatusLabel");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(overallStatusLabel->sizePolicy().hasHeightForWidth());
        overallStatusLabel->setSizePolicy(sizePolicy3);
        overallStatusLabel->setAlignment(Qt::AlignCenter);
        overallStatusLabel->setWordWrap(false);

        overallStatusLayout->addWidget(overallStatusLabel, 0, Qt::AlignLeft|Qt::AlignVCenter);

        overallStatusRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        overallStatusLayout->addItem(overallStatusRightSpacer);


        summaryTabVerticalLayout->addLayout(overallStatusLayout);

        reviewBeaconAuthButton = new QPushButton(summaryTab);
        reviewBeaconAuthButton->setObjectName("reviewBeaconAuthButton");
        sizePolicy2.setHeightForWidth(reviewBeaconAuthButton->sizePolicy().hasHeightForWidth());
        reviewBeaconAuthButton->setSizePolicy(sizePolicy2);

        summaryTabVerticalLayout->addWidget(reviewBeaconAuthButton, 0, Qt::AlignHCenter|Qt::AlignTop);

        headerBottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        summaryTabVerticalLayout->addItem(headerBottomSpacer);

        summaryTabDetailsLayout = new QHBoxLayout();
        summaryTabDetailsLayout->setObjectName("summaryTabDetailsLayout");
        summaryDetailsWrapper = new QWidget(summaryTab);
        summaryDetailsWrapper->setObjectName("summaryDetailsWrapper");
        QSizePolicy sizePolicy4(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy4.setHorizontalStretch(1);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(summaryDetailsWrapper->sizePolicy().hasHeightForWidth());
        summaryDetailsWrapper->setSizePolicy(sizePolicy4);
        summaryDetailsWrapper->setMinimumSize(QSize(300, 0));
        summaryDetailsWrapperLayout = new QVBoxLayout(summaryDetailsWrapper);
        summaryDetailsWrapperLayout->setSpacing(10);
        summaryDetailsWrapperLayout->setObjectName("summaryDetailsWrapperLayout");
        summaryDetailsWrapperLayout->setContentsMargins(0, 0, 0, 0);
        summaryDetailsIconLabel = new QLabel(summaryDetailsWrapper);
        summaryDetailsIconLabel->setObjectName("summaryDetailsIconLabel");
        sizePolicy2.setHeightForWidth(summaryDetailsIconLabel->sizePolicy().hasHeightForWidth());
        summaryDetailsIconLabel->setSizePolicy(sizePolicy2);
        summaryDetailsIconLabel->setMinimumSize(QSize(48, 48));
        summaryDetailsIconLabel->setMaximumSize(QSize(24, 24));
        summaryDetailsIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/ic_solo_active")));
        summaryDetailsIconLabel->setScaledContents(true);
        summaryDetailsIconLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        summaryDetailsWrapperLayout->addWidget(summaryDetailsIconLabel, 0, Qt::AlignHCenter|Qt::AlignTop);

        summaryDetailsLine = new QFrame(summaryDetailsWrapper);
        summaryDetailsLine->setObjectName("summaryDetailsLine");
        summaryDetailsLine->setFrameShape(QFrame::Shape::HLine);
        summaryDetailsLine->setFrameShadow(QFrame::Shadow::Sunken);

        summaryDetailsWrapperLayout->addWidget(summaryDetailsLine);

        summaryDetailsLayout = new QGridLayout();
        summaryDetailsLayout->setObjectName("summaryDetailsLayout");
        summaryDetailsLayout->setContentsMargins(6, -1, 0, -1);
        statusLabelLabel = new QLabel(summaryDetailsWrapper);
        statusLabelLabel->setObjectName("statusLabelLabel");

        summaryDetailsLayout->addWidget(statusLabelLabel, 0, 0, 1, 1);

        statusLabel = new QLabel(summaryDetailsWrapper);
        statusLabel->setObjectName("statusLabel");
        statusLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        statusLabel->setText(QString::fromUtf8(""));
        statusLabel->setWordWrap(true);
        statusLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        summaryDetailsLayout->addWidget(statusLabel, 0, 1, 1, 1);

        magnitudeLabelLabel = new QLabel(summaryDetailsWrapper);
        magnitudeLabelLabel->setObjectName("magnitudeLabelLabel");

        summaryDetailsLayout->addWidget(magnitudeLabelLabel, 1, 0, 1, 1);

        magnitudeLabel = new QLabel(summaryDetailsWrapper);
        magnitudeLabel->setObjectName("magnitudeLabel");
        magnitudeLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        magnitudeLabel->setText(QString::fromUtf8(""));
        magnitudeLabel->setWordWrap(true);
        magnitudeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        summaryDetailsLayout->addWidget(magnitudeLabel, 1, 1, 1, 1);

        accrualLabelLabel = new QLabel(summaryDetailsWrapper);
        accrualLabelLabel->setObjectName("accrualLabelLabel");

        summaryDetailsLayout->addWidget(accrualLabelLabel, 2, 0, 1, 1);

        accrualLabel = new QLabel(summaryDetailsWrapper);
        accrualLabel->setObjectName("accrualLabel");
        accrualLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        accrualLabel->setText(QString::fromUtf8(""));
        accrualLabel->setWordWrap(true);
        accrualLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        summaryDetailsLayout->addWidget(accrualLabel, 2, 1, 1, 1);

        summaryDetailsLayout->setColumnStretch(1, 1);

        summaryDetailsWrapperLayout->addLayout(summaryDetailsLayout);

        summaryDetailsFooterSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        summaryDetailsWrapperLayout->addItem(summaryDetailsFooterSpacer);


        summaryTabDetailsLayout->addWidget(summaryDetailsWrapper);

        beaconDetailsWrapper = new QWidget(summaryTab);
        beaconDetailsWrapper->setObjectName("beaconDetailsWrapper");
        sizePolicy4.setHeightForWidth(beaconDetailsWrapper->sizePolicy().hasHeightForWidth());
        beaconDetailsWrapper->setSizePolicy(sizePolicy4);
        beaconDetailsWrapper->setMinimumSize(QSize(300, 0));
        beaconDetailsWrapperLayout = new QVBoxLayout(beaconDetailsWrapper);
        beaconDetailsWrapperLayout->setSpacing(10);
        beaconDetailsWrapperLayout->setObjectName("beaconDetailsWrapperLayout");
        beaconDetailsWrapperLayout->setContentsMargins(0, 0, 0, 0);
        beaconDetailsIconLabel = new QLabel(beaconDetailsWrapper);
        beaconDetailsIconLabel->setObjectName("beaconDetailsIconLabel");
        sizePolicy2.setHeightForWidth(beaconDetailsIconLabel->sizePolicy().hasHeightForWidth());
        beaconDetailsIconLabel->setSizePolicy(sizePolicy2);
        beaconDetailsIconLabel->setMinimumSize(QSize(48, 48));
        beaconDetailsIconLabel->setMaximumSize(QSize(48, 48));
        beaconDetailsIconLabel->setScaledContents(true);
        beaconDetailsIconLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        beaconDetailsWrapperLayout->addWidget(beaconDetailsIconLabel, 0, Qt::AlignHCenter|Qt::AlignTop);

        beaconDetailsLine = new QFrame(beaconDetailsWrapper);
        beaconDetailsLine->setObjectName("beaconDetailsLine");
        beaconDetailsLine->setFrameShape(QFrame::Shape::HLine);
        beaconDetailsLine->setFrameShadow(QFrame::Shadow::Sunken);

        beaconDetailsWrapperLayout->addWidget(beaconDetailsLine);

        beaconDetailsLayout = new QGridLayout();
        beaconDetailsLayout->setObjectName("beaconDetailsLayout");
        beaconDetailsLayout->setContentsMargins(6, -1, 6, -1);
        beaconStatusLabelLabel = new QLabel(beaconDetailsWrapper);
        beaconStatusLabelLabel->setObjectName("beaconStatusLabelLabel");

        beaconDetailsLayout->addWidget(beaconStatusLabelLabel, 0, 0, 1, 1);

        beaconStatusLabel = new QLabel(beaconDetailsWrapper);
        beaconStatusLabel->setObjectName("beaconStatusLabel");
        beaconStatusLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        beaconStatusLabel->setText(QString::fromUtf8(""));
        beaconStatusLabel->setWordWrap(true);
        beaconStatusLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        beaconDetailsLayout->addWidget(beaconStatusLabel, 0, 1, 1, 1);

        beaconAgeLabelLabel = new QLabel(beaconDetailsWrapper);
        beaconAgeLabelLabel->setObjectName("beaconAgeLabelLabel");

        beaconDetailsLayout->addWidget(beaconAgeLabelLabel, 1, 0, 1, 1);

        beaconAgeLabel = new QLabel(beaconDetailsWrapper);
        beaconAgeLabel->setObjectName("beaconAgeLabel");
        beaconAgeLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        beaconAgeLabel->setText(QString::fromUtf8(""));
        beaconAgeLabel->setWordWrap(true);
        beaconAgeLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        beaconDetailsLayout->addWidget(beaconAgeLabel, 1, 1, 1, 1);

        beaconExpiresLabelLabel = new QLabel(beaconDetailsWrapper);
        beaconExpiresLabelLabel->setObjectName("beaconExpiresLabelLabel");

        beaconDetailsLayout->addWidget(beaconExpiresLabelLabel, 2, 0, 1, 1);

        beaconExpiresLabel = new QLabel(beaconDetailsWrapper);
        beaconExpiresLabel->setObjectName("beaconExpiresLabel");
        beaconExpiresLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        beaconExpiresLabel->setText(QString::fromUtf8(""));
        beaconExpiresLabel->setWordWrap(true);
        beaconExpiresLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        beaconDetailsLayout->addWidget(beaconExpiresLabel, 2, 1, 1, 1);

        rainAddressLabelLabel = new QLabel(beaconDetailsWrapper);
        rainAddressLabelLabel->setObjectName("rainAddressLabelLabel");

        beaconDetailsLayout->addWidget(rainAddressLabelLabel, 3, 0, 1, 1);

        rainAddressLabel = new QLabel(beaconDetailsWrapper);
        rainAddressLabel->setObjectName("rainAddressLabel");
        QFont font;
        font.setFamilies({QString::fromUtf8("Monospace")});
        rainAddressLabel->setFont(font);
        rainAddressLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        rainAddressLabel->setText(QString::fromUtf8(""));
        rainAddressLabel->setWordWrap(true);
        rainAddressLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        beaconDetailsLayout->addWidget(rainAddressLabel, 3, 1, 1, 1);

        beaconDetailsLayout->setColumnStretch(1, 1);

        beaconDetailsWrapperLayout->addLayout(beaconDetailsLayout);

        beaconButtonsLine = new QFrame(beaconDetailsWrapper);
        beaconButtonsLine->setObjectName("beaconButtonsLine");
        beaconButtonsLine->setFrameShape(QFrame::Shape::HLine);
        beaconButtonsLine->setFrameShadow(QFrame::Shadow::Sunken);

        beaconDetailsWrapperLayout->addWidget(beaconButtonsLine);

        renewBeaconButton = new QPushButton(beaconDetailsWrapper);
        renewBeaconButton->setObjectName("renewBeaconButton");
        sizePolicy2.setHeightForWidth(renewBeaconButton->sizePolicy().hasHeightForWidth());
        renewBeaconButton->setSizePolicy(sizePolicy2);

        beaconDetailsWrapperLayout->addWidget(renewBeaconButton);

        beaconDetailsFooterSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        beaconDetailsWrapperLayout->addItem(beaconDetailsFooterSpacer);


        summaryTabDetailsLayout->addWidget(beaconDetailsWrapper);


        summaryTabVerticalLayout->addLayout(summaryTabDetailsLayout);

        footerSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        summaryTabVerticalLayout->addItem(footerSpacer);

        tabWidget->addTab(summaryTab, QString());
        projectsTab = new QWidget();
        projectsTab->setObjectName("projectsTab");
        sizePolicy.setHeightForWidth(projectsTab->sizePolicy().hasHeightForWidth());
        projectsTab->setSizePolicy(sizePolicy);
        projectsTabVerticalLayout = new QVBoxLayout(projectsTab);
        projectsTabVerticalLayout->setObjectName("projectsTabVerticalLayout");
        headerHorizontalLayout = new QHBoxLayout();
        headerHorizontalLayout->setObjectName("headerHorizontalLayout");
        infoGridLayoutWidget = new QWidget(projectsTab);
        infoGridLayoutWidget->setObjectName("infoGridLayoutWidget");
        QSizePolicy sizePolicy5(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy5.setHorizontalStretch(0);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(infoGridLayoutWidget->sizePolicy().hasHeightForWidth());
        infoGridLayoutWidget->setSizePolicy(sizePolicy5);
        infoGridLayout = new QGridLayout(infoGridLayoutWidget);
        infoGridLayout->setObjectName("infoGridLayout");
        infoGridLayout->setContentsMargins(0, 0, 0, 0);
        emailLabelLabel = new QLabel(infoGridLayoutWidget);
        emailLabelLabel->setObjectName("emailLabelLabel");

        infoGridLayout->addWidget(emailLabelLabel, 0, 0, 1, 1);

        emailLabel = new QLabel(infoGridLayoutWidget);
        emailLabel->setObjectName("emailLabel");
        emailLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        emailLabel->setText(QString::fromUtf8(""));
        emailLabel->setWordWrap(true);
        emailLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        infoGridLayout->addWidget(emailLabel, 0, 1, 1, 1);

        boincPathLabelLabel = new QLabel(infoGridLayoutWidget);
        boincPathLabelLabel->setObjectName("boincPathLabelLabel");

        infoGridLayout->addWidget(boincPathLabelLabel, 1, 0, 1, 1);

        boincPathLabel = new QLabel(infoGridLayoutWidget);
        boincPathLabel->setObjectName("boincPathLabel");
        boincPathLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        boincPathLabel->setText(QString::fromUtf8(""));
        boincPathLabel->setWordWrap(true);
        boincPathLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        infoGridLayout->addWidget(boincPathLabel, 1, 1, 1, 1);

        selectedCpidLabelLabel = new QLabel(infoGridLayoutWidget);
        selectedCpidLabelLabel->setObjectName("selectedCpidLabelLabel");

        infoGridLayout->addWidget(selectedCpidLabelLabel, 2, 0, 1, 1);

        selectedCpidhorizontalLayout = new QHBoxLayout();
        selectedCpidhorizontalLayout->setObjectName("selectedCpidhorizontalLayout");
        selectedCpidLabel = new QLabel(infoGridLayoutWidget);
        selectedCpidLabel->setObjectName("selectedCpidLabel");
        QSizePolicy sizePolicy6(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Preferred);
        sizePolicy6.setHorizontalStretch(0);
        sizePolicy6.setVerticalStretch(0);
        sizePolicy6.setHeightForWidth(selectedCpidLabel->sizePolicy().hasHeightForWidth());
        selectedCpidLabel->setSizePolicy(sizePolicy6);
        selectedCpidLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        selectedCpidLabel->setText(QString::fromUtf8(""));
        selectedCpidLabel->setWordWrap(true);
        selectedCpidLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        selectedCpidhorizontalLayout->addWidget(selectedCpidLabel);

        selectedCpidIconLabel = new QLabel(infoGridLayoutWidget);
        selectedCpidIconLabel->setObjectName("selectedCpidIconLabel");
        sizePolicy2.setHeightForWidth(selectedCpidIconLabel->sizePolicy().hasHeightForWidth());
        selectedCpidIconLabel->setSizePolicy(sizePolicy2);
        selectedCpidIconLabel->setMinimumSize(QSize(16, 16));
        selectedCpidIconLabel->setMaximumSize(QSize(16, 16));
        selectedCpidIconLabel->setText(QString::fromUtf8(""));
        selectedCpidIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/white_and_red_x")));
        selectedCpidIconLabel->setScaledContents(true);
        selectedCpidIconLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        selectedCpidIconLabel->setTextInteractionFlags(Qt::NoTextInteraction);

        selectedCpidhorizontalLayout->addWidget(selectedCpidIconLabel, 0, Qt::AlignLeft|Qt::AlignTop);


        infoGridLayout->addLayout(selectedCpidhorizontalLayout, 2, 1, 1, 1);

        infoGridLayout->setColumnStretch(1, 1);

        headerHorizontalLayout->addWidget(infoGridLayoutWidget);

        refreshButton = new QPushButton(projectsTab);
        refreshButton->setObjectName("refreshButton");
        QSizePolicy sizePolicy7(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);
        sizePolicy7.setHorizontalStretch(0);
        sizePolicy7.setVerticalStretch(0);
        sizePolicy7.setHeightForWidth(refreshButton->sizePolicy().hasHeightForWidth());
        refreshButton->setSizePolicy(sizePolicy7);
        refreshButton->setFlat(false);

        headerHorizontalLayout->addWidget(refreshButton);


        projectsTabVerticalLayout->addLayout(headerHorizontalLayout);

        projectTableView = new QTableView(projectsTab);
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

        projectsTabVerticalLayout->addWidget(projectTableView);

        tabWidget->addTab(projectsTab, QString());

        summaryPageVerticalLayout->addWidget(tabWidget);


        retranslateUi(ResearcherWizardSummaryPage);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(ResearcherWizardSummaryPage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardSummaryPage)
    {
        ResearcherWizardSummaryPage->setWindowTitle(QCoreApplication::translate("ResearcherWizardSummaryPage", "Researcher Summary", nullptr));
        ResearcherWizardSummaryPage->setTitle(QString());
        ResearcherWizardSummaryPage->setSubTitle(QString());
        overallStatusLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Everything looks good.", nullptr));
        reviewBeaconAuthButton->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Review Beacon Verification", nullptr));
        summaryDetailsIconLabel->setText(QString());
        statusLabelLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Status:", nullptr));
#if QT_CONFIG(tooltip)
        statusLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        magnitudeLabelLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Magnitude:", nullptr));
#if QT_CONFIG(tooltip)
        magnitudeLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        accrualLabelLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Pending Reward:", nullptr));
#if QT_CONFIG(tooltip)
        accrualLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        beaconDetailsIconLabel->setText(QString());
        beaconStatusLabelLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Beacon:", nullptr));
#if QT_CONFIG(tooltip)
        beaconStatusLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        beaconAgeLabelLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Age:", nullptr));
#if QT_CONFIG(tooltip)
        beaconAgeLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        beaconExpiresLabelLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Expires:", nullptr));
#if QT_CONFIG(tooltip)
        beaconExpiresLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        rainAddressLabelLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Address:", nullptr));
#if QT_CONFIG(tooltip)
        rainAddressLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        renewBeaconButton->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "&Renew", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(summaryTab), QCoreApplication::translate("ResearcherWizardSummaryPage", "S&ummary", nullptr));
        emailLabelLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Email Address:", nullptr));
#if QT_CONFIG(tooltip)
        emailLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        boincPathLabelLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "BOINC Folder:", nullptr));
#if QT_CONFIG(tooltip)
        boincPathLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        selectedCpidLabelLabel->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "Selected CPID:", nullptr));
#if QT_CONFIG(tooltip)
        selectedCpidLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        refreshButton->setToolTip(QCoreApplication::translate("ResearcherWizardSummaryPage", "Re-scan the BOINC projects on your computer.", nullptr));
#endif // QT_CONFIG(tooltip)
        refreshButton->setText(QCoreApplication::translate("ResearcherWizardSummaryPage", "&Refresh", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(projectsTab), QCoreApplication::translate("ResearcherWizardSummaryPage", "&Projects", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardSummaryPage: public Ui_ResearcherWizardSummaryPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDSUMMARYPAGE_H
