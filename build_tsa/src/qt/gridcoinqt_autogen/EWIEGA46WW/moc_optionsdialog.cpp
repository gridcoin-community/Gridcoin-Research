/****************************************************************************
** Meta object code from reading C++ file 'optionsdialog.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/optionsdialog.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'optionsdialog.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13OptionsDialogE_t {};
} // unnamed namespace

template <> constexpr inline auto OptionsDialog::qt_create_metaobjectdata<qt_meta_tag_ZN13OptionsDialogE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "OptionsDialog",
        "proxyIpValid",
        "",
        "QValidatedLineEdit*",
        "object",
        "fValid",
        "stakingEfficiencyValid",
        "minStakeSplitValueValid",
        "pollExpireNotifyValid",
        "sidestakeAllocationInvalid",
        "sidestakeDescriptionInvalid",
        "resizeSideStakeTableColumns",
        "neighbor_pair_adjust",
        "index",
        "old_size",
        "new_size",
        "enableApplyButton",
        "disableApplyButton",
        "enableSaveButtons",
        "disableSaveButtons",
        "setSaveButtonState",
        "fState",
        "on_okButton_clicked",
        "on_cancelButton_clicked",
        "on_applyButton_clicked",
        "newSideStakeButton_clicked",
        "editSideStakeButton_clicked",
        "deleteSideStakeButton_clicked",
        "showRestartWarning_Proxy",
        "showRestartWarning_Lang",
        "updateDisplayUnit",
        "updateStyle",
        "hideStartMinimized",
        "hideLimitTxnDisplayDate",
        "hideStakeSplitting",
        "hidePollExpireNotify",
        "hideSideStakeEdit",
        "handleProxyIpValid",
        "handleStakingEfficiencyValid",
        "handleMinStakeSplitValueValid",
        "handlePollExpireNotifyValid",
        "handleSideStakeAllocationInvalid",
        "handleSideStakeDescriptionInvalid",
        "refreshSideStakeTableModel",
        "tabWidgetSelectionChanged",
        "sidestakeSelectionChanged",
        "updateSideStakeTableView",
        "sidestakeTableSectionResized"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'proxyIpValid'
        QtMocHelpers::SignalData<void(QValidatedLineEdit *, bool)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Bool, 5 },
        }}),
        // Signal 'stakingEfficiencyValid'
        QtMocHelpers::SignalData<void(QValidatedLineEdit *, bool)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Bool, 5 },
        }}),
        // Signal 'minStakeSplitValueValid'
        QtMocHelpers::SignalData<void(QValidatedLineEdit *, bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Bool, 5 },
        }}),
        // Signal 'pollExpireNotifyValid'
        QtMocHelpers::SignalData<void(QValidatedLineEdit *, bool)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Bool, 5 },
        }}),
        // Signal 'sidestakeAllocationInvalid'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'sidestakeDescriptionInvalid'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'resizeSideStakeTableColumns'
        QtMocHelpers::SlotData<void(const bool &, const int &, const int &, const int &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 12 }, { QMetaType::Int, 13 }, { QMetaType::Int, 14 }, { QMetaType::Int, 15 },
        }}),
        // Slot 'resizeSideStakeTableColumns'
        QtMocHelpers::SlotData<void(const bool &, const int &, const int &)>(11, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Bool, 12 }, { QMetaType::Int, 13 }, { QMetaType::Int, 14 },
        }}),
        // Slot 'resizeSideStakeTableColumns'
        QtMocHelpers::SlotData<void(const bool &, const int &)>(11, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Bool, 12 }, { QMetaType::Int, 13 },
        }}),
        // Slot 'resizeSideStakeTableColumns'
        QtMocHelpers::SlotData<void(const bool &)>(11, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Bool, 12 },
        }}),
        // Slot 'resizeSideStakeTableColumns'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Slot 'enableApplyButton'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'disableApplyButton'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'enableSaveButtons'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'disableSaveButtons'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'setSaveButtonState'
        QtMocHelpers::SlotData<void(bool)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'on_okButton_clicked'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_cancelButton_clicked'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_applyButton_clicked'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'newSideStakeButton_clicked'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'editSideStakeButton_clicked'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'deleteSideStakeButton_clicked'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showRestartWarning_Proxy'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showRestartWarning_Lang'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateDisplayUnit'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateStyle'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'hideStartMinimized'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'hideLimitTxnDisplayDate'
        QtMocHelpers::SlotData<void()>(33, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'hideStakeSplitting'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'hidePollExpireNotify'
        QtMocHelpers::SlotData<void()>(35, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'hideSideStakeEdit'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleProxyIpValid'
        QtMocHelpers::SlotData<void(QValidatedLineEdit *, bool)>(37, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Bool, 21 },
        }}),
        // Slot 'handleStakingEfficiencyValid'
        QtMocHelpers::SlotData<void(QValidatedLineEdit *, bool)>(38, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Bool, 21 },
        }}),
        // Slot 'handleMinStakeSplitValueValid'
        QtMocHelpers::SlotData<void(QValidatedLineEdit *, bool)>(39, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Bool, 21 },
        }}),
        // Slot 'handlePollExpireNotifyValid'
        QtMocHelpers::SlotData<void(QValidatedLineEdit *, bool)>(40, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Bool, 21 },
        }}),
        // Slot 'handleSideStakeAllocationInvalid'
        QtMocHelpers::SlotData<void()>(41, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleSideStakeDescriptionInvalid'
        QtMocHelpers::SlotData<void()>(42, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'refreshSideStakeTableModel'
        QtMocHelpers::SlotData<void()>(43, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'tabWidgetSelectionChanged'
        QtMocHelpers::SlotData<void(int)>(44, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 13 },
        }}),
        // Slot 'sidestakeSelectionChanged'
        QtMocHelpers::SlotData<void()>(45, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateSideStakeTableView'
        QtMocHelpers::SlotData<void()>(46, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'sidestakeTableSectionResized'
        QtMocHelpers::SlotData<void(int, int, int)>(47, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 13 }, { QMetaType::Int, 14 }, { QMetaType::Int, 15 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<OptionsDialog, qt_meta_tag_ZN13OptionsDialogE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject OptionsDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13OptionsDialogE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13OptionsDialogE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13OptionsDialogE_t>.metaTypes,
    nullptr
} };

void OptionsDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<OptionsDialog *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->proxyIpValid((*reinterpret_cast< std::add_pointer_t<QValidatedLineEdit*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 1: _t->stakingEfficiencyValid((*reinterpret_cast< std::add_pointer_t<QValidatedLineEdit*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 2: _t->minStakeSplitValueValid((*reinterpret_cast< std::add_pointer_t<QValidatedLineEdit*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 3: _t->pollExpireNotifyValid((*reinterpret_cast< std::add_pointer_t<QValidatedLineEdit*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 4: _t->sidestakeAllocationInvalid(); break;
        case 5: _t->sidestakeDescriptionInvalid(); break;
        case 6: _t->resizeSideStakeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 7: _t->resizeSideStakeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 8: _t->resizeSideStakeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 9: _t->resizeSideStakeTableColumns((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 10: _t->resizeSideStakeTableColumns(); break;
        case 11: _t->enableApplyButton(); break;
        case 12: _t->disableApplyButton(); break;
        case 13: _t->enableSaveButtons(); break;
        case 14: _t->disableSaveButtons(); break;
        case 15: _t->setSaveButtonState((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 16: _t->on_okButton_clicked(); break;
        case 17: _t->on_cancelButton_clicked(); break;
        case 18: _t->on_applyButton_clicked(); break;
        case 19: _t->newSideStakeButton_clicked(); break;
        case 20: _t->editSideStakeButton_clicked(); break;
        case 21: _t->deleteSideStakeButton_clicked(); break;
        case 22: _t->showRestartWarning_Proxy(); break;
        case 23: _t->showRestartWarning_Lang(); break;
        case 24: _t->updateDisplayUnit(); break;
        case 25: _t->updateStyle(); break;
        case 26: _t->hideStartMinimized(); break;
        case 27: _t->hideLimitTxnDisplayDate(); break;
        case 28: _t->hideStakeSplitting(); break;
        case 29: _t->hidePollExpireNotify(); break;
        case 30: _t->hideSideStakeEdit(); break;
        case 31: _t->handleProxyIpValid((*reinterpret_cast< std::add_pointer_t<QValidatedLineEdit*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 32: _t->handleStakingEfficiencyValid((*reinterpret_cast< std::add_pointer_t<QValidatedLineEdit*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 33: _t->handleMinStakeSplitValueValid((*reinterpret_cast< std::add_pointer_t<QValidatedLineEdit*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 34: _t->handlePollExpireNotifyValid((*reinterpret_cast< std::add_pointer_t<QValidatedLineEdit*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 35: _t->handleSideStakeAllocationInvalid(); break;
        case 36: _t->handleSideStakeDescriptionInvalid(); break;
        case 37: _t->refreshSideStakeTableModel(); break;
        case 38: _t->tabWidgetSelectionChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 39: _t->sidestakeSelectionChanged(); break;
        case 40: _t->updateSideStakeTableView(); break;
        case 41: _t->sidestakeTableSectionResized((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (OptionsDialog::*)(QValidatedLineEdit * , bool )>(_a, &OptionsDialog::proxyIpValid, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (OptionsDialog::*)(QValidatedLineEdit * , bool )>(_a, &OptionsDialog::stakingEfficiencyValid, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (OptionsDialog::*)(QValidatedLineEdit * , bool )>(_a, &OptionsDialog::minStakeSplitValueValid, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (OptionsDialog::*)(QValidatedLineEdit * , bool )>(_a, &OptionsDialog::pollExpireNotifyValid, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (OptionsDialog::*)()>(_a, &OptionsDialog::sidestakeAllocationInvalid, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (OptionsDialog::*)()>(_a, &OptionsDialog::sidestakeDescriptionInvalid, 5))
            return;
    }
}

const QMetaObject *OptionsDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *OptionsDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13OptionsDialogE_t>.strings))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int OptionsDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 42)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 42;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 42)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 42;
    }
    return _id;
}

// SIGNAL 0
void OptionsDialog::proxyIpValid(QValidatedLineEdit * _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void OptionsDialog::stakingEfficiencyValid(QValidatedLineEdit * _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void OptionsDialog::minStakeSplitValueValid(QValidatedLineEdit * _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void OptionsDialog::pollExpireNotifyValid(QValidatedLineEdit * _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void OptionsDialog::sidestakeAllocationInvalid()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void OptionsDialog::sidestakeDescriptionInvalid()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}
QT_WARNING_POP
