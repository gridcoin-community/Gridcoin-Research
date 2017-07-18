#include "statisticspage.h"
#include "ui_statisticspage.h"

#include "main.h"

StatisticsPage::StatisticsPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::StatisticsPage)
{
    ui->setupUi(this);
}

StatisticsPage::~StatisticsPage()
{
    delete ui;
}

void StatisticsPage::updateglobalstatus()
{

     ui->labelBlocks->setText(QString::fromUtf8(GlobalStatusStruct.blocks.c_str()));
     ui->labelDifficulty->setText(QString::fromUtf8(GlobalStatusStruct.difficulty.c_str()));
     ui->labelNetWeight->setText(QString::fromUtf8(GlobalStatusStruct.netWeight.c_str()));
     ui->labelDporWeight->setText(QString::fromUtf8(GlobalStatusStruct.dporWeight.c_str()));
     ui->labelMagnitude->setText(QString::fromUtf8(GlobalStatusStruct.magnitude.c_str()));
     ui->labelProject->setText(QString::fromUtf8(GlobalStatusStruct.project.c_str()));
     ui->labelDporSubsidy->setText(QString::fromUtf8(GlobalStatusStruct.dporSubsidy.c_str()));
     ui->labelDporMagUnit->setText(QString::fromUtf8(GlobalStatusStruct.dporMagUnit.c_str()));
     ui->labelCpid->setText(QString::fromUtf8(GlobalStatusStruct.cpid.c_str()));
     ui->labelStatus->setText(QString::fromUtf8(GlobalStatusStruct.status.c_str()));
     //ui->labelPoll->setText(QString::fromUtf8(GlobalStatusStruct.poll.c_str()));
     //ui->labelErrors->setText(QString::fromUtf8(GlobalStatusStruct.errors.c_str()));
}

