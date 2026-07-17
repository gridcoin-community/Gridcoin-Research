// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PSGTPOOLPAGE_H
#define BITCOIN_QT_PSGTPOOLPAGE_H

#include <QWidget>

#include <string>

class ClientModel;
class MultisignPSGTDialog;
class PSGTPoolTableModel;
class WalletModel;

namespace interfaces {
class PSGTPoolContext;
} // namespace interfaces

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QTableView;
QT_END_NAMESPACE

//!
//! \brief The PSGT Pool page (#2910): a persistent navigation page listing the
//! multisig transactions awaiting signatures on the network, with Sign /
//! Remove / Details actions. Modeled on the Voting page (a central-widget nav
//! page with its own table model).
//!
class PSGTPoolPage : public QWidget
{
    Q_OBJECT

public:
    explicit PSGTPoolPage(QWidget* parent = nullptr);

    //! Provide the PSGT pool interface (Phase 1d-v). Must be set before
    //! setWalletModel(), which constructs the interface-backed table model.
    void setPSGTPoolContext(interfaces::PSGTPoolContext* context);

    void setWalletModel(WalletModel* wallet_model);
    void setClientModel(ClientModel* client_model);
    //! The shared Multisign dialog the Details action opens (owned by BitcoinGUI).
    void setMultisignDialog(MultisignPSGTDialog* dialog);

private Q_SLOTS:
    void sign();
    void remove();
    void details();
    void updateActiveState();
    void updateButtons();

private:
    interfaces::PSGTPoolContext* m_psgt_context = nullptr;
    WalletModel* m_wallet_model = nullptr;
    ClientModel* m_client_model = nullptr;
    MultisignPSGTDialog* m_multisign_dialog = nullptr;
    PSGTPoolTableModel* m_table_model = nullptr;

    QLabel* m_banner = nullptr;
    QTableView* m_table = nullptr;
    QPushButton* m_sign_button = nullptr;
    QPushButton* m_remove_button = nullptr;
    QPushButton* m_details_button = nullptr;
    QPushButton* m_refresh_button = nullptr;

    //! The multisig image of the selected row, or nullopt if no row selected.
    bool selectedImageHex(std::string& image_hex_out) const;
};

#endif // BITCOIN_QT_PSGTPOOLPAGE_H
