/****************************************************************************
** Meta object code from reading C++ file 'consolidateunspentwizardselectdestinationpage.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/consolidateunspentwizardselectdestinationpage.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'consolidateunspentwizardselectdestinationpage.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN45ConsolidateUnspentWizardSelectDestinationPageE_t {};
} // unnamed namespace

template <> constexpr inline auto ConsolidateUnspentWizardSelectDestinationPage::qt_create_metaobjectdata<qt_meta_tag_ZN45ConsolidateUnspentWizardSelectDestinationPageE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ConsolidateUnspentWizardSelectDestinationPage",
        "updateFieldsSignal",
        "",
        "SetAddressList",
        "std::map<QString,QString>",
        "addressList",
        "setDefaultAddressSelection",
        "address",
        "addressSelectionChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'updateFieldsSignal'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'SetAddressList'
        QtMocHelpers::SlotData<void(std::map<QString,QString>)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
        // Slot 'setDefaultAddressSelection'
        QtMocHelpers::SlotData<void(QString)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
        // Slot 'addressSelectionChanged'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ConsolidateUnspentWizardSelectDestinationPage, qt_meta_tag_ZN45ConsolidateUnspentWizardSelectDestinationPageE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ConsolidateUnspentWizardSelectDestinationPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWizardPage::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN45ConsolidateUnspentWizardSelectDestinationPageE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN45ConsolidateUnspentWizardSelectDestinationPageE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN45ConsolidateUnspentWizardSelectDestinationPageE_t>.metaTypes,
    nullptr
} };

void ConsolidateUnspentWizardSelectDestinationPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ConsolidateUnspentWizardSelectDestinationPage *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->updateFieldsSignal(); break;
        case 1: _t->SetAddressList((*reinterpret_cast< std::add_pointer_t<std::map<QString,QString>>>(_a[1]))); break;
        case 2: _t->setDefaultAddressSelection((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->addressSelectionChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ConsolidateUnspentWizardSelectDestinationPage::*)()>(_a, &ConsolidateUnspentWizardSelectDestinationPage::updateFieldsSignal, 0))
            return;
    }
}

const QMetaObject *ConsolidateUnspentWizardSelectDestinationPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ConsolidateUnspentWizardSelectDestinationPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN45ConsolidateUnspentWizardSelectDestinationPageE_t>.strings))
        return static_cast<void*>(this);
    return QWizardPage::qt_metacast(_clname);
}

int ConsolidateUnspentWizardSelectDestinationPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWizardPage::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void ConsolidateUnspentWizardSelectDestinationPage::updateFieldsSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
