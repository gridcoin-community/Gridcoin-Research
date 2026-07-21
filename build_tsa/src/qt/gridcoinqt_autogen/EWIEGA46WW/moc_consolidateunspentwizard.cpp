/****************************************************************************
** Meta object code from reading C++ file 'consolidateunspentwizard.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/consolidateunspentwizard.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'consolidateunspentwizard.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN24ConsolidateUnspentWizardE_t {};
} // unnamed namespace

template <> constexpr inline auto ConsolidateUnspentWizard::qt_create_metaobjectdata<qt_meta_tag_ZN24ConsolidateUnspentWizardE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ConsolidateUnspentWizard",
        "passCoinControlSignal",
        "",
        "interfaces::WalletCoinControl*",
        "selectedConsolidationRecipientSignal",
        "SendCoinsRecipient",
        "sendConsolidationTransactionSignal"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'passCoinControlSignal'
        QtMocHelpers::SignalData<void(interfaces::WalletCoinControl *)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 2 },
        }}),
        // Signal 'selectedConsolidationRecipientSignal'
        QtMocHelpers::SignalData<void(SendCoinsRecipient)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 2 },
        }}),
        // Signal 'sendConsolidationTransactionSignal'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ConsolidateUnspentWizard, qt_meta_tag_ZN24ConsolidateUnspentWizardE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ConsolidateUnspentWizard::staticMetaObject = { {
    QMetaObject::SuperData::link<QWizard::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN24ConsolidateUnspentWizardE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN24ConsolidateUnspentWizardE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN24ConsolidateUnspentWizardE_t>.metaTypes,
    nullptr
} };

void ConsolidateUnspentWizard::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ConsolidateUnspentWizard *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->passCoinControlSignal((*reinterpret_cast< std::add_pointer_t<interfaces::WalletCoinControl*>>(_a[1]))); break;
        case 1: _t->selectedConsolidationRecipientSignal((*reinterpret_cast< std::add_pointer_t<SendCoinsRecipient>>(_a[1]))); break;
        case 2: _t->sendConsolidationTransactionSignal(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ConsolidateUnspentWizard::*)(interfaces::WalletCoinControl * )>(_a, &ConsolidateUnspentWizard::passCoinControlSignal, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsolidateUnspentWizard::*)(SendCoinsRecipient )>(_a, &ConsolidateUnspentWizard::selectedConsolidationRecipientSignal, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsolidateUnspentWizard::*)()>(_a, &ConsolidateUnspentWizard::sendConsolidationTransactionSignal, 2))
            return;
    }
}

const QMetaObject *ConsolidateUnspentWizard::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ConsolidateUnspentWizard::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN24ConsolidateUnspentWizardE_t>.strings))
        return static_cast<void*>(this);
    return QWizard::qt_metacast(_clname);
}

int ConsolidateUnspentWizard::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWizard::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void ConsolidateUnspentWizard::passCoinControlSignal(interfaces::WalletCoinControl * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ConsolidateUnspentWizard::selectedConsolidationRecipientSignal(SendCoinsRecipient _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void ConsolidateUnspentWizard::sendConsolidationTransactionSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
