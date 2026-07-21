/****************************************************************************
** Meta object code from reading C++ file 'overviewpage.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/overviewpage.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'overviewpage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN12OverviewPageE_t {};
} // unnamed namespace

template <> constexpr inline auto OverviewPage::qt_create_metaobjectdata<qt_meta_tag_ZN12OverviewPageE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "OverviewPage",
        "transactionClicked",
        "",
        "QModelIndex",
        "index",
        "pollLabelClicked",
        "setBalance",
        "balance",
        "stake",
        "unconfirmedBalance",
        "immatureBalance",
        "setHeight",
        "height",
        "height_of_peers",
        "in_sync",
        "setDifficulty",
        "difficulty",
        "net_weight",
        "setCoinWeight",
        "coin_weight",
        "setCurrentPollTitle",
        "title",
        "setPrivacy",
        "privacy",
        "showHideMRCToolButton",
        "updateDisplayUnit",
        "updateTransactions",
        "updateResearcherStatus",
        "updateMagnitude",
        "updatePendingAccrual",
        "updateResearcherAlert",
        "onBeaconButtonClicked",
        "onMRCRequestClicked",
        "handleTransactionClicked",
        "handlePollLabelClicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'transactionClicked'
        QtMocHelpers::SignalData<void(const QModelIndex &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'pollLabelClicked'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setBalance'
        QtMocHelpers::SlotData<void(qint64, qint64, qint64, qint64)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 7 }, { QMetaType::LongLong, 8 }, { QMetaType::LongLong, 9 }, { QMetaType::LongLong, 10 },
        }}),
        // Slot 'setHeight'
        QtMocHelpers::SlotData<void(int, int, bool)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 12 }, { QMetaType::Int, 13 }, { QMetaType::Bool, 14 },
        }}),
        // Slot 'setDifficulty'
        QtMocHelpers::SlotData<void(double, double)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 16 }, { QMetaType::Double, 17 },
        }}),
        // Slot 'setCoinWeight'
        QtMocHelpers::SlotData<void(double)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 19 },
        }}),
        // Slot 'setCurrentPollTitle'
        QtMocHelpers::SlotData<void(const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Slot 'setPrivacy'
        QtMocHelpers::SlotData<void(bool)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 23 },
        }}),
        // Slot 'showHideMRCToolButton'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'updateDisplayUnit'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateTransactions'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateResearcherStatus'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateMagnitude'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updatePendingAccrual'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateResearcherAlert'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onBeaconButtonClicked'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onMRCRequestClicked'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleTransactionClicked'
        QtMocHelpers::SlotData<void(const QModelIndex &)>(33, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'handlePollLabelClicked'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<OverviewPage, qt_meta_tag_ZN12OverviewPageE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject OverviewPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12OverviewPageE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12OverviewPageE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12OverviewPageE_t>.metaTypes,
    nullptr
} };

void OverviewPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<OverviewPage *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->transactionClicked((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 1: _t->pollLabelClicked(); break;
        case 2: _t->setBalance((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4]))); break;
        case 3: _t->setHeight((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3]))); break;
        case 4: _t->setDifficulty((*reinterpret_cast< std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[2]))); break;
        case 5: _t->setCoinWeight((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 6: _t->setCurrentPollTitle((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->setPrivacy((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 8: _t->showHideMRCToolButton(); break;
        case 9: _t->updateDisplayUnit(); break;
        case 10: _t->updateTransactions(); break;
        case 11: _t->updateResearcherStatus(); break;
        case 12: _t->updateMagnitude(); break;
        case 13: _t->updatePendingAccrual(); break;
        case 14: _t->updateResearcherAlert(); break;
        case 15: _t->onBeaconButtonClicked(); break;
        case 16: _t->onMRCRequestClicked(); break;
        case 17: _t->handleTransactionClicked((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 18: _t->handlePollLabelClicked(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (OverviewPage::*)(const QModelIndex & )>(_a, &OverviewPage::transactionClicked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (OverviewPage::*)()>(_a, &OverviewPage::pollLabelClicked, 1))
            return;
    }
}

const QMetaObject *OverviewPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *OverviewPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12OverviewPageE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int OverviewPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 19)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 19;
    }
    return _id;
}

// SIGNAL 0
void OverviewPage::transactionClicked(const QModelIndex & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void OverviewPage::pollLabelClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
