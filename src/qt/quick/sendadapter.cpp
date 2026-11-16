// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <qt/quick/sendadapter.h>

#include <chainparams.h>
#include <interfaces/wallet.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/quick/qmlprovider.h>
#include <qt/quick/walletopgate.h>
#include <qt/sendfeenegotiator.h>
#include <qt/walletmodel.h>

#include <QCoreApplication>
#include <QJSEngine>
#include <QStringList>

#include <utility>

namespace {

QString SendTitle()
{
    return QCoreApplication::translate("SendCoinsDialog", "Send Coins");
}

//! The message the Widgets send dialog shows for a send that ended with
//! `result`, with its context.
QString FailureMessage(const WalletModel::SendCoinsReturn& result)
{
    switch (result.status) {
    case WalletModel::InvalidAddress:
        return QCoreApplication::translate("SendCoinsDialog", "The recipient address is not valid, please recheck.");
    case WalletModel::InvalidAmount:
        return QCoreApplication::translate("SendCoinsDialog", "The amount to pay must be larger than 0.");
    case WalletModel::AmountExceedsBalance:
        return QCoreApplication::translate("SendCoinsDialog", "The amount exceeds your balance.");
    case WalletModel::AmountWithFeeExceedsBalance:
        return QCoreApplication::translate("SendCoinsDialog",
                                           "The total exceeds your balance when the %1 transaction fee is included.")
            .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, result.fee));
    case WalletModel::FeeExceedsSubtractedAmount:
        return QCoreApplication::translate("SendCoinsDialog",
                                           "The transaction fee (%1) exceeds the amount being sent to a recipient "
                                           "with 'Subtract fee from amount' enabled. Please enter a larger amount.")
            .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, result.fee));
    case WalletModel::DuplicateAddress:
        return QCoreApplication::translate("SendCoinsDialog",
                                           "Duplicate address found, can only send to each address once per send operation.");
    case WalletModel::TransactionCreationFailed:
        return QCoreApplication::translate("SendCoinsDialog", "Error: Transaction creation failed.");
    case WalletModel::WalletUnlockedForStakingOnly:
        return QCoreApplication::translate("SendCoinsDialog",
                                           "Error: The wallet is unlocked for staking only, so it will not send. "
                                           "Lock the wallet and unlock it again without the staking-only option to send.");
    case WalletModel::TransactionCommitFailed:
        return QCoreApplication::translate("SendCoinsDialog",
                                           "Error: The transaction was rejected. This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
    case WalletModel::FeeConfirmationRequired:
        // The negotiator turns every fee request into a question or a creation
        // failure, so a finished send never carries this status.
    case WalletModel::OK:
    case WalletModel::Aborted:
        break;
    }
    return QString();
}

QString FeeText(qint64 fee)
{
    // The Widgets dialog's fee question, in the context its translations
    // shipped under.
    return QCoreApplication::translate("BitcoinGUI",
               "This transaction is over the size limit.  You can still send it for a fee of %1, "
               "which goes to the nodes that process your transaction and helps to support the network.  "
               "Do you want to pay the fee?").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, fee));
}

} // namespace

//! What a send attempt returned. A plain value.
struct SendAdapter::AttemptResult {
    WalletModel::SendCoinsReturn sent;
};

SendAdapter::SendAdapter(int marker, WalletOpGate& gate)
    : QObject(nullptr), m_marker(marker), m_gate(gate), m_unit(BitcoinUnits::BTC)
{
}

SendAdapter::~SendAdapter() = default;

SendAdapter* SendAdapter::create(QQmlEngine*, QJSEngine*)
{
    QmlProvider* provider = QmlProvider::installed();
    if (!provider) {
        // A programming error that would hand QML a null singleton; qFatal
        // halts in every build.
        qFatal("SendAdapter::create: no QmlProvider is installed");
    }
    QJSEngine::setObjectOwnership(&provider->sendAdapter(), QJSEngine::CppOwnership);
    return &provider->sendAdapter();
}

void SendAdapter::attach(WalletModel& wallet, ClientModel* client, QmlCallRunner& runner)
{
    // Every wallet call goes through the gate, which was attached with the
    // same runner.
    Q_UNUSED(runner);
    m_wallet = &wallet;
    if (OptionsModel* options = wallet.getOptionsModel()) {
        m_connections.push_back(
            connect(options, &OptionsModel::displayUnitChanged, this, &SendAdapter::onDisplayUnitChanged));
        onDisplayUnitChanged(options->getDisplayUnit());
    }
    if (client) {
        m_connections.push_back(connect(client, &ClientModel::numBlocksChanged, this, &SendAdapter::setChainHeight));
    }
}

void SendAdapter::detach()
{
    for (const QMetaObject::Connection& connection : m_connections) disconnect(connection);
    m_connections.clear();
    m_wallet = nullptr;
    // No signal: the request ends silently, as the gate's does.
    m_request.reset();
    setBusy(false);
    m_take_uri = nullptr;
    m_put_uri = nullptr;
}

void SendAdapter::setUriSource(std::function<QString()> take, std::function<void(const QString&)> put)
{
    m_take_uri = std::move(take);
    m_put_uri = std::move(put);
}

QAbstractItemModel* SendAdapter::recipients() const
{
    return const_cast<SendRecipientModel*>(&m_recipients);
}

QString SendAdapter::unitName() const
{
    return BitcoinUnits::name(m_unit);
}

bool SendAdapter::validRow(int row) const
{
    return row >= 0 && row < m_recipients.rowCount();
}

SendAdapter::Request* SendAdapter::requestIn(quint64 id, State state)
{
    if (!m_request || m_request->id != id || m_request->state != state) return nullptr;
    return &*m_request;
}

bool SendAdapter::addRecipient()
{
    if (!attached() || m_busy) return false;
    m_recipients.appendRow(SendRecipientModel::Row{});
    return true;
}

bool SendAdapter::removeRecipient(int row)
{
    if (!attached() || m_busy || !validRow(row) || m_recipients.rowCount() <= 1) return false;
    m_recipients.removeRowAt(row);
    return true;
}

bool SendAdapter::clearRecipients()
{
    if (!attached() || m_busy) return false;
    m_recipients.reset();
    return true;
}

bool SendAdapter::updateRecipient(int row, const QVariantMap& fields)
{
    if (!attached() || m_busy || !validRow(row)) return false;
    SendRecipientModel::Row value = m_recipients.row(row);
    if (fields.contains(QStringLiteral("recipient"))) {
        value.recipient.address = fields.value(QStringLiteral("recipient")).toString();
        value.address_valid = true;
    }
    if (fields.contains(QStringLiteral("label"))) {
        value.recipient.label = fields.value(QStringLiteral("label")).toString();
    }
    if (fields.contains(QStringLiteral("amount"))) {
        const QString text = fields.value(QStringLiteral("amount")).toString();
        qint64 amount = 0;
        // A text that does not parse is kept as typed, with no amount.
        if (!BitcoinUnits::parse(m_unit, text, &amount)) amount = 0;
        value.recipient.amount = amount;
        value.amount_text = text;
        value.amount_valid = true;
    }
    if (fields.contains(QStringLiteral("message"))) {
        value.recipient.Message = fields.value(QStringLiteral("message")).toString();
    }
    if (fields.contains(QStringLiteral("subtractFee"))) {
        value.recipient.fSubtractFeeFromAmount = fields.value(QStringLiteral("subtractFee")).toBool();
    }
    m_recipients.setRow(row, value);
    return true;
}

quint64 SendAdapter::send()
{
    if (!attached() || m_busy) return 0;

    // As SendCoinsEntry::validate: a valid address and an amount above 0.
    bool all_valid = true;
    for (int row = 0; row < m_recipients.rowCount(); ++row) {
        SendRecipientModel::Row value = m_recipients.row(row);
        const bool address_valid = m_wallet->validateAddress(value.recipient.address);
        const bool amount_valid = value.recipient.amount > 0;
        if (!address_valid || !amount_valid) all_valid = false;
        if (address_valid != value.address_valid || amount_valid != value.amount_valid) {
            value.address_valid = address_valid;
            value.amount_valid = amount_valid;
            m_recipients.setRow(row, value);
        }
    }
    if (!all_valid) return 0;

    // The request's own copy: later edits, URIs and block heights do not
    // change what was confirmed. Only the first row carries the message, and
    // only while the network accepts message contracts.
    Request request;
    request.id = m_next_id + 1;
    request.state = State::AwaitingConfirm;
    for (int row = 0; row < m_recipients.rowCount(); ++row) {
        SendCoinsRecipient recipient = m_recipients.row(row).recipient;
        if (row != 0 || !m_message_allowed) recipient.Message.clear();
        request.recipients.append(recipient);
    }

    // SendCoinsDialog::on_sendButton_clicked's confirmation, in its context.
    QStringList formatted;
    for (const SendCoinsRecipient& recipient : std::as_const(request.recipients)) {
        QString amount = BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, recipient.amount);
        if (recipient.fSubtractFeeFromAmount) {
            amount += QStringLiteral(" ") + QCoreApplication::translate("SendCoinsDialog", "(minus fee)");
        }
        formatted.append(QCoreApplication::translate("SendCoinsDialog", "<b>%1</b> to %2 (%3)")
                             .arg(amount, recipient.label.toHtmlEscaped(), recipient.address));
    }
    const QString title = QCoreApplication::translate("SendCoinsDialog", "Confirm send coins");
    const QString text = QCoreApplication::translate("SendCoinsDialog", "Are you sure you want to send %1?")
                             .arg(formatted.join(QCoreApplication::translate("SendCoinsDialog", " and ")));

    const quint64 id = request.id;
    m_next_id = id;
    m_request = std::move(request);
    setBusy(true);
    emit sendConfirmationRequested(id, title, text);
    return id;
}

bool SendAdapter::confirmSend(quint64 id, bool yes)
{
    if (!attached()) return false;
    Request* request = requestIn(id, State::AwaitingConfirm);
    if (!request) return false;
    if (!yes) {
        finish(Aborted, QString(), QString(), QString());
        return true;
    }

    request->state = State::AwaitingGate;
    const quint64 gate_id = m_gate.begin([this, id](quint64 granted_id) { onGranted(id, granted_id); },
                                         [this, id](quint64, WalletOpGate::Result) { onRefused(id); });
    if (gate_id == 0) {
        // The gate refused to begin: the prompt latch is held, or the runner,
        // closed at quit, refused the read.
        finish(Busy, SendTitle(),
               QCoreApplication::translate("SendCoinsDialog", "Another wallet operation is in progress. Please try again when it has finished."),
               QString());
        return true;
    }
    request->gate_id = gate_id;
    return true;
}

bool SendAdapter::confirmFee(quint64 id, bool yes)
{
    if (!attached()) return false;
    Request* request = requestIn(id, State::AwaitingFee);
    if (!request) return false;
    const SendFeeNegotiator::Step step = request->negotiator->onAnswer(yes);
    if (step.kind == SendFeeNegotiator::Step::Send) {
        postAttempt();
        return true;
    }
    // Declined: Aborted, with no text.
    finish(Aborted, QString(), QString(), QString());
    return true;
}

void SendAdapter::onGranted(quint64 id, quint64 gate_id)
{
    Request* request = requestIn(id, State::AwaitingGate);
    if (!request) return;
    request->granted = true;
    request->gate_id = gate_id;
    // A fresh fee loop for every send: nothing carries into the next one.
    request->negotiator = std::make_unique<SendFeeNegotiator>();
    postAttempt();
}

void SendAdapter::onRefused(quint64 id)
{
    // The passphrase prompt was cancelled: no box, no send, the rows kept, as
    // the Widgets dialog does when the unlock is cancelled.
    if (!requestIn(id, State::AwaitingGate)) return;
    finish(Aborted, QString(), QString(), QString());
}

void SendAdapter::postAttempt()
{
    Request& request = *m_request;
    request.state = State::AttemptInFlight;
    // WalletModel::sendCoins is the one audited model method called on the
    // serial lane: it validates, calls the wallet and maps the result, and
    // stores nothing. The model outlives the call, since the lane is drained
    // before the wallet is detached.
    WalletModel& model = *m_wallet;
    const QList<SendCoinsRecipient> recipients = request.recipients;
    const qint64 fee = request.negotiator->acceptedFee();
    auto holder = std::make_shared<AttemptResult>();
    const quint64 id = request.id;
    // A refused post leaves the request in flight; detach drops it.
    (void)m_gate.post(
        request.gate_id,
        [&model, recipients, fee, holder](interfaces::Wallet&) {
            holder->sent = model.sendCoins(recipients, std::nullopt, fee);
        },
        [this, id, holder] { deliverAttempt(id, *holder); });
}

void SendAdapter::deliverAttempt(quint64 id, const AttemptResult& result)
{
    Request* request = requestIn(id, State::AttemptInFlight);
    if (!request) return;
    const SendFeeNegotiator::Step step = request->negotiator->onResult(result.sent);
    if (step.kind == SendFeeNegotiator::Step::Confirm) {
        // The wallet stays unlocked while the question is open. The request
        // is in its new state before the signal and is not touched after it.
        request->state = State::AwaitingFee;
        emit feeConfirmationRequested(id, step.fee,
                                      QCoreApplication::translate("BitcoinGUI", "Confirm transaction fee"),
                                      FeeText(step.fee));
        return;
    }

    switch (step.result.status) {
    case WalletModel::OK:
        finish(Sent, QString(), QString(), step.result.hex);
        return;
    case WalletModel::Aborted:
        finish(Aborted, QString(), QString(), QString());
        return;
    default:
        finish(Failed, SendTitle(), FailureMessage(step.result), QString());
        return;
    }
}

void SendAdapter::finish(SendAdapter::Outcome outcome, const QString& title, const QString& message,
                         const QString& txid)
{
    const quint64 id = m_request->id;
    if (m_request->granted) m_gate.end(m_request->gate_id);
    if (outcome == Sent) m_recipients.reset();
    m_request.reset();
    setBusy(false);
    emit finished(id, outcome, title, message, txid);
    // A URI that arrived during the send waited for it.
    claimPendingUri();
}

void SendAdapter::setBusy(bool busy)
{
    if (busy == m_busy) return;
    m_busy = busy;
    emit busyChanged();
}

bool SendAdapter::acceptUri(const QString& url)
{
    if (!attached() || !m_put_uri) return false;
    // The path a received URI takes: the bridge stores it and signals, and
    // the claim follows (now, or once a send in progress has finished).
    m_put_uri(url);
    return true;
}

void SendAdapter::claimPendingUri()
{
    if (!attached() || !m_take_uri || m_busy) return;
    const QString uri = m_take_uri();
    if (uri.isEmpty()) return;

    // SendCoinsDialog::handleURI's checks, then BitcoinGUI::handleURI's
    // texts for a URI that fails them.
    SendCoinsRecipient recipient;
    if (GUIUtil::parseBitcoinURI(uri, &recipient) && m_wallet->validateAddress(recipient.address)) {
        paste(recipient);
        emit uriAccepted();
        return;
    }
    emit uriRejected(QCoreApplication::translate("BitcoinGUI", "URI handling"),
                     QCoreApplication::translate("BitcoinGUI", "URI can not be parsed! This can be caused by an invalid Gridcoin address or malformed URI parameters."));
}

void SendAdapter::paste(const SendCoinsRecipient& recipient)
{
    SendRecipientModel::Row value;
    value.recipient = recipient;
    if (recipient.amount != 0) value.amount_text = BitcoinUnits::format(m_unit, recipient.amount);
    // As SendCoinsDialog::pasteEntry: the one row is replaced while its
    // recipient is empty; otherwise a row is appended.
    if (m_recipients.rowCount() == 1 && m_recipients.row(0).recipient.address.isEmpty()) {
        m_recipients.setRow(0, value);
    } else {
        m_recipients.appendRow(value);
    }
}

QString SendAdapter::outOfSyncTitle() const
{
    return QCoreApplication::translate("BitcoinGUI", "Wallet Not In Sync");
}

QString SendAdapter::outOfSyncText() const
{
    return QCoreApplication::translate("BitcoinGUI",
                                       "The wallet is not yet in sync with the network. Your balance "
                                       "may be inaccurate, and transactions created while out of sync "
                                       "may not confirm properly.\n\n"
                                       "Are you sure you want to proceed?");
}

void SendAdapter::setChainHeight(int height)
{
    if (!attached()) return;
    // The first block a transaction sent now could be mined in, as the
    // Widgets dialog judges it.
    const bool allowed = IsMessageContractEnabled(height + 1);
    if (allowed == m_message_allowed) return;
    m_message_allowed = allowed;
    emit messageAllowedChanged();
}

void SendAdapter::onDisplayUnitChanged(int unit)
{
    if (unit != m_unit) {
        m_unit = unit;
        emit unitNameChanged();
    }
    // A typed amount follows the unit, as the Widgets amount field does.
    for (int row = 0; row < m_recipients.rowCount(); ++row) {
        SendRecipientModel::Row value = m_recipients.row(row);
        if (value.amount_text.isEmpty()) continue;
        value.amount_text = BitcoinUnits::format(unit, value.recipient.amount);
        m_recipients.setRow(row, value);
    }
}
