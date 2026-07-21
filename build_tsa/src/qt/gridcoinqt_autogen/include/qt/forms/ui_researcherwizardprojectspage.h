/********************************************************************************
** Form generated from reading UI file 'researcherwizardprojectspage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESEARCHERWIZARDPROJECTSPAGE_H
#define UI_RESEARCHERWIZARDPROJECTSPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QWidget>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class Ui_ResearcherWizardProjectsPage
{
public:
    QGridLayout *gridLayout;
    QTableView *projectTableView;
    QHBoxLayout *horizontalLayout;
    QWidget *selectedCpidLayoutWidget;
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

    void setupUi(QWizardPage *ResearcherWizardProjectsPage)
    {
        if (ResearcherWizardProjectsPage->objectName().isEmpty())
            ResearcherWizardProjectsPage->setObjectName("ResearcherWizardProjectsPage");
        ResearcherWizardProjectsPage->resize(630, 480);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ResearcherWizardProjectsPage->sizePolicy().hasHeightForWidth());
        ResearcherWizardProjectsPage->setSizePolicy(sizePolicy);
        gridLayout = new QGridLayout(ResearcherWizardProjectsPage);
        gridLayout->setObjectName("gridLayout");
        projectTableView = new QTableView(ResearcherWizardProjectsPage);
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

        gridLayout->addWidget(projectTableView, 1, 0, 1, 1);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        selectedCpidLayoutWidget = new QWidget(ResearcherWizardProjectsPage);
        selectedCpidLayoutWidget->setObjectName("selectedCpidLayoutWidget");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(selectedCpidLayoutWidget->sizePolicy().hasHeightForWidth());
        selectedCpidLayoutWidget->setSizePolicy(sizePolicy1);
        infoGridLayout = new QGridLayout(selectedCpidLayoutWidget);
        infoGridLayout->setObjectName("infoGridLayout");
        infoGridLayout->setContentsMargins(0, 0, 0, 0);
        emailLabelLabel = new QLabel(selectedCpidLayoutWidget);
        emailLabelLabel->setObjectName("emailLabelLabel");

        infoGridLayout->addWidget(emailLabelLabel, 0, 0, 1, 1);

        emailLabel = new QLabel(selectedCpidLayoutWidget);
        emailLabel->setObjectName("emailLabel");
        emailLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        emailLabel->setText(QString::fromUtf8(""));
        emailLabel->setWordWrap(true);
        emailLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        infoGridLayout->addWidget(emailLabel, 0, 1, 1, 1);

        boincPathLabelLabel = new QLabel(selectedCpidLayoutWidget);
        boincPathLabelLabel->setObjectName("boincPathLabelLabel");

        infoGridLayout->addWidget(boincPathLabelLabel, 1, 0, 1, 1);

        boincPathLabel = new QLabel(selectedCpidLayoutWidget);
        boincPathLabel->setObjectName("boincPathLabel");
        boincPathLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        boincPathLabel->setText(QString::fromUtf8(""));
        boincPathLabel->setWordWrap(true);
        boincPathLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        infoGridLayout->addWidget(boincPathLabel, 1, 1, 1, 1);

        selectedCpidLabelLabel = new QLabel(selectedCpidLayoutWidget);
        selectedCpidLabelLabel->setObjectName("selectedCpidLabelLabel");

        infoGridLayout->addWidget(selectedCpidLabelLabel, 2, 0, 1, 1);

        selectedCpidhorizontalLayout = new QHBoxLayout();
        selectedCpidhorizontalLayout->setObjectName("selectedCpidhorizontalLayout");
        selectedCpidLabel = new QLabel(selectedCpidLayoutWidget);
        selectedCpidLabel->setObjectName("selectedCpidLabel");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Preferred);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(selectedCpidLabel->sizePolicy().hasHeightForWidth());
        selectedCpidLabel->setSizePolicy(sizePolicy2);
        selectedCpidLabel->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        selectedCpidLabel->setText(QString::fromUtf8(""));
        selectedCpidLabel->setWordWrap(true);
        selectedCpidLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        selectedCpidhorizontalLayout->addWidget(selectedCpidLabel);

        selectedCpidIconLabel = new QLabel(selectedCpidLayoutWidget);
        selectedCpidIconLabel->setObjectName("selectedCpidIconLabel");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(selectedCpidIconLabel->sizePolicy().hasHeightForWidth());
        selectedCpidIconLabel->setSizePolicy(sizePolicy3);
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

        horizontalLayout->addWidget(selectedCpidLayoutWidget);

        refreshButton = new QPushButton(ResearcherWizardProjectsPage);
        refreshButton->setObjectName("refreshButton");
        QSizePolicy sizePolicy4(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(refreshButton->sizePolicy().hasHeightForWidth());
        refreshButton->setSizePolicy(sizePolicy4);
        refreshButton->setFlat(false);

        horizontalLayout->addWidget(refreshButton);


        gridLayout->addLayout(horizontalLayout, 0, 0, 1, 1);


        retranslateUi(ResearcherWizardProjectsPage);

        QMetaObject::connectSlotsByName(ResearcherWizardProjectsPage);
    } // setupUi

    void retranslateUi(QWizardPage *ResearcherWizardProjectsPage)
    {
        ResearcherWizardProjectsPage->setWindowTitle(QCoreApplication::translate("ResearcherWizardProjectsPage", "BOINC CPID Detection", nullptr));
        ResearcherWizardProjectsPage->setTitle(QCoreApplication::translate("ResearcherWizardProjectsPage", "BOINC CPID Detection", nullptr));
        ResearcherWizardProjectsPage->setSubTitle(QCoreApplication::translate("ResearcherWizardProjectsPage", "Gridcoin scans the BOINC projects on your computer to find an eligible cross-project identifier (CPID). The network tracks CPIDs to allocate research rewards.", nullptr));
        emailLabelLabel->setText(QCoreApplication::translate("ResearcherWizardProjectsPage", "Email Address:", nullptr));
#if QT_CONFIG(tooltip)
        emailLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        boincPathLabelLabel->setText(QCoreApplication::translate("ResearcherWizardProjectsPage", "BOINC Folder:", nullptr));
#if QT_CONFIG(tooltip)
        boincPathLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        selectedCpidLabelLabel->setText(QCoreApplication::translate("ResearcherWizardProjectsPage", "Selected CPID:", nullptr));
#if QT_CONFIG(tooltip)
        selectedCpidLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        refreshButton->setToolTip(QCoreApplication::translate("ResearcherWizardProjectsPage", "Re-scan the BOINC projects on your computer.", nullptr));
#endif // QT_CONFIG(tooltip)
        refreshButton->setText(QCoreApplication::translate("ResearcherWizardProjectsPage", "&Refresh", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResearcherWizardProjectsPage: public Ui_ResearcherWizardProjectsPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESEARCHERWIZARDPROJECTSPAGE_H
