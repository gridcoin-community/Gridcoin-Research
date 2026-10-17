# Test-only QML module: a QML-file singleton (FixtureTheme) and a root item,
# so the module tests can exercise the qrc:/qt/qml import path.
set_source_files_properties(FixtureTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_library(gridcoinqmlfixture STATIC)
set_target_properties(gridcoinqmlfixture PROPERTIES AUTOMOC ON)
qt_add_qml_module(gridcoinqmlfixture
    URI FixtureTheme
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    PLUGIN_TARGET gridcoinqmlfixtureplugin
    QML_FILES
        FixtureTheme.qml
        FixtureRoot.qml
)
