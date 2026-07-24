// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/coinselectionmodel.h"

#include "bitcoinunits.h"
#include "interfaces/wallet.h"
#include "qt/guilog.h"
#include "qt/optionsmodel.h"
#include "qt/walletmodel.h"

#include <QDateTime>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace {

//! Windowing constants — the DetailedTxModel values (windowed-model PR5).
constexpr int kInitialWindow = 200;  //!< initial fetch + minimum margin each side
constexpr int kDebounceMs = 100;     //!< scroll-fetch coalescing
constexpr int kMaxFetch = 4096;      //!< backstop on a single content fetch

//! The Qt column enum must track the Qt-free sort vocabulary so setViewSort
//! pushes the right key (COLUMN_CHECKBOX is unsortable and has no mirror).
static_assert(static_cast<int>(CoinSelectionModel::COLUMN_AMOUNT) - 1 == GRC::COINCOL_AMOUNT
              && static_cast<int>(CoinSelectionModel::COLUMN_LABEL) - 1 == GRC::COINCOL_LABEL
              && static_cast<int>(CoinSelectionModel::COLUMN_ADDRESS) - 1 == GRC::COINCOL_ADDRESS
              && static_cast<int>(CoinSelectionModel::COLUMN_DATE) - 1 == GRC::COINCOL_DATE
              && static_cast<int>(CoinSelectionModel::COLUMN_CONFIRMATIONS) - 1 == GRC::COINCOL_CONFS,
              "CoinSelectionModel columns must mirror GRC::CoinSortColumn (offset by the checkbox column)");

} // anonymous namespace

//! Scope-aware WindowCache sink: forwards structural brackets to the model's
//! row signals under the right parent (the flat root, or a group's index
//! resolved BY ADDRESS at signal time — never by a captured row, which live
//! directory re-slots would invalidate).
struct CoinCacheSink : public GRC::WindowCacheSink {
    CoinSelectionModel* m;
    std::string scope; // "" == flat

    CoinCacheSink(CoinSelectionModel* model, std::string s) : m(model), scope(std::move(s)) {}

    QModelIndex parentIndex() const
    {
        if (scope.empty()) return QModelIndex();
        return m->groupIndexByAddress(scope);
    }

    void beginReset() override { m->beginResetModel(); }
    void endReset() override { m->endResetModel(); }
    void beginInsert(int first, int count) override
    {
        m->beginInsertRows(parentIndex(), first, first + count - 1);
    }
    void endInsert() override { m->endInsertRows(); }
    void beginRemove(int first, int count) override
    {
        m->beginRemoveRows(parentIndex(), first, first + count - 1);
    }
    void endRemove() override { m->endRemoveRows(); }
    void dataChanged(int first, int count) override
    {
        const QModelIndex parent = parentIndex();
        emit m->QAbstractItemModel::dataChanged(
            m->index(first, 0, parent),
            m->index(first + count - 1, CoinSelectionModel::COLUMN_CONFIRMATIONS, parent));
    }
};

CoinSelectionModel::CoinSelectionModel(WalletModel* wallet_model,
                                       interfaces::WalletCoinControl* coin_control,
                                       int view_id,
                                       QObject* parent)
    : QAbstractItemModel(parent)
    , m_wallet_model(wallet_model)
    , m_source(wallet_model->coinSource())
    , m_coin_control(coin_control)
    , m_view_id(view_id)
{
    m_flat_sink = std::make_unique<CoinCacheSink>(this, std::string());

    m_fetch_timer = new QTimer(this);
    m_fetch_timer->setSingleShot(true);
    connect(m_fetch_timer, &QTimer::timeout, this, &CoinSelectionModel::onFetchTimeout);

    // Register before wiring the drain so the registration Reset is the first
    // event this model sees; the initial wallet scan runs on WalletModel's
    // one-shot load thread (never this GUI thread — the scan holds
    // cs_main+cs_wallet and is O(wallet)).
    m_source.registerView(m_view_id, m_mode, GRC::COINCOL_AMOUNT, /*desc*/ 1);

    // Reconcile the GUI-side selection against the store (prunes stale
    // outpoints from earlier sessions of the dialog).
    m_coin_control->selected = m_source.reconcileSelection(m_coin_control->selected);

    connect(m_wallet_model, &WalletModel::coinEventsDrained,
            this, &CoinSelectionModel::applyCoinEventBatch);

    m_wallet_model->ensureCoinSourceLoaded();

    // A warm store (dialog reopen) already delivered its Reset into the
    // queue; pull it through now so the first paint has data.
    m_wallet_model->drainCoinEventQueue();
}

CoinSelectionModel::~CoinSelectionModel()
{
    m_source.unregisterView(m_view_id);
}

// ---- id registry --------------------------------------------------------

int CoinSelectionModel::idForAddress(const std::string& address)
{
    auto it = m_addr_id.find(address);
    if (it != m_addr_id.end()) return it->second;
    const int id = static_cast<int>(m_id_addr.size());
    m_id_addr.push_back(address);
    m_addr_id.emplace(address, id);
    return id;
}

int CoinSelectionModel::rowForAddress(const std::string& address) const
{
    auto it = m_dir_row.find(address);
    return (it == m_dir_row.end()) ? -1 : it->second;
}

const std::string* CoinSelectionModel::addressForId(quintptr id_plus_one) const
{
    if (id_plus_one == 0) return nullptr;
    const std::size_t id = static_cast<std::size_t>(id_plus_one - 1);
    if (id >= m_id_addr.size()) return nullptr;
    return &m_id_addr[id];
}

QModelIndex CoinSelectionModel::groupIndexByAddress(const std::string& address) const
{
    const int row = rowForAddress(address);
    if (row < 0) return QModelIndex();
    return createIndex(row, 0, quintptr(0));
}

// ---- QAbstractItemModel structure ---------------------------------------

QModelIndex CoinSelectionModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column < 0 || column >= columnCount()) return QModelIndex();

    if (!parent.isValid()) {
        return createIndex(row, column, quintptr(0));
    }

    // Children exist only under root-level group rows in tree mode. The
    // internalId carries the STABLE group id (+1), never the directory row.
    if (m_mode != GRC::CoinViewMode::Tree || parent.internalId() != 0) return QModelIndex();
    if (parent.row() < 0 || parent.row() >= static_cast<int>(m_directory.size())) return QModelIndex();

    const std::string& address = m_directory[static_cast<std::size_t>(parent.row())].address;
    auto it = m_addr_id.find(address);
    if (it == m_addr_id.end()) return QModelIndex();
    return createIndex(row, column, quintptr(it->second + 1));
}

QModelIndex CoinSelectionModel::parent(const QModelIndex& index) const
{
    if (!index.isValid() || index.internalId() == 0) return QModelIndex();
    const std::string* address = addressForId(index.internalId());
    if (!address) return QModelIndex();
    return groupIndexByAddress(*address);
}

int CoinSelectionModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        if (m_mode == GRC::CoinViewMode::Flat) return m_flat.total();
        return static_cast<int>(m_directory.size());
    }
    if (m_mode != GRC::CoinViewMode::Tree || parent.internalId() != 0) return 0;
    if (parent.column() != 0) return 0;
    if (parent.row() < 0 || parent.row() >= static_cast<int>(m_directory.size())) return 0;

    const std::string& address = m_directory[static_cast<std::size_t>(parent.row())].address;
    auto it = m_groups.find(address);
    return (it == m_groups.end()) ? 0 : it->second.cache.total();
}

int CoinSelectionModel::columnCount(const QModelIndex&) const
{
    return COLUMN_CONFIRMATIONS + 1;
}

bool CoinSelectionModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid()) return true;
    // Every group has at least one member (empty groups are removed
    // server-side), so the expand arrow is unconditional for group rows.
    return m_mode == GRC::CoinViewMode::Tree && parent.internalId() == 0;
}

bool CoinSelectionModel::canFetchMore(const QModelIndex& parent) const
{
    if (!parent.isValid() || m_mode != GRC::CoinViewMode::Tree || parent.internalId() != 0) {
        return false;
    }
    if (parent.row() < 0 || parent.row() >= static_cast<int>(m_directory.size())) return false;
    return m_groups.count(m_directory[static_cast<std::size_t>(parent.row())].address) == 0;
}

void CoinSelectionModel::fetchMore(const QModelIndex& parent)
{
    if (!canFetchMore(parent)) return;
    const std::string address = m_directory[static_cast<std::size_t>(parent.row())].address;

    GRC::CoinRowsResult result = m_source.getGroupRows(m_view_id, address, 0, kInitialWindow);

    GroupSlot& slot = m_groups[address];
    slot.sink = std::make_unique<CoinCacheSink>(this, address);

    if (result.total_accepted > 0) {
        // The rows are VIRTUAL: the cache holds the first window; the rest
        // render as placeholders until the viewport-driven fetches fill them.
        // QTreeView materializes a view item per row here (its own cost —
        // measured by the synthetic acceptance gate), but data() stays pure.
        beginInsertRows(parent, 0, result.total_accepted - 1);
        slot.cache.seedInitial(std::move(result.records), 0, result.total_accepted,
                               result.epoch, result.high_water);
        endInsertRows();
    } else {
        slot.cache.seedInitial({}, 0, 0, result.epoch, result.high_water);
    }
}

void CoinSelectionModel::releaseGroup(const QModelIndex& group_index)
{
    if (!isGroup(group_index)) return;
    const std::string address = m_directory[static_cast<std::size_t>(group_index.row())].address;
    auto it = m_groups.find(address);
    if (it == m_groups.end()) return;

    // Un-realize with a proper removal bracket: rowCount(parent) flips back
    // to 0, and Qt's persistent indexes into the branch are invalidated
    // cleanly. Checkbox truth lives in coinControl, so nothing is lost.
    const int total = it->second.cache.total();
    if (total > 0) {
        beginRemoveRows(group_index, 0, total - 1);
        m_groups.erase(it);
        endRemoveRows();
    } else {
        m_groups.erase(it);
    }
}

// ---- data ---------------------------------------------------------------

const GRC::CoinRecord* CoinSelectionModel::recordAt(const QModelIndex& index) const
{
    if (!index.isValid()) return nullptr;
    if (m_mode == GRC::CoinViewMode::Flat) {
        if (index.internalId() != 0) return nullptr;
        return m_flat.at(index.row());
    }
    const std::string* address = addressForId(index.internalId());
    if (!address) return nullptr;
    auto it = m_groups.find(*address);
    if (it == m_groups.end()) return nullptr;
    return it->second.cache.at(index.row());
}

bool CoinSelectionModel::isGroup(const QModelIndex& index) const
{
    return m_mode == GRC::CoinViewMode::Tree && index.isValid() && index.internalId() == 0;
}

QVariant CoinSelectionModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return QVariant();

    const int unit = m_wallet_model->getOptionsModel()
        ? m_wallet_model->getOptionsModel()->getDisplayUnit()
        : static_cast<int>(BitcoinUnits::BTC);

    if (isGroup(index)) {
        if (index.row() >= static_cast<int>(m_directory.size())) return QVariant();
        const GRC::CoinGroupInfo& g = m_directory[static_cast<std::size_t>(index.row())];

        if (role == Qt::CheckStateRole && index.column() == COLUMN_CHECKBOX) {
            if (g.selected_count == 0) return Qt::Unchecked;
            if (g.selected_count >= g.output_count) return Qt::Checked;
            return Qt::PartiallyChecked;
        }
        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case COLUMN_CHECKBOX:
                return QString("(%1)").arg(g.output_count);
            case COLUMN_AMOUNT:
                return BitcoinUnits::format(unit, g.total_amount);
            case COLUMN_LABEL:
                return g.label.empty() ? tr("(no label)")
                                       : QString::fromStdString(g.label);
            case COLUMN_ADDRESS:
                return QString::fromStdString(g.address);
            default:
                return QVariant();
            }
        }
        return QVariant();
    }

    const GRC::CoinRecord* rec = recordAt(index);
    if (!rec) {
        // Placeholder (off-window row): blank text, and critically NO
        // CheckStateRole — a placeholder checkbox must render empty and be
        // un-clickable (see flags()), never a lying "unchecked".
        return QVariant();
    }

    if (role == Qt::CheckStateRole && index.column() == COLUMN_CHECKBOX) {
        return m_coin_control->IsSelected(rec->outpoint) ? Qt::Checked : Qt::Unchecked;
    }
    if (role == Qt::ToolTipRole && index.column() == COLUMN_LABEL && rec->is_change) {
        return tr("change from %1 (%2)")
            .arg(rec->label.empty() ? tr("(no label)") : QString::fromStdString(rec->label),
                 QString::fromStdString(rec->group_address));
    }
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case COLUMN_AMOUNT:
            return BitcoinUnits::format(unit, rec->amount);
        case COLUMN_LABEL:
            if (rec->is_change) return tr("(change)");
            if (m_mode == GRC::CoinViewMode::Tree) return QVariant(); // shown on the parent
            return rec->label.empty() ? tr("(no label)") : QString::fromStdString(rec->label);
        case COLUMN_ADDRESS:
            // Tree mode: the address is shown on the parent; children repeat
            // it only when it differs (change walked from elsewhere).
            if (m_mode == GRC::CoinViewMode::Tree && !rec->is_change) return QVariant();
            return QString::fromStdString(rec->address);
        case COLUMN_DATE:
            return QDateTime::fromSecsSinceEpoch(rec->time).toUTC().toString("yy-MM-dd hh:mm");
        case COLUMN_CONFIRMATIONS: {
            // Serve-time depth from the drained tip height: a tip advance
            // repaints this column without touching any cached record.
            const int depth = (rec->block_height >= 0 && m_tip_height >= rec->block_height)
                ? m_tip_height - rec->block_height + 1
                : rec->depth;
            return QString::number(depth);
        }
        default:
            return QVariant();
        }
    }
    return QVariant();
}

Qt::ItemFlags CoinSelectionModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() != COLUMN_CHECKBOX) return f;

    if (isGroup(index)) {
        // NOT AutoTristate: the check state is computed from server-side
        // aggregates; a click drives selectGroup explicitly in setData.
        return f | Qt::ItemIsUserCheckable;
    }
    // A placeholder row's checkbox is not clickable (its outpoint is not
    // cached, so a toggle could not be validated).
    if (!recordAt(index)) return f;
    return f | Qt::ItemIsUserCheckable;
}

QVariant CoinSelectionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return QVariant();
    switch (section) {
    case COLUMN_CHECKBOX: return QString();
    case COLUMN_AMOUNT: return tr("Amount");
    case COLUMN_LABEL: return tr("Label");
    case COLUMN_ADDRESS: return tr("Address");
    case COLUMN_DATE: return tr("Date");
    case COLUMN_CONFIRMATIONS: return tr("Confirmations");
    }
    return QVariant();
}

// ---- selection ----------------------------------------------------------

bool CoinSelectionModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::CheckStateRole || index.column() != COLUMN_CHECKBOX) return false;
    const bool desired = (value.value<Qt::CheckState>() != Qt::Unchecked);

    if (isGroup(index)) {
        const std::string address = m_directory[static_cast<std::size_t>(index.row())].address;
        applyDeltaToCoinControl(m_source.selectGroup(address, desired));

        // Refresh the parent aggregates from the authoritative source result
        // shape: full/empty selection is exact without a refetch.
        GRC::CoinGroupInfo& g = m_directory[static_cast<std::size_t>(index.row())];
        g.selected_count = desired ? g.output_count : 0;
        g.selected_amount = desired ? g.total_amount : 0;

        emit QAbstractItemModel::dataChanged(index.siblingAtColumn(COLUMN_CHECKBOX),
                                             index.siblingAtColumn(COLUMN_CHECKBOX));
        // Realized children repaint their checkboxes.
        auto it = m_groups.find(address);
        if (it != m_groups.end() && it->second.cache.total() > 0) {
            emit QAbstractItemModel::dataChanged(
                this->index(0, COLUMN_CHECKBOX, index),
                this->index(it->second.cache.total() - 1, COLUMN_CHECKBOX, index));
        }
        emit selectionChanged();
        return true;
    }

    const GRC::CoinRecord* rec = recordAt(index);
    if (!rec) return false;

    const GRC::CoinSelectionUpdate update = m_source.setSelected(rec->outpoint, desired);
    if (!update.applied) {
        // The coin vanished between render and click (a worker removal the
        // drain has not applied yet). Refusing keeps coinControl and the
        // mirror consistent; the pending removal event will repaint.
        return false;
    }
    if (desired) {
        m_coin_control->Select(rec->outpoint);
    } else {
        m_coin_control->UnSelect(rec->outpoint);
    }

    emit QAbstractItemModel::dataChanged(index.siblingAtColumn(COLUMN_CHECKBOX),
                                         index.siblingAtColumn(COLUMN_CHECKBOX));

    // Parent tristate from the synchronously returned aggregates — located
    // by ADDRESS (the stable identity), never by a store-returned row.
    if (m_mode == GRC::CoinViewMode::Tree && !update.group.address.empty()) {
        const int row = rowForAddress(update.group.address);
        if (row >= 0) {
            m_directory[static_cast<std::size_t>(row)] = update.group;
            const QModelIndex gidx = createIndex(row, COLUMN_CHECKBOX, quintptr(0));
            emit QAbstractItemModel::dataChanged(gidx, gidx);
        }
    }
    emit selectionChanged();
    return true;
}

void CoinSelectionModel::applyDeltaToCoinControl(const GRC::CoinBulkSelectionResult& delta)
{
    for (const COutPoint& outpoint : delta.added) m_coin_control->Select(outpoint);
    for (const COutPoint& outpoint : delta.removed) m_coin_control->UnSelect(outpoint);
}

int CoinSelectionModel::selectAll(bool selected)
{
    const GRC::CoinBulkSelectionResult delta = m_source.selectAll(selected);
    applyDeltaToCoinControl(delta);
    refreshDirectoryAggregates();
    emit selectionChanged();
    return static_cast<int>(delta.added.size() + delta.removed.size());
}

bool CoinSelectionModel::applyValueFilter(bool less_or_equal, qint64 value, unsigned int max_inputs)
{
    const GRC::CoinBulkSelectionResult delta =
        m_source.applyValueFilter(less_or_equal, value, max_inputs);
    applyDeltaToCoinControl(delta);
    refreshDirectoryAggregates();
    emit selectionChanged();
    return delta.culled;
}

void CoinSelectionModel::refreshDirectoryAggregates()
{
    // Selection is never a directory sort key, so positions are unchanged —
    // refresh aggregate values in place and repaint. (Cheap: the directory
    // is small; the children are what scale.)
    if (m_mode != GRC::CoinViewMode::Tree || m_directory.empty()) return;

    GRC::CoinGroupsResult result =
        m_source.getGroups(m_view_id, 0, static_cast<int>(m_directory.size()));
    for (std::size_t i = 0; i < result.groups.size() && i < m_directory.size(); ++i) {
        m_directory[i] = result.groups[i];
    }
    emit QAbstractItemModel::dataChanged(
        createIndex(0, COLUMN_CHECKBOX, quintptr(0)),
        createIndex(static_cast<int>(m_directory.size()) - 1, COLUMN_AMOUNT, quintptr(0)));
    for (auto& entry : m_groups) {
        if (entry.second.cache.total() <= 0) continue;
        const QModelIndex parent = groupIndexByAddress(entry.first);
        if (!parent.isValid()) continue;
        emit QAbstractItemModel::dataChanged(
            index(0, COLUMN_CHECKBOX, parent),
            index(entry.second.cache.total() - 1, COLUMN_CHECKBOX, parent));
    }
}

// ---- sort / mode --------------------------------------------------------

void CoinSelectionModel::sort(int column, Qt::SortOrder order)
{
    if (column == COLUMN_CHECKBOX) return;
    const int sort_column = column - 1; // the static_assert-pinned mirror offset
    m_source.setViewSort(m_view_id, sort_column, order == Qt::DescendingOrder ? 1 : 0);
    // Pull the Reset through immediately so the header click feels
    // synchronous (the drain applies it via reseedFromSource).
    m_wallet_model->drainCoinEventQueue();
}

void CoinSelectionModel::setDisplayMode(GRC::CoinViewMode mode)
{
    if (mode == m_mode) return;
    m_mode = mode;
    m_source.setViewMode(m_view_id, mode);
    m_wallet_model->drainCoinEventQueue();
}

// ---- event application --------------------------------------------------

void CoinSelectionModel::reseedFromSource()
{
    beginResetModel();
    m_groups.clear();
    m_dir_row.clear();
    m_addr_id.clear();
    m_id_addr.clear();
    m_directory.clear();
    m_pending_fetch.clear();

    if (m_mode == GRC::CoinViewMode::Tree) {
        GRC::CoinGroupsResult result = m_source.getGroups(m_view_id, 0, -1);
        m_directory = std::move(result.groups);
        for (std::size_t i = 0; i < m_directory.size(); ++i) {
            m_dir_row.emplace(m_directory[i].address, static_cast<int>(i));
            idForAddress(m_directory[i].address);
        }
        m_flat.seedInitial({}, 0, 0, result.epoch, result.high_water);
    } else {
        GRC::CoinRowsResult result = m_source.getRows(m_view_id, 0, kInitialWindow);
        m_flat.seedInitial(std::move(result.records), 0, result.total_accepted,
                           result.epoch, result.high_water);
    }
    endResetModel();

    if (m_loading) {
        m_loading = false;
        emit loadingFinished();
    }
}

void CoinSelectionModel::applyCoinEventBatch(const std::vector<GRC::WalletCoinEvent>& events)
{
    if (m_applying_events) return;
    m_applying_events = true;
    struct Guard { bool& f; ~Guard() { f = false; } } guard{m_applying_events};

    for (const GRC::WalletCoinEvent& ev : events) {
        if (const auto* p = std::get_if<GRC::CoinDepthRefreshPayload>(&ev.payload)) {
            m_tip_height = p->height;
            // Repaint the confirmations column of whatever is cached.
            if (m_mode == GRC::CoinViewMode::Flat && m_flat.cacheSize() > 0) {
                emit QAbstractItemModel::dataChanged(
                    createIndex(m_flat.cacheFirst(), COLUMN_CONFIRMATIONS, quintptr(0)),
                    createIndex(m_flat.cacheFirst() + m_flat.cacheSize() - 1,
                                COLUMN_CONFIRMATIONS, quintptr(0)));
            }
            for (auto& entry : m_groups) {
                if (entry.second.cache.cacheSize() <= 0) continue;
                const QModelIndex parent = groupIndexByAddress(entry.first);
                if (!parent.isValid()) continue;
                emit QAbstractItemModel::dataChanged(
                    index(entry.second.cache.cacheFirst(), COLUMN_CONFIRMATIONS, parent),
                    index(entry.second.cache.cacheFirst() + entry.second.cache.cacheSize() - 1,
                          COLUMN_CONFIRMATIONS, parent));
            }
            continue;
        }

        if (const auto* p = std::get_if<GRC::CoinResetPayload>(&ev.payload)) {
            if (p->view_id == m_view_id) reseedFromSource();
            continue;
        }

        if (const auto* p = std::get_if<GRC::CoinRowsRemovedPayload>(&ev.payload)) {
            if (p->view_id != m_view_id) continue;
            // Selection pruning is UNCONDITIONAL — before, and independent
            // of, the seqno-gated cache application (idempotent; a cache
            // reseeded past this event still needs the coinControl prune).
            for (const COutPoint& outpoint : p->outpoints) {
                m_coin_control->UnSelect(outpoint);
            }
            if (p->scope.empty()) {
                if (m_mode == GRC::CoinViewMode::Flat) {
                    m_flat.applyRemove(*m_flat_sink, ev.seqno, p->position, p->count);
                }
            } else {
                auto it = m_groups.find(p->scope);
                if (it != m_groups.end()) {
                    it->second.cache.applyRemove(*it->second.sink, ev.seqno, p->position, p->count);
                }
            }
            emit selectionChanged();
            continue;
        }

        if (const auto* p = std::get_if<GRC::CoinRowsInsertedPayload>(&ev.payload)) {
            if (p->view_id != m_view_id) continue;
            if (p->scope.empty()) {
                if (m_mode == GRC::CoinViewMode::Flat) {
                    m_flat.applyInsert(*m_flat_sink, ev.seqno, p->position, p->records);
                }
            } else {
                auto it = m_groups.find(p->scope);
                if (it != m_groups.end()) {
                    it->second.cache.applyInsert(*it->second.sink, ev.seqno, p->position, p->records);
                }
            }
            continue;
        }

        if (const auto* p = std::get_if<GRC::CoinRowsChangedPayload>(&ev.payload)) {
            if (p->view_id != m_view_id) continue;
            GRC::WindowCache<GRC::CoinRecord>* cache = nullptr;
            GRC::WindowCacheSink* sink = nullptr;
            if (p->scope.empty()) {
                if (m_mode == GRC::CoinViewMode::Flat) {
                    cache = &m_flat;
                    sink = m_flat_sink.get();
                }
            } else {
                auto it = m_groups.find(p->scope);
                if (it != m_groups.end()) {
                    cache = &it->second.cache;
                    sink = it->second.sink.get();
                }
            }
            if (cache) {
                GRC::CoinRowsResult fresh = p->scope.empty()
                    ? m_source.getRows(m_view_id, p->first, p->count)
                    : m_source.getGroupRows(m_view_id, p->scope, p->first, p->count);
                cache->applyChange(*sink, ev.seqno, p->first, p->count, fresh.records);
            }
            continue;
        }

        if (const auto* p = std::get_if<GRC::CoinGroupsInsertedPayload>(&ev.payload)) {
            if (p->view_id != m_view_id || m_mode != GRC::CoinViewMode::Tree) continue;
            const int pos = std::min(p->position, static_cast<int>(m_directory.size()));
            beginInsertRows(QModelIndex(), pos, pos + static_cast<int>(p->groups.size()) - 1);
            m_directory.insert(m_directory.begin() + pos, p->groups.begin(), p->groups.end());
            rebuildDirRows(pos);
            for (const GRC::CoinGroupInfo& g : p->groups) idForAddress(g.address);
            endInsertRows();
            continue;
        }

        if (const auto* p = std::get_if<GRC::CoinGroupsRemovedPayload>(&ev.payload)) {
            if (p->view_id != m_view_id || m_mode != GRC::CoinViewMode::Tree) continue;
            if (p->position < 0 || p->position + p->count > static_cast<int>(m_directory.size())) {
                continue;
            }
            // Drop any realized child caches of the removed rows first (the
            // branch disappears with its parent).
            for (int i = 0; i < p->count; ++i) {
                m_groups.erase(m_directory[static_cast<std::size_t>(p->position + i)].address);
            }
            beginRemoveRows(QModelIndex(), p->position, p->position + p->count - 1);
            m_directory.erase(m_directory.begin() + p->position,
                              m_directory.begin() + p->position + p->count);
            rebuildDirRows(p->position);
            endRemoveRows();
            continue;
        }

        if (const auto* p = std::get_if<GRC::CoinGroupsChangedPayload>(&ev.payload)) {
            if (p->view_id != m_view_id || m_mode != GRC::CoinViewMode::Tree) continue;
            if (p->first < 0 || p->first + p->count > static_cast<int>(m_directory.size())) {
                continue;
            }
            GRC::CoinGroupsResult fresh = m_source.getGroups(m_view_id, p->first, p->count);
            for (int i = 0; i < static_cast<int>(fresh.groups.size()) && i < p->count; ++i) {
                // Positions are event-replayed; only refresh values for rows
                // whose address still matches (a mid-batch move is corrected
                // by its own Remove+Insert events).
                GRC::CoinGroupInfo& dst = m_directory[static_cast<std::size_t>(p->first + i)];
                if (dst.address == fresh.groups[static_cast<std::size_t>(i)].address) {
                    dst = fresh.groups[static_cast<std::size_t>(i)];
                }
            }
            emit QAbstractItemModel::dataChanged(
                createIndex(p->first, COLUMN_CHECKBOX, quintptr(0)),
                createIndex(p->first + p->count - 1, COLUMN_AMOUNT, quintptr(0)));
            continue;
        }
    }
}

void CoinSelectionModel::rebuildDirRows(int from)
{
    // Directory rows shifted at/after `from`: rebuild the address -> row map
    // for the tail (the head is unchanged).
    for (std::size_t i = static_cast<std::size_t>(std::max(from, 0)); i < m_directory.size(); ++i) {
        m_dir_row[m_directory[i].address] = static_cast<int>(i);
    }
    // Drop stale entries that no longer name a row (erased groups).
    for (auto it = m_dir_row.begin(); it != m_dir_row.end();) {
        const std::size_t row = static_cast<std::size_t>(it->second);
        if (row >= m_directory.size() || m_directory[row].address != it->first) {
            it = m_dir_row.erase(it);
        } else {
            ++it;
        }
    }
}

// ---- content fetches ----------------------------------------------------

void CoinSelectionModel::onVisibleIndexes(const QList<QModelIndex>& visible)
{
    // Bucket visible child/flat rows per scope; group headers need no fetch
    // (the directory is materialized).
    for (const QModelIndex& idx : visible) {
        if (!idx.isValid()) continue;
        std::string scope;
        if (m_mode == GRC::CoinViewMode::Flat) {
            if (idx.internalId() != 0) continue;
        } else {
            if (idx.internalId() == 0) continue; // header
            const std::string* address = addressForId(idx.internalId());
            if (!address) continue;
            scope = *address;
        }
        auto it = m_pending_fetch.find(scope);
        if (it == m_pending_fetch.end()) {
            m_pending_fetch.emplace(scope, std::make_pair(idx.row(), idx.row()));
        } else {
            it->second.first = std::min(it->second.first, idx.row());
            it->second.second = std::max(it->second.second, idx.row());
        }
    }
    if (!m_pending_fetch.empty()) {
        m_fetch_timer->start(kDebounceMs);
    }
}

void CoinSelectionModel::onFetchTimeout()
{
    auto pending = std::move(m_pending_fetch);
    m_pending_fetch.clear();
    for (const auto& entry : pending) {
        const int first = std::max(0, entry.second.first - kInitialWindow);
        const int count = std::min(kMaxFetch,
                                   entry.second.second - first + 1 + kInitialWindow);
        fetchScope(entry.first, first, count);
    }
}

void CoinSelectionModel::fetchScope(const std::string& scope, int first, int count)
{
    GRC::WindowCache<GRC::CoinRecord>* cache = nullptr;
    GRC::WindowCacheSink* sink = nullptr;
    if (scope.empty()) {
        if (m_mode != GRC::CoinViewMode::Flat) return;
        cache = &m_flat;
        sink = m_flat_sink.get();
    } else {
        auto it = m_groups.find(scope);
        if (it == m_groups.end()) return;
        cache = &it->second.cache;
        sink = it->second.sink.get();
    }

    // Skip if the requested range is already cached.
    if (cache->has(first) && cache->has(first + count - 1)) return;

    // Drain first so the cache's structural seqno reflects every pushed
    // event, then fetch; a gate rejection means a producer raced the read —
    // re-arm the debounce for a bounded retry.
    m_wallet_model->drainCoinEventQueue();

    GRC::CoinRowsResult result = scope.empty()
        ? m_source.getRows(m_view_id, first, count)
        : m_source.getGroupRows(m_view_id, scope, first, count);

    if (!cache->fillContent(*sink, first, std::move(result.records),
                            result.epoch, result.high_water)) {
        auto it = m_pending_fetch.find(scope);
        if (it == m_pending_fetch.end()) {
            m_pending_fetch.emplace(scope, std::make_pair(first, first + count - 1));
        }
        m_fetch_timer->start(kDebounceMs);
    }
}

// ---- dialog accessors ---------------------------------------------------

bool CoinSelectionModel::outpointAt(const QModelIndex& index, COutPoint& out) const
{
    const GRC::CoinRecord* rec = recordAt(index);
    if (!rec || isGroup(index)) return false;
    out = rec->outpoint;
    return true;
}

QString CoinSelectionModel::addressAt(const QModelIndex& index) const
{
    if (isGroup(index)) {
        return QString::fromStdString(m_directory[static_cast<std::size_t>(index.row())].address);
    }
    const GRC::CoinRecord* rec = recordAt(index);
    if (!rec) return QString();
    // Parity with the legacy context menu: an empty child address cell falls
    // back to the parent's address.
    if (m_mode == GRC::CoinViewMode::Tree && !rec->is_change) {
        return QString::fromStdString(rec->group_address);
    }
    return QString::fromStdString(rec->address);
}

QString CoinSelectionModel::labelAt(const QModelIndex& index) const
{
    if (isGroup(index)) {
        const GRC::CoinGroupInfo& g = m_directory[static_cast<std::size_t>(index.row())];
        return g.label.empty() ? tr("(no label)") : QString::fromStdString(g.label);
    }
    const GRC::CoinRecord* rec = recordAt(index);
    if (!rec) return QString();
    if (rec->is_change) return tr("(change)");
    return rec->label.empty() ? tr("(no label)") : QString::fromStdString(rec->label);
}

QString CoinSelectionModel::amountTextAt(const QModelIndex& index) const
{
    return data(index.siblingAtColumn(COLUMN_AMOUNT), Qt::DisplayRole).toString();
}

QString CoinSelectionModel::txHashTextAt(const QModelIndex& index) const
{
    COutPoint outpoint;
    if (!outpointAt(index, outpoint)) return QString();
    return QString::fromStdString(outpoint.hash.GetHex());
}

std::vector<GRC::CoinGroupInfo> CoinSelectionModel::groupDirectory() const
{
    return m_source.getGroupDirectory();
}
