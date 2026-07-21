/****************************************************************************
** Meta object code from reading C++ file 'walletmodel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/walletmodel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'walletmodel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11WalletModelE_t {};
} // unnamed namespace

template <> constexpr inline auto WalletModel::qt_create_metaobjectdata<qt_meta_tag_ZN11WalletModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "WalletModel",
        "transactionUpdated",
        "",
        "walletEventsDrained",
        "std::vector<GRC::WalletEvent>",
        "events",
        "balanceChanged",
        "balance",
        "stake",
        "unconfirmedBalance",
        "immatureBalance",
        "numTransactionsChanged",
        "count",
        "encryptionStatusChanged",
        "status",
        "requireUnlock",
        "error",
        "title",
        "message",
        "modal",
        "updateStatus",
        "updateAddressBook",
        "address",
        "label",
        "isMine",
        "drainEventQueue"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'transactionUpdated'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'walletEventsDrained'
        QtMocHelpers::SignalData<void(const std::vector<GRC::WalletEvent> &)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
        // Signal 'balanceChanged'
        QtMocHelpers::SignalData<void(qint64, qint64, qint64, qint64)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 7 }, { QMetaType::LongLong, 8 }, { QMetaType::LongLong, 9 }, { QMetaType::LongLong, 10 },
        }}),
        // Signal 'numTransactionsChanged'
        QtMocHelpers::SignalData<void(int)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 12 },
        }}),
        // Signal 'encryptionStatusChanged'
        QtMocHelpers::SignalData<void(int)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 14 },
        }}),
        // Signal 'requireUnlock'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'error'
        QtMocHelpers::SignalData<void(const QString &, const QString &, bool)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 17 }, { QMetaType::QString, 18 }, { QMetaType::Bool, 19 },
        }}),
        // Slot 'updateStatus'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'updateAddressBook'
        QtMocHelpers::SlotData<void(const QString &, const QString &, bool, int)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 }, { QMetaType::QString, 23 }, { QMetaType::Bool, 24 }, { QMetaType::Int, 14 },
        }}),
        // Slot 'drainEventQueue'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<WalletModel, qt_meta_tag_ZN11WalletModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject WalletModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11WalletModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11WalletModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11WalletModelE_t>.metaTypes,
    nullptr
} };

void WalletModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<WalletModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->transactionUpdated(); break;
        case 1: _t->walletEventsDrained((*reinterpret_cast< std::add_pointer_t<std::vector<GRC::WalletEvent>>>(_a[1]))); break;
        case 2: _t->balanceChanged((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4]))); break;
        case 3: _t->numTransactionsChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->encryptionStatusChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->requireUnlock(); break;
        case 6: _t->error((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3]))); break;
        case 7: _t->updateStatus(); break;
        case 8: _t->updateAddressBook((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 9: _t->drainEventQueue(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (WalletModel::*)()>(_a, &WalletModel::transactionUpdated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (WalletModel::*)(const std::vector<GRC::WalletEvent> & )>(_a, &WalletModel::walletEventsDrained, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (WalletModel::*)(qint64 , qint64 , qint64 , qint64 )>(_a, &WalletModel::balanceChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (WalletModel::*)(int )>(_a, &WalletModel::numTransactionsChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (WalletModel::*)(int )>(_a, &WalletModel::encryptionStatusChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (WalletModel::*)()>(_a, &WalletModel::requireUnlock, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (WalletModel::*)(const QString & , const QString & , bool )>(_a, &WalletModel::error, 6))
            return;
    }
}

const QMetaObject *WalletModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WalletModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11WalletModelE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int WalletModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void WalletModel::transactionUpdated()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void WalletModel::walletEventsDrained(const std::vector<GRC::WalletEvent> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void WalletModel::balanceChanged(qint64 _t1, qint64 _t2, qint64 _t3, qint64 _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 3
void WalletModel::numTransactionsChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void WalletModel::encryptionStatusChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void WalletModel::requireUnlock()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void WalletModel::error(const QString & _t1, const QString & _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
