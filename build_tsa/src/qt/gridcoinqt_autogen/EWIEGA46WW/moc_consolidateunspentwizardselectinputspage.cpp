/****************************************************************************
** Meta object code from reading C++ file 'consolidateunspentwizardselectinputspage.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/consolidateunspentwizardselectinputspage.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'consolidateunspentwizardselectinputspage.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN40ConsolidateUnspentWizardSelectInputsPageE_t {};
} // unnamed namespace

template <> constexpr inline auto ConsolidateUnspentWizardSelectInputsPage::qt_create_metaobjectdata<qt_meta_tag_ZN40ConsolidateUnspentWizardSelectInputsPageE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ConsolidateUnspentWizardSelectInputsPage",
        "setAddressListSignal",
        "",
        "std::map<QString,QString>",
        "setDefaultAddressSignal",
        "updateFieldsSignal",
        "updateLabels",
        "treeModeRadioButton",
        "listModeRadioButton",
        "viewItemChanged",
        "QTreeWidgetItem*",
        "headerSectionClicked",
        "buttonSelectAllClicked",
        "maxMinOutputValueChanged",
        "buttonFilterModeClicked",
        "buttonFilterClicked",
        "SetOutputWarningStop",
        "InputStatus",
        "input_status"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'setAddressListSignal'
        QtMocHelpers::SignalData<void(std::map<QString,QString>)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 2 },
        }}),
        // Signal 'setDefaultAddressSignal'
        QtMocHelpers::SignalData<void(QString)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 2 },
        }}),
        // Signal 'updateFieldsSignal'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'updateLabels'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'treeModeRadioButton'
        QtMocHelpers::SlotData<void(bool)>(7, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
        // Slot 'listModeRadioButton'
        QtMocHelpers::SlotData<void(bool)>(8, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
        // Slot 'viewItemChanged'
        QtMocHelpers::SlotData<void(QTreeWidgetItem *, int)>(9, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 10, 2 }, { QMetaType::Int, 2 },
        }}),
        // Slot 'headerSectionClicked'
        QtMocHelpers::SlotData<void(int)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 2 },
        }}),
        // Slot 'buttonSelectAllClicked'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'maxMinOutputValueChanged'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'buttonFilterModeClicked'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'buttonFilterClicked'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'SetOutputWarningStop'
        QtMocHelpers::SlotData<void(InputStatus)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 17, 18 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ConsolidateUnspentWizardSelectInputsPage, qt_meta_tag_ZN40ConsolidateUnspentWizardSelectInputsPageE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ConsolidateUnspentWizardSelectInputsPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWizardPage::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN40ConsolidateUnspentWizardSelectInputsPageE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN40ConsolidateUnspentWizardSelectInputsPageE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN40ConsolidateUnspentWizardSelectInputsPageE_t>.metaTypes,
    nullptr
} };

void ConsolidateUnspentWizardSelectInputsPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ConsolidateUnspentWizardSelectInputsPage *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->setAddressListSignal((*reinterpret_cast< std::add_pointer_t<std::map<QString,QString>>>(_a[1]))); break;
        case 1: _t->setDefaultAddressSignal((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->updateFieldsSignal(); break;
        case 3: _t->updateLabels(); break;
        case 4: _t->treeModeRadioButton((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->listModeRadioButton((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->viewItemChanged((*reinterpret_cast< std::add_pointer_t<QTreeWidgetItem*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 7: _t->headerSectionClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->buttonSelectAllClicked(); break;
        case 9: _t->maxMinOutputValueChanged(); break;
        case 10: _t->buttonFilterModeClicked(); break;
        case 11: _t->buttonFilterClicked(); break;
        case 12: _t->SetOutputWarningStop((*reinterpret_cast< std::add_pointer_t<InputStatus>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ConsolidateUnspentWizardSelectInputsPage::*)(std::map<QString,QString> )>(_a, &ConsolidateUnspentWizardSelectInputsPage::setAddressListSignal, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsolidateUnspentWizardSelectInputsPage::*)(QString )>(_a, &ConsolidateUnspentWizardSelectInputsPage::setDefaultAddressSignal, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsolidateUnspentWizardSelectInputsPage::*)()>(_a, &ConsolidateUnspentWizardSelectInputsPage::updateFieldsSignal, 2))
            return;
    }
}

const QMetaObject *ConsolidateUnspentWizardSelectInputsPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ConsolidateUnspentWizardSelectInputsPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN40ConsolidateUnspentWizardSelectInputsPageE_t>.strings))
        return static_cast<void*>(this);
    return QWizardPage::qt_metacast(_clname);
}

int ConsolidateUnspentWizardSelectInputsPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWizardPage::qt_metacall(_c, _id, _a);
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
void ConsolidateUnspentWizardSelectInputsPage::setAddressListSignal(std::map<QString,QString> _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ConsolidateUnspentWizardSelectInputsPage::setDefaultAddressSignal(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void ConsolidateUnspentWizardSelectInputsPage::updateFieldsSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
