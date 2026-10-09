add_subdirectory(fixture)

add_executable(test_gridcoin-qml
    test_main.cpp
    interfacefakes_qml.h
    qmltestutil.h
    bridgetests.cpp
    bridgetests.h
    callrunnertests.cpp
    callrunnertests.h
    moduletests.cpp
    moduletests.h
    quittests.cpp
    quittests.h
)

# Outside bin/: bin/ holds the modules' on-disk qmldir files, which would
# satisfy an import without the qrc:/qt/qml import path the module tests pin.
set_target_properties(test_gridcoin-qml PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qmltest"
)

target_link_libraries(test_gridcoin-qml PRIVATE
${RUNTIME_LIBS}
    gridcoinqml
    gridcoinqmlplugin
    gridcoinqmlfixtureplugin
    ${QT}::Test
    ${QT}::Quick
    ${QT}::Qml
    ${QT}::Widgets
    gridcoinqt
    gridcoin_util
)

# The offscreen platform plugin import: see the long comment in
# src/qt/test/CMakeLists.txt.
if(TARGET ${QT}::QOffscreenIntegrationPlugin)
    if(COMMAND qt_import_plugins)
        qt_import_plugins(test_gridcoin-qml INCLUDE ${QT}::QOffscreenIntegrationPlugin)
    elseif(COMMAND qt5_import_plugins)
        qt5_import_plugins(test_gridcoin-qml INCLUDE ${QT}::QOffscreenIntegrationPlugin)
    endif()
endif()

add_test(NAME gridcoin_qml_tests COMMAND test_gridcoin-qml WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/qmltest")
