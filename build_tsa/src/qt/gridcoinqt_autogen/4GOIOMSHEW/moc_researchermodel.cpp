/****************************************************************************
** Meta object code from reading C++ file 'researchermodel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/researcher/researchermodel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'researchermodel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15ResearcherModelE_t {};
} // unnamed namespace

template <> constexpr inline auto ResearcherModel::qt_create_metaobjectdata<qt_meta_tag_ZN15ResearcherModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ResearcherModel",
        "researcherChanged",
        "",
        "beaconChanged",
        "magnitudeChanged",
        "accrualChanged",
        "reload",
        "refresh",
        "onResearcherChanged",
        "switchToSolo",
        "email",
        "switchToPool",
        "switchToNoncruncher",
        "updateBeacon",
        "advertiseBeacon",
        "BeaconStatus",
        "onWizardClose"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'researcherChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'beaconChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'magnitudeChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'accrualChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'reload'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'refresh'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onResearcherChanged'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'switchToSolo'
        QtMocHelpers::SlotData<bool(const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 10 },
        }}),
        // Slot 'switchToPool'
        QtMocHelpers::SlotData<bool()>(11, 2, QMC::AccessPublic, QMetaType::Bool),
        // Slot 'switchToNoncruncher'
        QtMocHelpers::SlotData<bool()>(12, 2, QMC::AccessPublic, QMetaType::Bool),
        // Slot 'updateBeacon'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'advertiseBeacon'
        QtMocHelpers::SlotData<BeaconStatus()>(14, 2, QMC::AccessPublic, 0x80000000 | 15),
        // Slot 'onWizardClose'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ResearcherModel, qt_meta_tag_ZN15ResearcherModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ResearcherModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15ResearcherModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15ResearcherModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15ResearcherModelE_t>.metaTypes,
    nullptr
} };

void ResearcherModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ResearcherModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->researcherChanged(); break;
        case 1: _t->beaconChanged(); break;
        case 2: _t->magnitudeChanged(); break;
        case 3: _t->accrualChanged(); break;
        case 4: _t->reload(); break;
        case 5: _t->refresh(); break;
        case 6: _t->onResearcherChanged(); break;
        case 7: { bool _r = _t->switchToSolo((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 8: { bool _r = _t->switchToPool();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 9: { bool _r = _t->switchToNoncruncher();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 10: _t->updateBeacon(); break;
        case 11: { BeaconStatus _r = _t->advertiseBeacon();
            if (_a[0]) *reinterpret_cast< BeaconStatus*>(_a[0]) = std::move(_r); }  break;
        case 12: _t->onWizardClose(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ResearcherModel::*)()>(_a, &ResearcherModel::researcherChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ResearcherModel::*)()>(_a, &ResearcherModel::beaconChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ResearcherModel::*)()>(_a, &ResearcherModel::magnitudeChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ResearcherModel::*)()>(_a, &ResearcherModel::accrualChanged, 3))
            return;
    }
}

const QMetaObject *ResearcherModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ResearcherModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15ResearcherModelE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ResearcherModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }
    return _id;
}

// SIGNAL 0
void ResearcherModel::researcherChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ResearcherModel::beaconChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ResearcherModel::magnitudeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ResearcherModel::accrualChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
