/********************************************************************************
** Form generated from reading UI file 'mrcrequestpage.ui'
**
** Created by: Qt User Interface Compiler version 6.9.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MRCREQUESTPAGE_H
#define UI_MRCREQUESTPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include "bitcoinamountfield.h"

QT_BEGIN_NAMESPACE

class Ui_MRCRequestPage
{
public:
    QVBoxLayout *verticalLayout;
    QFrame *waitForNextBlockUpdateFrame;
    QVBoxLayout *verticalLayout_5;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QLabel *waitForBlockUpdateLabel;
    QLabel *waitForBlockUpdate;
    QSpacerItem *horizontalSpacer_2;
    QFrame *mrcStatusSubmitFrame;
    QVBoxLayout *verticalLayout_6;
    QGridLayout *gridLayout;
    QLabel *mrcQueuePayLimitFeeLabel;
    QLabel *mrcQueueTailFeeLabel;
    QLabel *mrcQueuePosition;
    QLabel *numMRCInQueueLabel;
    QLabel *numMRCInQueue;
    QLabel *mrcQueuePositionLabel;
    QLabel *mrcQueuePayLimitFee;
    QLabel *mrcQueueLimitLabel;
    QLabel *mrcMinimumSubmitFeeLabel;
    QLabel *mrcMinimumSubmitFee;
    QLabel *mrcQueueTailFee;
    QLabel *mrcQueueLimit;
    QLabel *mrcQueueHeadFee;
    QLabel *mrcQueueHeadFeeLabel;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *mrcFeeHorizontalLayout;
    QLabel *mrcFeeBoostLabel;
    QPushButton *mrcFeeBoostRaiseToMinimumButton;
    BitcoinAmountField *mrcFeeBoostSpinBox;
    QHBoxLayout *ButtonHorizontalLayout;
    QPushButton *mrcUpdateButton;
    QPushButton *mrcSubmitButton;
    QLabel *SubmittedIconLabel;
    QLabel *ErrorIconLabel;
    QDialogButtonBox *mrcRequestButtonBox;

    void setupUi(QDialog *MRCRequestPage)
    {
        if (MRCRequestPage->objectName().isEmpty())
            MRCRequestPage->setObjectName("MRCRequestPage");
        MRCRequestPage->resize(694, 580);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(MRCRequestPage->sizePolicy().hasHeightForWidth());
        MRCRequestPage->setSizePolicy(sizePolicy);
        verticalLayout = new QVBoxLayout(MRCRequestPage);
        verticalLayout->setObjectName("verticalLayout");
        waitForNextBlockUpdateFrame = new QFrame(MRCRequestPage);
        waitForNextBlockUpdateFrame->setObjectName("waitForNextBlockUpdateFrame");
        verticalLayout_5 = new QVBoxLayout(waitForNextBlockUpdateFrame);
        verticalLayout_5->setObjectName("verticalLayout_5");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        waitForBlockUpdateLabel = new QLabel(waitForNextBlockUpdateFrame);
        waitForBlockUpdateLabel->setObjectName("waitForBlockUpdateLabel");
        waitForBlockUpdateLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout->addWidget(waitForBlockUpdateLabel);

        waitForBlockUpdate = new QLabel(waitForNextBlockUpdateFrame);
        waitForBlockUpdate->setObjectName("waitForBlockUpdate");
        waitForBlockUpdate->setPixmap(QPixmap(QString::fromUtf8(":/icons/no_result")));

        horizontalLayout->addWidget(waitForBlockUpdate);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);


        verticalLayout_5->addLayout(horizontalLayout);


        verticalLayout->addWidget(waitForNextBlockUpdateFrame);

        mrcStatusSubmitFrame = new QFrame(MRCRequestPage);
        mrcStatusSubmitFrame->setObjectName("mrcStatusSubmitFrame");
        verticalLayout_6 = new QVBoxLayout(mrcStatusSubmitFrame);
        verticalLayout_6->setObjectName("verticalLayout_6");
        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        mrcQueuePayLimitFeeLabel = new QLabel(mrcStatusSubmitFrame);
        mrcQueuePayLimitFeeLabel->setObjectName("mrcQueuePayLimitFeeLabel");

        gridLayout->addWidget(mrcQueuePayLimitFeeLabel, 3, 0, 1, 1);

        mrcQueueTailFeeLabel = new QLabel(mrcStatusSubmitFrame);
        mrcQueueTailFeeLabel->setObjectName("mrcQueueTailFeeLabel");

        gridLayout->addWidget(mrcQueueTailFeeLabel, 4, 0, 1, 1);

        mrcQueuePosition = new QLabel(mrcStatusSubmitFrame);
        mrcQueuePosition->setObjectName("mrcQueuePosition");

        gridLayout->addWidget(mrcQueuePosition, 5, 1, 1, 1);

        numMRCInQueueLabel = new QLabel(mrcStatusSubmitFrame);
        numMRCInQueueLabel->setObjectName("numMRCInQueueLabel");

        gridLayout->addWidget(numMRCInQueueLabel, 1, 0, 1, 1);

        numMRCInQueue = new QLabel(mrcStatusSubmitFrame);
        numMRCInQueue->setObjectName("numMRCInQueue");

        gridLayout->addWidget(numMRCInQueue, 1, 1, 1, 1);

        mrcQueuePositionLabel = new QLabel(mrcStatusSubmitFrame);
        mrcQueuePositionLabel->setObjectName("mrcQueuePositionLabel");

        gridLayout->addWidget(mrcQueuePositionLabel, 5, 0, 1, 1);

        mrcQueuePayLimitFee = new QLabel(mrcStatusSubmitFrame);
        mrcQueuePayLimitFee->setObjectName("mrcQueuePayLimitFee");

        gridLayout->addWidget(mrcQueuePayLimitFee, 3, 1, 1, 1);

        mrcQueueLimitLabel = new QLabel(mrcStatusSubmitFrame);
        mrcQueueLimitLabel->setObjectName("mrcQueueLimitLabel");

        gridLayout->addWidget(mrcQueueLimitLabel, 0, 0, 1, 1);

        mrcMinimumSubmitFeeLabel = new QLabel(mrcStatusSubmitFrame);
        mrcMinimumSubmitFeeLabel->setObjectName("mrcMinimumSubmitFeeLabel");

        gridLayout->addWidget(mrcMinimumSubmitFeeLabel, 6, 0, 1, 1);

        mrcMinimumSubmitFee = new QLabel(mrcStatusSubmitFrame);
        mrcMinimumSubmitFee->setObjectName("mrcMinimumSubmitFee");

        gridLayout->addWidget(mrcMinimumSubmitFee, 6, 1, 1, 1);

        mrcQueueTailFee = new QLabel(mrcStatusSubmitFrame);
        mrcQueueTailFee->setObjectName("mrcQueueTailFee");

        gridLayout->addWidget(mrcQueueTailFee, 4, 1, 1, 1);

        mrcQueueLimit = new QLabel(mrcStatusSubmitFrame);
        mrcQueueLimit->setObjectName("mrcQueueLimit");

        gridLayout->addWidget(mrcQueueLimit, 0, 1, 1, 1);

        mrcQueueHeadFee = new QLabel(mrcStatusSubmitFrame);
        mrcQueueHeadFee->setObjectName("mrcQueueHeadFee");

        gridLayout->addWidget(mrcQueueHeadFee, 2, 1, 1, 1);

        mrcQueueHeadFeeLabel = new QLabel(mrcStatusSubmitFrame);
        mrcQueueHeadFeeLabel->setObjectName("mrcQueueHeadFeeLabel");

        gridLayout->addWidget(mrcQueueHeadFeeLabel, 2, 0, 1, 1);

        gridLayout->setColumnStretch(0, 300);
        gridLayout->setColumnStretch(1, 100);

        verticalLayout_6->addLayout(gridLayout);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_6->addItem(verticalSpacer);

        mrcFeeHorizontalLayout = new QHBoxLayout();
        mrcFeeHorizontalLayout->setObjectName("mrcFeeHorizontalLayout");
        mrcFeeBoostLabel = new QLabel(mrcStatusSubmitFrame);
        mrcFeeBoostLabel->setObjectName("mrcFeeBoostLabel");
        mrcFeeBoostLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        mrcFeeHorizontalLayout->addWidget(mrcFeeBoostLabel);

        mrcFeeBoostRaiseToMinimumButton = new QPushButton(mrcStatusSubmitFrame);
        mrcFeeBoostRaiseToMinimumButton->setObjectName("mrcFeeBoostRaiseToMinimumButton");

        mrcFeeHorizontalLayout->addWidget(mrcFeeBoostRaiseToMinimumButton);

        mrcFeeBoostSpinBox = new BitcoinAmountField(mrcStatusSubmitFrame);
        mrcFeeBoostSpinBox->setObjectName("mrcFeeBoostSpinBox");

        mrcFeeHorizontalLayout->addWidget(mrcFeeBoostSpinBox);

        mrcFeeHorizontalLayout->setStretch(0, 300);
        mrcFeeHorizontalLayout->setStretch(2, 100);

        verticalLayout_6->addLayout(mrcFeeHorizontalLayout);

        ButtonHorizontalLayout = new QHBoxLayout();
        ButtonHorizontalLayout->setObjectName("ButtonHorizontalLayout");
        mrcUpdateButton = new QPushButton(mrcStatusSubmitFrame);
        mrcUpdateButton->setObjectName("mrcUpdateButton");

        ButtonHorizontalLayout->addWidget(mrcUpdateButton);

        mrcSubmitButton = new QPushButton(mrcStatusSubmitFrame);
        mrcSubmitButton->setObjectName("mrcSubmitButton");

        ButtonHorizontalLayout->addWidget(mrcSubmitButton);

        SubmittedIconLabel = new QLabel(mrcStatusSubmitFrame);
        SubmittedIconLabel->setObjectName("SubmittedIconLabel");
        SubmittedIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/transaction_confirmed")));
        SubmittedIconLabel->setScaledContents(false);

        ButtonHorizontalLayout->addWidget(SubmittedIconLabel);

        ErrorIconLabel = new QLabel(mrcStatusSubmitFrame);
        ErrorIconLabel->setObjectName("ErrorIconLabel");
        ErrorIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/warning")));
        ErrorIconLabel->setScaledContents(false);

        ButtonHorizontalLayout->addWidget(ErrorIconLabel);

        mrcRequestButtonBox = new QDialogButtonBox(mrcStatusSubmitFrame);
        mrcRequestButtonBox->setObjectName("mrcRequestButtonBox");
        mrcRequestButtonBox->setStandardButtons(QDialogButtonBox::Ok);

        ButtonHorizontalLayout->addWidget(mrcRequestButtonBox);


        verticalLayout_6->addLayout(ButtonHorizontalLayout);

        verticalLayout_6->setStretch(0, 8);
        verticalLayout_6->setStretch(2, 2);
        verticalLayout_6->setStretch(3, 2);

        verticalLayout->addWidget(mrcStatusSubmitFrame);


        retranslateUi(MRCRequestPage);

        QMetaObject::connectSlotsByName(MRCRequestPage);
    } // setupUi

    void retranslateUi(QDialog *MRCRequestPage)
    {
        MRCRequestPage->setWindowTitle(QCoreApplication::translate("MRCRequestPage", "MRC Requests", nullptr));
        waitForBlockUpdateLabel->setText(QCoreApplication::translate("MRCRequestPage", "Please wait.", nullptr));
        waitForBlockUpdate->setText(QString());
        mrcQueuePayLimitFeeLabel->setText(QCoreApplication::translate("MRCRequestPage", "MRC Fee @ Pay Limit Position in Queue", nullptr));
        mrcQueueTailFeeLabel->setText(QCoreApplication::translate("MRCRequestPage", "MRC Fee @ Tail of Queue", nullptr));
#if QT_CONFIG(tooltip)
        mrcQueuePosition->setToolTip(QCoreApplication::translate("MRCRequestPage", "Your projected or actual position among MRCs in the memory pool ordered by MRC fee in descending order", nullptr));
#endif // QT_CONFIG(tooltip)
        mrcQueuePosition->setText(QString());
        numMRCInQueueLabel->setText(QCoreApplication::translate("MRCRequestPage", "Number of All MRC Requests in Queue", nullptr));
#if QT_CONFIG(tooltip)
        numMRCInQueue->setToolTip(QCoreApplication::translate("MRCRequestPage", "The number of MRCs in the memory pool", nullptr));
#endif // QT_CONFIG(tooltip)
        numMRCInQueue->setText(QString());
        mrcQueuePositionLabel->setText(QCoreApplication::translate("MRCRequestPage", "Your Projected MRC Request Position in Queue", nullptr));
#if QT_CONFIG(tooltip)
        mrcQueuePayLimitFee->setToolTip(QCoreApplication::translate("MRCRequestPage", "The MRC fee being paid by the MRC in the last position within the pay limit in the memory pool", nullptr));
#endif // QT_CONFIG(tooltip)
        mrcQueuePayLimitFee->setText(QString());
#if QT_CONFIG(tooltip)
        mrcQueueLimitLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        mrcQueueLimitLabel->setText(QCoreApplication::translate("MRCRequestPage", "MRC Request Pay Limit per Block", nullptr));
        mrcMinimumSubmitFeeLabel->setText(QCoreApplication::translate("MRCRequestPage", "Your MRC Calculated Minimum Fee", nullptr));
#if QT_CONFIG(tooltip)
        mrcMinimumSubmitFee->setToolTip(QCoreApplication::translate("MRCRequestPage", "The calculated minimum fee for the MRC. This may not be sufficient to submit the MRC if the queue is already full. In that case, the MRC Fee Boost field will appear and you need to use it to raise the fee to get your MRC in the queue.", nullptr));
#endif // QT_CONFIG(tooltip)
        mrcMinimumSubmitFee->setText(QString());
#if QT_CONFIG(tooltip)
        mrcQueueTailFee->setToolTip(QCoreApplication::translate("MRCRequestPage", "The lowest MRC fee being paid of MRCs in the memory pool", nullptr));
#endif // QT_CONFIG(tooltip)
        mrcQueueTailFee->setText(QString());
#if QT_CONFIG(tooltip)
        mrcQueueLimit->setToolTip(QCoreApplication::translate("MRCRequestPage", "The maximum number of MRCs that can be paid per block", nullptr));
#endif // QT_CONFIG(tooltip)
        mrcQueueLimit->setText(QString());
#if QT_CONFIG(tooltip)
        mrcQueueHeadFee->setToolTip(QCoreApplication::translate("MRCRequestPage", "The highest MRC fee being paid of MRCs in the memory pool", nullptr));
#endif // QT_CONFIG(tooltip)
        mrcQueueHeadFee->setText(QString());
        mrcQueueHeadFeeLabel->setText(QCoreApplication::translate("MRCRequestPage", "MRC Fee @ Head of Queue", nullptr));
        mrcFeeBoostLabel->setText(QCoreApplication::translate("MRCRequestPage", "MRC Fee Boost", nullptr));
#if QT_CONFIG(tooltip)
        mrcFeeBoostRaiseToMinimumButton->setToolTip(QCoreApplication::translate("MRCRequestPage", "This will automatically boost the MRC fee you are paying to get your MRC request in the queue.", nullptr));
#endif // QT_CONFIG(tooltip)
        mrcFeeBoostRaiseToMinimumButton->setText(QCoreApplication::translate("MRCRequestPage", "Raise to Minimum For Submit", nullptr));
#if QT_CONFIG(tooltip)
        mrcFeeBoostSpinBox->setToolTip(QCoreApplication::translate("MRCRequestPage", "This appears if the queue is full and you need to boost the fee you will pay out of your rewards to displace someone else in the queue. It is NOT the amount of reward to be redeemed!", nullptr));
#endif // QT_CONFIG(tooltip)
        mrcUpdateButton->setText(QCoreApplication::translate("MRCRequestPage", "Update", nullptr));
        mrcSubmitButton->setText(QCoreApplication::translate("MRCRequestPage", "Submit", nullptr));
        SubmittedIconLabel->setText(QString());
        ErrorIconLabel->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class MRCRequestPage: public Ui_MRCRequestPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MRCREQUESTPAGE_H
