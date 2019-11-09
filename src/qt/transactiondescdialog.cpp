#include "transactiondescdialog.h"
#include "transactiontablemodel.h"
#include "ui_transactiondescdialog.h"
#include "main.h"
#include "util.h"
#include <QMessageBox>
#include <QModelIndex>

QString ToQString(std::string s);

TransactionDescDialog::TransactionDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
