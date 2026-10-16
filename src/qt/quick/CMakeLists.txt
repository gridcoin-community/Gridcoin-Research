# Gridcoin.Qml: the C++ side of the QML front end. Form: a STATIC backing
# library created first, with AUTOMOC set before qt_add_qml_module (Qt 6.4
# reads it immediately), and no STATIC keyword on qt_add_qml_module (it
# rejects one for an existing target).
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

qt_add_library(gridcoinqml STATIC)
set_target_properties(gridcoinqml PROPERTIES AUTOMOC ON)
qt_add_qml_module(gridcoinqml
    URI Gridcoin.Qml
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    PLUGIN_TARGET gridcoinqmlplugin
    SOURCES
        qmlcallrunner.cpp
        qmlcallrunner.h
        qmlengine.cpp
        qmlengine.h
        qmlfrontend.cpp
        qmlfrontend.h
        qmlprovider.cpp
        qmlprovider.h
        shelladapter.cpp
        shelladapter.h
        splashadapter.cpp
        splashadapter.h
)
target_include_directories(gridcoinqml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gridcoinqml PUBLIC
    gridcoinqt
    ${QT}::Qml
    ${QT}::Quick
)

if(ENABLE_TESTS)
    add_subdirectory(test)
endif()
