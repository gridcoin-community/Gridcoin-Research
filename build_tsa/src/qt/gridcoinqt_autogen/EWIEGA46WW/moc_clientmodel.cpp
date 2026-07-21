/****************************************************************************
** Meta object code from reading C++ file 'clientmodel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/clientmodel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'clientmodel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11ClientModelE_t {};
} // unnamed namespace

template <> constexpr inline auto ClientModel::qt_create_metaobjectdata<qt_meta_tag_ZN11ClientModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ClientModel",
        "numConnectionsChanged",
        "",
        "count",
        "numBlocksChanged",
        "countOfPeers",
        "difficultyChanged",
        "difficulty",
        "bytesChanged",
        "totalBytesIn",
        "totalBytesOut",
        "minerStatusChanged",
        "staking",
        "netWeight",
        "coinWeight",
        "etts_days",
        "updateScraperLog",
        "message",
        "updateScraperStatus",
        "ScraperEventtype",
        "status",
        "psgtPoolChanged",
        "revision_hash",
        "change_type",
        "reason",
        "error",
        "title",
        "modal",
        "updateNumBlocks",
        "height",
        "int64_t",
        "best_time",
        "uint32_t",
        "target_bits",
        "updateTimer",
        "updateBanlist",
        "updateNumConnections",
        "numConnections",
        "updateAlert",
        "hash",
        "updateMinerStatus",
        "coin_weight",
        "updateScraper",
        "scraperEventtype",
        "updatePSGTPool"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'numConnectionsChanged'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'numBlocksChanged'
        QtMocHelpers::SignalData<void(int, int)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 5 },
        }}),
        // Signal 'difficultyChanged'
        QtMocHelpers::SignalData<void(double)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 7 },
        }}),
        // Signal 'bytesChanged'
        QtMocHelpers::SignalData<void(quint64, quint64)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 9 }, { QMetaType::ULongLong, 10 },
        }}),
        // Signal 'minerStatusChanged'
        QtMocHelpers::SignalData<void(bool, double, double, double)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 12 }, { QMetaType::Double, 13 }, { QMetaType::Double, 14 }, { QMetaType::Double, 15 },
        }}),
        // Signal 'updateScraperLog'
        QtMocHelpers::SignalData<void(QString)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 17 },
        }}),
        // Signal 'updateScraperStatus'
        QtMocHelpers::SignalData<void(int, int)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 19 }, { QMetaType::Int, 20 },
        }}),
        // Signal 'psgtPoolChanged'
        QtMocHelpers::SignalData<void(QString, quint8, int)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 }, { QMetaType::UChar, 23 }, { QMetaType::Int, 24 },
        }}),
        // Signal 'error'
        QtMocHelpers::SignalData<void(const QString &, const QString &, bool)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 26 }, { QMetaType::QString, 17 }, { QMetaType::Bool, 27 },
        }}),
        // Slot 'updateNumBlocks'
        QtMocHelpers::SlotData<void(int, int64_t, uint32_t)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 29 }, { 0x80000000 | 30, 31 }, { 0x80000000 | 32, 33 },
        }}),
        // Slot 'updateTimer'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'updateBanlist'
        QtMocHelpers::SlotData<void()>(35, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'updateNumConnections'
        QtMocHelpers::SlotData<void(int)>(36, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 37 },
        }}),
        // Slot 'updateAlert'
        QtMocHelpers::SlotData<void(const QString &, int)>(38, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 39 }, { QMetaType::Int, 20 },
        }}),
        // Slot 'updateMinerStatus'
        QtMocHelpers::SlotData<void(bool, double)>(40, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 12 }, { QMetaType::Double, 41 },
        }}),
        // Slot 'updateScraper'
        QtMocHelpers::SlotData<void(int, int, const QString)>(42, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 43 }, { QMetaType::Int, 20 }, { QMetaType::QString, 17 },
        }}),
        // Slot 'updatePSGTPool'
        QtMocHelpers::SlotData<void(const QString &, int, int)>(44, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 }, { QMetaType::Int, 20 }, { QMetaType::Int, 24 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ClientModel, qt_meta_tag_ZN11ClientModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ClientModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11ClientModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11ClientModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11ClientModelE_t>.metaTypes,
    nullptr
} };

void ClientModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ClientModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->numConnectionsChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->numBlocksChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 2: _t->difficultyChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 3: _t->bytesChanged((*reinterpret_cast< std::add_pointer_t<quint64>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2]))); break;
        case 4: _t->minerStatusChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[4]))); break;
        case 5: _t->updateScraperLog((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->updateScraperStatus((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 7: _t->psgtPoolChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint8>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 8: _t->error((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3]))); break;
        case 9: _t->updateNumBlocks((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<uint32_t>>(_a[3]))); break;
        case 10: _t->updateTimer(); break;
        case 11: _t->updateBanlist(); break;
        case 12: _t->updateNumConnections((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 13: _t->updateAlert((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 14: _t->updateMinerStatus((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[2]))); break;
        case 15: _t->updateScraper((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 16: _t->updatePSGTPool((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ClientModel::*)(int )>(_a, &ClientModel::numConnectionsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ClientModel::*)(int , int )>(_a, &ClientModel::numBlocksChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ClientModel::*)(double )>(_a, &ClientModel::difficultyChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ClientModel::*)(quint64 , quint64 )>(_a, &ClientModel::bytesChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (ClientModel::*)(bool , double , double , double )>(_a, &ClientModel::minerStatusChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (ClientModel::*)(QString )>(_a, &ClientModel::updateScraperLog, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (ClientModel::*)(int , int )>(_a, &ClientModel::updateScraperStatus, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (ClientModel::*)(QString , quint8 , int )>(_a, &ClientModel::psgtPoolChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (ClientModel::*)(const QString & , const QString & , bool )>(_a, &ClientModel::error, 8))
            return;
    }
}

const QMetaObject *ClientModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ClientModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11ClientModelE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ClientModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 17;
    }
    return _id;
}

// SIGNAL 0
void ClientModel::numConnectionsChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ClientModel::numBlocksChanged(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void ClientModel::difficultyChanged(double _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void ClientModel::bytesChanged(quint64 _t1, quint64 _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void ClientModel::minerStatusChanged(bool _t1, double _t2, double _t3, double _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 5
void ClientModel::updateScraperLog(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void ClientModel::updateScraperStatus(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void ClientModel::psgtPoolChanged(QString _t1, quint8 _t2, int _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2, _t3);
}

// SIGNAL 8
void ClientModel::error(const QString & _t1, const QString & _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
