#include "qt/decoration.h"
#include "transactiondescdialog.h"
#include "ui_transactiondescdialog.h"
#include "main.h"
#include "util.h"
#include <QMessageBox>

QString ToQString(std::string s);

TransactionDescDialog::TransactionDescDialog(const QString& html, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    resize(GRC::ScaleSize(this, width(), height()));

    // Detail HTML is rendered producer-side by WalletTxStore::getRowDetail
    // (windowed-model PR5-C); an empty result means the tx is no longer in the
    // wallet (or the key was stale), so show a plain fallback rather than a blank.
    ui->detailText->setHtml(html.isEmpty() ? tr("Transaction details unavailable.") : html);
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
