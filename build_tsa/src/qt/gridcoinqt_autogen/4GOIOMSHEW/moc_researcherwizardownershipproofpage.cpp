/****************************************************************************
** Meta object code from reading C++ file 'researcherwizardownershipproofpage.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/qt/researcher/researcherwizardownershipproofpage.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'researcherwizardownershipproofpage.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN34ResearcherWizardOwnershipProofPageE_t {};
} // unnamed namespace

template <> constexpr inline auto ResearcherWizardOwnershipProofPage::qt_create_metaobjectdata<qt_meta_tag_ZN34ResearcherWizardOwnershipProofPageE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ResearcherWizardOwnershipProofPage",
        "copyPubKeyToClipboard",
        "",
        "submitOwnershipProof",
        "updateStatusIcon",
        "icon"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'copyPubKeyToClipboard'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'submitOwnershipProof'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateStatusIcon'
        QtMocHelpers::SlotData<void(const QIcon &)>(4, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QIcon, 5 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ResearcherWizardOwnershipProofPage, qt_meta_tag_ZN34ResearcherWizardOwnershipProofPageE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ResearcherWizardOwnershipProofPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWizardPage::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN34ResearcherWizardOwnershipProofPageE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN34ResearcherWizardOwnershipProofPageE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN34ResearcherWizardOwnershipProofPageE_t>.metaTypes,
    nullptr
} };

void ResearcherWizardOwnershipProofPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ResearcherWizardOwnershipProofPage *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->copyPubKeyToClipboard(); break;
        case 1: _t->submitOwnershipProof(); break;
        case 2: _t->updateStatusIcon((*reinterpret_cast< std::add_pointer_t<QIcon>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *ResearcherWizardOwnershipProofPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ResearcherWizardOwnershipProofPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN34ResearcherWizardOwnershipProofPageE_t>.strings))
        return static_cast<void*>(this);
    return QWizardPage::qt_metacast(_clname);
}

int ResearcherWizardOwnershipProofPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWizardPage::qt_metacall(_c, _id, _a);
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
QT_WARNING_POP
