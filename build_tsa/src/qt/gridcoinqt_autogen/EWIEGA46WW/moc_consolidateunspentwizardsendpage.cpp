/****************************************************************************
** Meta object code from reading C++ file 'consolidateunspentwizardsendpage.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/consolidateunspentwizardsendpage.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'consolidateunspentwizardsendpage.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN32ConsolidateUnspentWizardSendPageE_t {};
} // unnamed namespace

template <> constexpr inline auto ConsolidateUnspentWizardSendPage::qt_create_metaobjectdata<qt_meta_tag_ZN32ConsolidateUnspentWizardSendPageE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ConsolidateUnspentWizardSendPage",
        "selectedConsolidationRecipientSignal",
        "",
        "SendCoinsRecipient",
        "consolidationRecipient",
        "onFinishButtonClicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'selectedConsolidationRecipientSignal'
        QtMocHelpers::SignalData<void(SendCoinsRecipient)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'onFinishButtonClicked'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ConsolidateUnspentWizardSendPage, qt_meta_tag_ZN32ConsolidateUnspentWizardSendPageE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ConsolidateUnspentWizardSendPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWizardPage::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN32ConsolidateUnspentWizardSendPageE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN32ConsolidateUnspentWizardSendPageE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN32ConsolidateUnspentWizardSendPageE_t>.metaTypes,
    nullptr
} };

void ConsolidateUnspentWizardSendPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ConsolidateUnspentWizardSendPage *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->selectedConsolidationRecipientSignal((*reinterpret_cast< std::add_pointer_t<SendCoinsRecipient>>(_a[1]))); break;
        case 1: _t->onFinishButtonClicked(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ConsolidateUnspentWizardSendPage::*)(SendCoinsRecipient )>(_a, &ConsolidateUnspentWizardSendPage::selectedConsolidationRecipientSignal, 0))
            return;
    }
}

const QMetaObject *ConsolidateUnspentWizardSendPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ConsolidateUnspentWizardSendPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN32ConsolidateUnspentWizardSendPageE_t>.strings))
        return static_cast<void*>(this);
    return QWizardPage::qt_metacast(_clname);
}

int ConsolidateUnspentWizardSendPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWizardPage::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void ConsolidateUnspentWizardSendPage::selectedConsolidationRecipientSignal(SendCoinsRecipient _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
QT_WARNING_POP
