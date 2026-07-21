/********************************************************************************
** Form generated from reading UI file 'pollcard.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POLLCARD_H
#define UI_POLLCARD_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_PollCard
{
public:
    QVBoxLayout *pollCardLayout;
    QFrame *detailsFrame;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout_2;
    QLabel *titleLabel;
    QSpacerItem *horizontalSpacer_3;
    QLabel *typeLabel;
    QGridLayout *gridLayout;
    QLabel *totalWeightTextLabel;
    QLabel *votePercentAVWLabel;
    QLabel *topAnswerLabel;
    QLabel *topAnswerTextLabel;
    QLabel *activeVoteWeightTextLabel;
    QLabel *votePercentAVWTextLabel;
    QLabel *voteCountTextLabel;
    QLabel *totalWeightLabel;
    QLabel *expirationTextLabel;
    QLabel *voteCountLabel;
    QLabel *expirationLabel;
    QLabel *activeVoteWeightLabel;
    QLabel *myLastVoteAnswerLabel;
    QLabel *myLastVoteAnswerTextLabel;
    QLabel *myVoteWeightTextLabel;
    QLabel *myVoteWeightLabel;
    QLabel *myPercentAVWTextLabel;
    QLabel *myPercentAVWLabel;
    QFrame *buttonFrame;
    QHBoxLayout *horizontalLayout;
    QLabel *balanceLabel;
    QLabel *magnitudeLabel;
    QSpacerItem *horizontalSpacer_2;
    QLabel *staleLabel;
    QLabel *validatedLabel;
    QLabel *invalidLabel;
    QSpacerItem *horizontalSpacer;
    QLabel *remainingLabel;
    QPushButton *voteButton;
    QPushButton *detailsButton;

    void setupUi(QWidget *PollCard)
    {
        if (PollCard->objectName().isEmpty())
            PollCard->setObjectName("PollCard");
        PollCard->resize(666, 146);
        pollCardLayout = new QVBoxLayout(PollCard);
        pollCardLayout->setSpacing(0);
        pollCardLayout->setObjectName("pollCardLayout");
        pollCardLayout->setContentsMargins(0, 0, 0, 0);
        detailsFrame = new QFrame(PollCard);
        detailsFrame->setObjectName("detailsFrame");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(detailsFrame->sizePolicy().hasHeightForWidth());
        detailsFrame->setSizePolicy(sizePolicy);
        detailsFrame->setFrameShape(QFrame::StyledPanel);
        detailsFrame->setFrameShadow(QFrame::Raised);
        verticalLayout_2 = new QVBoxLayout(detailsFrame);
        verticalLayout_2->setSpacing(16);
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        titleLabel = new QLabel(detailsFrame);
        titleLabel->setObjectName("titleLabel");
        titleLabel->setText(QString::fromUtf8("Title"));
        titleLabel->setTextFormat(Qt::PlainText);
        titleLabel->setWordWrap(true);

        horizontalLayout_2->addWidget(titleLabel);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer_3);

        typeLabel = new QLabel(detailsFrame);
        typeLabel->setObjectName("typeLabel");

        horizontalLayout_2->addWidget(typeLabel);


        verticalLayout_2->addLayout(horizontalLayout_2);

        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        gridLayout->setVerticalSpacing(3);
        totalWeightTextLabel = new QLabel(detailsFrame);
        totalWeightTextLabel->setObjectName("totalWeightTextLabel");

        gridLayout->addWidget(totalWeightTextLabel, 1, 2, 1, 1);

        votePercentAVWLabel = new QLabel(detailsFrame);
        votePercentAVWLabel->setObjectName("votePercentAVWLabel");

        gridLayout->addWidget(votePercentAVWLabel, 1, 5, 1, 1);

        topAnswerLabel = new QLabel(detailsFrame);
        topAnswerLabel->setObjectName("topAnswerLabel");
        topAnswerLabel->setText(QString::fromUtf8(""));
        topAnswerLabel->setTextFormat(Qt::PlainText);
        topAnswerLabel->setWordWrap(true);

        gridLayout->addWidget(topAnswerLabel, 1, 1, 1, 1);

        topAnswerTextLabel = new QLabel(detailsFrame);
        topAnswerTextLabel->setObjectName("topAnswerTextLabel");

        gridLayout->addWidget(topAnswerTextLabel, 1, 0, 1, 1);

        activeVoteWeightTextLabel = new QLabel(detailsFrame);
        activeVoteWeightTextLabel->setObjectName("activeVoteWeightTextLabel");

        gridLayout->addWidget(activeVoteWeightTextLabel, 0, 4, 1, 1);

        votePercentAVWTextLabel = new QLabel(detailsFrame);
        votePercentAVWTextLabel->setObjectName("votePercentAVWTextLabel");

        gridLayout->addWidget(votePercentAVWTextLabel, 1, 4, 1, 1);

        voteCountTextLabel = new QLabel(detailsFrame);
        voteCountTextLabel->setObjectName("voteCountTextLabel");

        gridLayout->addWidget(voteCountTextLabel, 0, 2, 1, 1);

        totalWeightLabel = new QLabel(detailsFrame);
        totalWeightLabel->setObjectName("totalWeightLabel");
        totalWeightLabel->setText(QString::fromUtf8(""));

        gridLayout->addWidget(totalWeightLabel, 1, 3, 1, 1);

        expirationTextLabel = new QLabel(detailsFrame);
        expirationTextLabel->setObjectName("expirationTextLabel");

        gridLayout->addWidget(expirationTextLabel, 0, 0, 1, 1);

        voteCountLabel = new QLabel(detailsFrame);
        voteCountLabel->setObjectName("voteCountLabel");
        voteCountLabel->setText(QString::fromUtf8(""));

        gridLayout->addWidget(voteCountLabel, 0, 3, 1, 1);

        expirationLabel = new QLabel(detailsFrame);
        expirationLabel->setObjectName("expirationLabel");
        expirationLabel->setText(QString::fromUtf8(""));

        gridLayout->addWidget(expirationLabel, 0, 1, 1, 1);

        activeVoteWeightLabel = new QLabel(detailsFrame);
        activeVoteWeightLabel->setObjectName("activeVoteWeightLabel");

        gridLayout->addWidget(activeVoteWeightLabel, 0, 5, 1, 1);

        myLastVoteAnswerLabel = new QLabel(detailsFrame);
        myLastVoteAnswerLabel->setObjectName("myLastVoteAnswerLabel");

        gridLayout->addWidget(myLastVoteAnswerLabel, 2, 1, 1, 1);

        myLastVoteAnswerTextLabel = new QLabel(detailsFrame);
        myLastVoteAnswerTextLabel->setObjectName("myLastVoteAnswerTextLabel");

        gridLayout->addWidget(myLastVoteAnswerTextLabel, 2, 0, 1, 1);

        myVoteWeightTextLabel = new QLabel(detailsFrame);
        myVoteWeightTextLabel->setObjectName("myVoteWeightTextLabel");

        gridLayout->addWidget(myVoteWeightTextLabel, 2, 2, 1, 1);

        myVoteWeightLabel = new QLabel(detailsFrame);
        myVoteWeightLabel->setObjectName("myVoteWeightLabel");

        gridLayout->addWidget(myVoteWeightLabel, 2, 3, 1, 1);

        myPercentAVWTextLabel = new QLabel(detailsFrame);
        myPercentAVWTextLabel->setObjectName("myPercentAVWTextLabel");

        gridLayout->addWidget(myPercentAVWTextLabel, 2, 4, 1, 1);

        myPercentAVWLabel = new QLabel(detailsFrame);
        myPercentAVWLabel->setObjectName("myPercentAVWLabel");

        gridLayout->addWidget(myPercentAVWLabel, 2, 5, 1, 1);

        gridLayout->setColumnStretch(1, 10000);
        gridLayout->setColumnStretch(3, 10000);
        gridLayout->setColumnStretch(5, 10000);

        verticalLayout_2->addLayout(gridLayout);


        pollCardLayout->addWidget(detailsFrame);

        buttonFrame = new QFrame(PollCard);
        buttonFrame->setObjectName("buttonFrame");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Minimum);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(buttonFrame->sizePolicy().hasHeightForWidth());
        buttonFrame->setSizePolicy(sizePolicy1);
        buttonFrame->setFrameShape(QFrame::StyledPanel);
        buttonFrame->setFrameShadow(QFrame::Raised);
        buttonFrame->setProperty("buttonFrame", QVariant(true));
        horizontalLayout = new QHBoxLayout(buttonFrame);
        horizontalLayout->setSpacing(9);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        balanceLabel = new QLabel(buttonFrame);
        balanceLabel->setObjectName("balanceLabel");

        horizontalLayout->addWidget(balanceLabel, 0, Qt::AlignVCenter);

        magnitudeLabel = new QLabel(buttonFrame);
        magnitudeLabel->setObjectName("magnitudeLabel");

        horizontalLayout->addWidget(magnitudeLabel, 0, Qt::AlignVCenter);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        staleLabel = new QLabel(buttonFrame);
        staleLabel->setObjectName("staleLabel");

        horizontalLayout->addWidget(staleLabel);

        validatedLabel = new QLabel(buttonFrame);
        validatedLabel->setObjectName("validatedLabel");

        horizontalLayout->addWidget(validatedLabel);

        invalidLabel = new QLabel(buttonFrame);
        invalidLabel->setObjectName("invalidLabel");

        horizontalLayout->addWidget(invalidLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        remainingLabel = new QLabel(buttonFrame);
        remainingLabel->setObjectName("remainingLabel");

        horizontalLayout->addWidget(remainingLabel, 0, Qt::AlignVCenter);

        voteButton = new QPushButton(buttonFrame);
        voteButton->setObjectName("voteButton");

        horizontalLayout->addWidget(voteButton);

        detailsButton = new QPushButton(buttonFrame);
        detailsButton->setObjectName("detailsButton");

        horizontalLayout->addWidget(detailsButton);


        pollCardLayout->addWidget(buttonFrame, 0, Qt::AlignBottom);


        retranslateUi(PollCard);

        QMetaObject::connectSlotsByName(PollCard);
    } // setupUi

    void retranslateUi(QWidget *PollCard)
    {
        PollCard->setWindowTitle(QCoreApplication::translate("PollCard", "Form", nullptr));
        typeLabel->setText(QCoreApplication::translate("PollCard", "Poll Type", nullptr));
        totalWeightTextLabel->setText(QCoreApplication::translate("PollCard", "Total Weight:", nullptr));
        votePercentAVWLabel->setText(QString());
        topAnswerTextLabel->setText(QCoreApplication::translate("PollCard", "Top Answer:", nullptr));
        activeVoteWeightTextLabel->setText(QCoreApplication::translate("PollCard", "AVW:", nullptr));
        votePercentAVWTextLabel->setText(QCoreApplication::translate("PollCard", "% of AVW:", nullptr));
        voteCountTextLabel->setText(QCoreApplication::translate("PollCard", "Votes:", nullptr));
        expirationTextLabel->setText(QCoreApplication::translate("PollCard", "Expiration:", nullptr));
        activeVoteWeightLabel->setText(QString());
        myLastVoteAnswerLabel->setText(QString());
        myLastVoteAnswerTextLabel->setText(QCoreApplication::translate("PollCard", "Your Vote(s):", nullptr));
        myVoteWeightTextLabel->setText(QCoreApplication::translate("PollCard", "Your Vote Weight(s):", nullptr));
        myVoteWeightLabel->setText(QString());
        myPercentAVWTextLabel->setText(QCoreApplication::translate("PollCard", "Your % of AVW:", nullptr));
        myPercentAVWLabel->setText(QString());
        balanceLabel->setText(QCoreApplication::translate("PollCard", "Balance", nullptr));
        magnitudeLabel->setText(QCoreApplication::translate("PollCard", "Magnitude", nullptr));
        staleLabel->setText(QCoreApplication::translate("PollCard", "Stale", nullptr));
        validatedLabel->setText(QCoreApplication::translate("PollCard", "Validated", nullptr));
        invalidLabel->setText(QCoreApplication::translate("PollCard", "Invalid", nullptr));
        remainingLabel->setText(QCoreApplication::translate("PollCard", "Voting finished.", nullptr));
        voteButton->setText(QCoreApplication::translate("PollCard", "Vote", nullptr));
        detailsButton->setText(QCoreApplication::translate("PollCard", "Details", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PollCard: public Ui_PollCard {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POLLCARD_H
