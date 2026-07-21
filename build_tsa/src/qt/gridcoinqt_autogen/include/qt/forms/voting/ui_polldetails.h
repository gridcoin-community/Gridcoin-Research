/********************************************************************************
** Form generated from reading UI file 'polldetails.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLDETAILS_H
#define UI_POLLDETAILS_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "voting/additionalfieldstableview.h"

QT_BEGIN_NAMESPACE

class Ui_PollDetails
{
public:
    QVBoxLayout *pollDetailsLayout;
    QLabel *dateRangeLabel;
    QLabel *titleLabel;
    QLabel *questionLabel;
    QHBoxLayout *urlLayout;
    QLabel *linkIconLabel;
    QLabel *urlLabel;
    QLabel *additionalFieldsLabel;
    AdditionalFieldsTableView *additionalFieldsTableView;
    QHBoxLayout *topAnswerLayout;
    QLabel *topAnswerTextLabel;
    QLabel *topAnswerLabel;

    void setupUi(QWidget *PollDetails)
    {
        if (PollDetails->objectName().isEmpty())
            PollDetails->setObjectName("PollDetails");
        PollDetails->resize(599, 312);
        pollDetailsLayout = new QVBoxLayout(PollDetails);
        pollDetailsLayout->setObjectName("pollDetailsLayout");
        pollDetailsLayout->setContentsMargins(0, 0, 0, 0);
        dateRangeLabel = new QLabel(PollDetails);
        dateRangeLabel->setObjectName("dateRangeLabel");
        dateRangeLabel->setText(QString::fromUtf8("Date Range"));
        dateRangeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        pollDetailsLayout->addWidget(dateRangeLabel);

        titleLabel = new QLabel(PollDetails);
        titleLabel->setObjectName("titleLabel");
        titleLabel->setText(QString::fromUtf8("Title"));
        titleLabel->setTextFormat(Qt::PlainText);
        titleLabel->setWordWrap(true);
        titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        pollDetailsLayout->addWidget(titleLabel);

        questionLabel = new QLabel(PollDetails);
        questionLabel->setObjectName("questionLabel");
        questionLabel->setText(QString::fromUtf8("Question"));
        questionLabel->setTextFormat(Qt::PlainText);
        questionLabel->setWordWrap(true);
        questionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        pollDetailsLayout->addWidget(questionLabel);

        urlLayout = new QHBoxLayout();
        urlLayout->setObjectName("urlLayout");
        linkIconLabel = new QLabel(PollDetails);
        linkIconLabel->setObjectName("linkIconLabel");

        urlLayout->addWidget(linkIconLabel);

        urlLabel = new QLabel(PollDetails);
        urlLabel->setObjectName("urlLabel");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(urlLabel->sizePolicy().hasHeightForWidth());
        urlLabel->setSizePolicy(sizePolicy);
        urlLabel->setText(QString::fromUtf8("URL"));
        urlLabel->setTextFormat(Qt::RichText);
        urlLabel->setOpenExternalLinks(true);
        urlLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

        urlLayout->addWidget(urlLabel);


        pollDetailsLayout->addLayout(urlLayout);

        additionalFieldsLabel = new QLabel(PollDetails);
        additionalFieldsLabel->setObjectName("additionalFieldsLabel");

        pollDetailsLayout->addWidget(additionalFieldsLabel);

        additionalFieldsTableView = new AdditionalFieldsTableView(PollDetails);
        additionalFieldsTableView->setObjectName("additionalFieldsTableView");

        pollDetailsLayout->addWidget(additionalFieldsTableView);

        topAnswerLayout = new QHBoxLayout();
        topAnswerLayout->setObjectName("topAnswerLayout");
        topAnswerLayout->setContentsMargins(-1, 0, -1, -1);
        topAnswerTextLabel = new QLabel(PollDetails);
        topAnswerTextLabel->setObjectName("topAnswerTextLabel");

        topAnswerLayout->addWidget(topAnswerTextLabel);

        topAnswerLabel = new QLabel(PollDetails);
        topAnswerLabel->setObjectName("topAnswerLabel");
        sizePolicy.setHeightForWidth(topAnswerLabel->sizePolicy().hasHeightForWidth());
        topAnswerLabel->setSizePolicy(sizePolicy);
        topAnswerLabel->setTextFormat(Qt::PlainText);
        topAnswerLabel->setWordWrap(true);
        topAnswerLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        topAnswerLayout->addWidget(topAnswerLabel);


        pollDetailsLayout->addLayout(topAnswerLayout);


        retranslateUi(PollDetails);

        QMetaObject::connectSlotsByName(PollDetails);
    } // setupUi

    void retranslateUi(QWidget *PollDetails)
    {
        PollDetails->setWindowTitle(QCoreApplication::translate("PollDetails", "Form", nullptr));
        additionalFieldsLabel->setText(QCoreApplication::translate("PollDetails", "Additional Fields", nullptr));
        topAnswerTextLabel->setText(QCoreApplication::translate("PollDetails", "Top Answer:", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PollDetails: public Ui_PollDetails {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLDETAILS_H
