/****************************************************************************
** Meta object code from reading C++ file 'tilesetview.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../render/tilesetview.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'tilesetview.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
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
struct qt_meta_tag_ZN11TilesetViewE_t {};
} // unnamed namespace

template <> constexpr inline auto TilesetView::qt_create_metaobjectdata<qt_meta_tag_ZN11TilesetViewE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TilesetView",
        "QML.Element",
        "auto",
        "hoverChanged",
        "",
        "edgeScrollOutsideChanged",
        "scrollSpeedChanged",
        "zoomEnabledChanged",
        "shadowsEnabledChanged",
        "mobileShadowsEnabledChanged",
        "gridEnabledChanged",
        "labelsEnabledChanged",
        "ambientStructureEnabledChanged",
        "ambientUnitEnabledChanged",
        "ambientResourceEnabledChanged",
        "overlayEnabledChanged",
        "edgeScrollEnabledChanged",
        "selectionChanged",
        "reportsChanged",
        "cameraChanged",
        "advanceAnimation",
        "gameTicks",
        "selectAt",
        "mouseX",
        "mouseY",
        "clearSelection",
        "setSelectedIdle",
        "on",
        "focusUnit",
        "index",
        "generation",
        "exportViewport",
        "exportFullMap",
        "zoomAt",
        "deltaY",
        "x",
        "y",
        "toggleBuildingAt",
        "toggleDisabled",
        "surveyMineAt",
        "tileX",
        "tileY",
        "surveyAllMines",
        "player",
        "loadMap",
        "mapRel",
        "hoverTileX",
        "hoverTileY",
        "hoverInfo",
        "hoverObjActive",
        "hoverObjName",
        "hoverCursorX",
        "hoverCursorY",
        "selectedInfo",
        "QVariantMap",
        "selectedValid",
        "factoriesReport",
        "QVariantList",
        "labsReport",
        "resourcesReport",
        "colonistsReport",
        "moraleReport",
        "moraleStructures",
        "colonistStructures",
        "commsMessages",
        "spaceReport",
        "starshipReport",
        "edgeScrollOutside",
        "scrollSpeed",
        "zoomEnabled",
        "shadowsEnabled",
        "mobileShadowsEnabled",
        "gridEnabled",
        "labelsEnabled",
        "ambientStructureEnabled",
        "ambientUnitEnabled",
        "ambientResourceEnabled",
        "overlayEnabled",
        "edgeScrollEnabled"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'hoverChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'edgeScrollOutsideChanged'
        QtMocHelpers::SignalData<void()>(5, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'scrollSpeedChanged'
        QtMocHelpers::SignalData<void()>(6, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'zoomEnabledChanged'
        QtMocHelpers::SignalData<void()>(7, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'shadowsEnabledChanged'
        QtMocHelpers::SignalData<void()>(8, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'mobileShadowsEnabledChanged'
        QtMocHelpers::SignalData<void()>(9, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'gridEnabledChanged'
        QtMocHelpers::SignalData<void()>(10, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'labelsEnabledChanged'
        QtMocHelpers::SignalData<void()>(11, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'ambientStructureEnabledChanged'
        QtMocHelpers::SignalData<void()>(12, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'ambientUnitEnabledChanged'
        QtMocHelpers::SignalData<void()>(13, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'ambientResourceEnabledChanged'
        QtMocHelpers::SignalData<void()>(14, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'overlayEnabledChanged'
        QtMocHelpers::SignalData<void()>(15, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'edgeScrollEnabledChanged'
        QtMocHelpers::SignalData<void()>(16, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'selectionChanged'
        QtMocHelpers::SignalData<void()>(17, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'reportsChanged'
        QtMocHelpers::SignalData<void()>(18, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'cameraChanged'
        QtMocHelpers::SignalData<void()>(19, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'advanceAnimation'
        QtMocHelpers::MethodData<void(int)>(20, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 21 },
        }}),
        // Method 'advanceAnimation'
        QtMocHelpers::MethodData<void()>(20, 4, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Method 'selectAt'
        QtMocHelpers::MethodData<void(qreal, qreal)>(22, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QReal, 23 }, { QMetaType::QReal, 24 },
        }}),
        // Method 'clearSelection'
        QtMocHelpers::MethodData<void()>(25, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'setSelectedIdle'
        QtMocHelpers::MethodData<void(bool)>(26, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 27 },
        }}),
        // Method 'focusUnit'
        QtMocHelpers::MethodData<void(quint32, quint32)>(28, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 29 }, { QMetaType::UInt, 30 },
        }}),
        // Method 'exportViewport'
        QtMocHelpers::MethodData<QString()>(31, 4, QMC::AccessPublic, QMetaType::QString),
        // Method 'exportFullMap'
        QtMocHelpers::MethodData<QString()>(32, 4, QMC::AccessPublic, QMetaType::QString),
        // Method 'zoomAt'
        QtMocHelpers::MethodData<void(qreal, qreal, qreal)>(33, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QReal, 34 }, { QMetaType::QReal, 35 }, { QMetaType::QReal, 36 },
        }}),
        // Method 'toggleBuildingAt'
        QtMocHelpers::MethodData<void(qreal, qreal, bool)>(37, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QReal, 23 }, { QMetaType::QReal, 24 }, { QMetaType::Bool, 38 },
        }}),
        // Method 'surveyMineAt'
        QtMocHelpers::MethodData<bool(int, int)>(39, 4, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 40 }, { QMetaType::Int, 41 },
        }}),
        // Method 'surveyAllMines'
        QtMocHelpers::MethodData<int(int)>(42, 4, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Int, 43 },
        }}),
        // Method 'surveyAllMines'
        QtMocHelpers::MethodData<int()>(42, 4, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Int),
        // Method 'loadMap'
        QtMocHelpers::MethodData<void(const QString &)>(44, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 45 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'hoverTileX'
        QtMocHelpers::PropertyData<int>(46, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'hoverTileY'
        QtMocHelpers::PropertyData<int>(47, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'hoverInfo'
        QtMocHelpers::PropertyData<QString>(48, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'hoverObjActive'
        QtMocHelpers::PropertyData<bool>(49, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'hoverObjName'
        QtMocHelpers::PropertyData<QString>(50, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'hoverCursorX'
        QtMocHelpers::PropertyData<int>(51, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'hoverCursorY'
        QtMocHelpers::PropertyData<int>(52, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'selectedInfo'
        QtMocHelpers::PropertyData<QVariantMap>(53, 0x80000000 | 54, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 13),
        // property 'selectedValid'
        QtMocHelpers::PropertyData<bool>(55, QMetaType::Bool, QMC::DefaultPropertyFlags, 13),
        // property 'factoriesReport'
        QtMocHelpers::PropertyData<QVariantList>(56, 0x80000000 | 57, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'labsReport'
        QtMocHelpers::PropertyData<QVariantList>(58, 0x80000000 | 57, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'resourcesReport'
        QtMocHelpers::PropertyData<QVariantMap>(59, 0x80000000 | 54, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'colonistsReport'
        QtMocHelpers::PropertyData<QVariantMap>(60, 0x80000000 | 54, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'moraleReport'
        QtMocHelpers::PropertyData<QVariantMap>(61, 0x80000000 | 54, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'moraleStructures'
        QtMocHelpers::PropertyData<QVariantList>(62, 0x80000000 | 57, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'colonistStructures'
        QtMocHelpers::PropertyData<QVariantList>(63, 0x80000000 | 57, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'commsMessages'
        QtMocHelpers::PropertyData<QVariantList>(64, 0x80000000 | 57, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'spaceReport'
        QtMocHelpers::PropertyData<QVariantList>(65, 0x80000000 | 57, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'starshipReport'
        QtMocHelpers::PropertyData<QVariantList>(66, 0x80000000 | 57, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'edgeScrollOutside'
        QtMocHelpers::PropertyData<bool>(67, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'scrollSpeed'
        QtMocHelpers::PropertyData<int>(68, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 2),
        // property 'zoomEnabled'
        QtMocHelpers::PropertyData<bool>(69, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 3),
        // property 'shadowsEnabled'
        QtMocHelpers::PropertyData<bool>(70, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 4),
        // property 'mobileShadowsEnabled'
        QtMocHelpers::PropertyData<bool>(71, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 5),
        // property 'gridEnabled'
        QtMocHelpers::PropertyData<bool>(72, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 6),
        // property 'labelsEnabled'
        QtMocHelpers::PropertyData<bool>(73, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 7),
        // property 'ambientStructureEnabled'
        QtMocHelpers::PropertyData<bool>(74, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 8),
        // property 'ambientUnitEnabled'
        QtMocHelpers::PropertyData<bool>(75, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 9),
        // property 'ambientResourceEnabled'
        QtMocHelpers::PropertyData<bool>(76, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 10),
        // property 'overlayEnabled'
        QtMocHelpers::PropertyData<bool>(77, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 11),
        // property 'edgeScrollEnabled'
        QtMocHelpers::PropertyData<bool>(78, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 12),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<TilesetView, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject TilesetView::staticMetaObject = { {
    QMetaObject::SuperData::link<QQuickPaintedItem::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11TilesetViewE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11TilesetViewE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11TilesetViewE_t>.metaTypes,
    nullptr
} };

void TilesetView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TilesetView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->hoverChanged(); break;
        case 1: _t->edgeScrollOutsideChanged(); break;
        case 2: _t->scrollSpeedChanged(); break;
        case 3: _t->zoomEnabledChanged(); break;
        case 4: _t->shadowsEnabledChanged(); break;
        case 5: _t->mobileShadowsEnabledChanged(); break;
        case 6: _t->gridEnabledChanged(); break;
        case 7: _t->labelsEnabledChanged(); break;
        case 8: _t->ambientStructureEnabledChanged(); break;
        case 9: _t->ambientUnitEnabledChanged(); break;
        case 10: _t->ambientResourceEnabledChanged(); break;
        case 11: _t->overlayEnabledChanged(); break;
        case 12: _t->edgeScrollEnabledChanged(); break;
        case 13: _t->selectionChanged(); break;
        case 14: _t->reportsChanged(); break;
        case 15: _t->cameraChanged(); break;
        case 16: _t->advanceAnimation((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 17: _t->advanceAnimation(); break;
        case 18: _t->selectAt((*reinterpret_cast<std::add_pointer_t<qreal>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qreal>>(_a[2]))); break;
        case 19: _t->clearSelection(); break;
        case 20: _t->setSelectedIdle((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 21: _t->focusUnit((*reinterpret_cast<std::add_pointer_t<quint32>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint32>>(_a[2]))); break;
        case 22: { QString _r = _t->exportViewport();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 23: { QString _r = _t->exportFullMap();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 24: _t->zoomAt((*reinterpret_cast<std::add_pointer_t<qreal>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qreal>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<qreal>>(_a[3]))); break;
        case 25: _t->toggleBuildingAt((*reinterpret_cast<std::add_pointer_t<qreal>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qreal>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 26: { bool _r = _t->surveyMineAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 27: { int _r = _t->surveyAllMines((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 28: { int _r = _t->surveyAllMines();
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 29: _t->loadMap((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::hoverChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::edgeScrollOutsideChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::scrollSpeedChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::zoomEnabledChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::shadowsEnabledChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::mobileShadowsEnabledChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::gridEnabledChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::labelsEnabledChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::ambientStructureEnabledChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::ambientUnitEnabledChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::ambientResourceEnabledChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::overlayEnabledChanged, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::edgeScrollEnabledChanged, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::selectionChanged, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::reportsChanged, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (TilesetView::*)()>(_a, &TilesetView::cameraChanged, 15))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<int*>(_v) = _t->hoverTileX(); break;
        case 1: *reinterpret_cast<int*>(_v) = _t->hoverTileY(); break;
        case 2: *reinterpret_cast<QString*>(_v) = _t->hoverInfo(); break;
        case 3: *reinterpret_cast<bool*>(_v) = _t->hoverObjActive(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->hoverObjName(); break;
        case 5: *reinterpret_cast<int*>(_v) = _t->hoverCursorX(); break;
        case 6: *reinterpret_cast<int*>(_v) = _t->hoverCursorY(); break;
        case 7: *reinterpret_cast<QVariantMap*>(_v) = _t->selectedInfo(); break;
        case 8: *reinterpret_cast<bool*>(_v) = _t->selectedValid(); break;
        case 9: *reinterpret_cast<QVariantList*>(_v) = _t->factoriesReport(); break;
        case 10: *reinterpret_cast<QVariantList*>(_v) = _t->labsReport(); break;
        case 11: *reinterpret_cast<QVariantMap*>(_v) = _t->resourcesReport(); break;
        case 12: *reinterpret_cast<QVariantMap*>(_v) = _t->colonistsReport(); break;
        case 13: *reinterpret_cast<QVariantMap*>(_v) = _t->moraleReport(); break;
        case 14: *reinterpret_cast<QVariantList*>(_v) = _t->moraleStructures(); break;
        case 15: *reinterpret_cast<QVariantList*>(_v) = _t->colonistStructures(); break;
        case 16: *reinterpret_cast<QVariantList*>(_v) = _t->commsMessages(); break;
        case 17: *reinterpret_cast<QVariantList*>(_v) = _t->spaceReport(); break;
        case 18: *reinterpret_cast<QVariantList*>(_v) = _t->starshipReport(); break;
        case 19: *reinterpret_cast<bool*>(_v) = _t->edgeScrollOutside(); break;
        case 20: *reinterpret_cast<int*>(_v) = _t->scrollSpeed(); break;
        case 21: *reinterpret_cast<bool*>(_v) = _t->zoomEnabled(); break;
        case 22: *reinterpret_cast<bool*>(_v) = _t->shadowsEnabled(); break;
        case 23: *reinterpret_cast<bool*>(_v) = _t->mobileShadowsEnabled(); break;
        case 24: *reinterpret_cast<bool*>(_v) = _t->gridEnabled(); break;
        case 25: *reinterpret_cast<bool*>(_v) = _t->labelsEnabled(); break;
        case 26: *reinterpret_cast<bool*>(_v) = _t->ambientStructureEnabled(); break;
        case 27: *reinterpret_cast<bool*>(_v) = _t->ambientUnitEnabled(); break;
        case 28: *reinterpret_cast<bool*>(_v) = _t->ambientResourceEnabled(); break;
        case 29: *reinterpret_cast<bool*>(_v) = _t->overlayEnabled(); break;
        case 30: *reinterpret_cast<bool*>(_v) = _t->edgeScrollEnabled(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 19: _t->setEdgeScrollOutside(*reinterpret_cast<bool*>(_v)); break;
        case 20: _t->setScrollSpeed(*reinterpret_cast<int*>(_v)); break;
        case 21: _t->setZoomEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 22: _t->setShadowsEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 23: _t->setMobileShadowsEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 24: _t->setGridEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 25: _t->setLabelsEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 26: _t->setAmbientStructureEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 27: _t->setAmbientUnitEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 28: _t->setAmbientResourceEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 29: _t->setOverlayEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 30: _t->setEdgeScrollEnabled(*reinterpret_cast<bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *TilesetView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TilesetView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11TilesetViewE_t>.strings))
        return static_cast<void*>(this);
    return QQuickPaintedItem::qt_metacast(_clname);
}

int TilesetView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QQuickPaintedItem::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 30)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 30;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 30)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 30;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 31;
    }
    return _id;
}

// SIGNAL 0
void TilesetView::hoverChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void TilesetView::edgeScrollOutsideChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void TilesetView::scrollSpeedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void TilesetView::zoomEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void TilesetView::shadowsEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void TilesetView::mobileShadowsEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void TilesetView::gridEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void TilesetView::labelsEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void TilesetView::ambientStructureEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void TilesetView::ambientUnitEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void TilesetView::ambientResourceEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void TilesetView::overlayEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void TilesetView::edgeScrollEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void TilesetView::selectionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 13, nullptr);
}

// SIGNAL 14
void TilesetView::reportsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void TilesetView::cameraChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}
QT_WARNING_POP
