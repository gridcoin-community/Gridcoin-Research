add_executable(test_gridcoin-qt
    test_main.cpp
    bitcoinunitstests.cpp
    uritests.cpp
    sidestakemodeltests.cpp
    coinselectionmodeltests.cpp
    coinselectionviewtests.cpp
)

set_target_properties(test_gridcoin-qt PROPERTIES
    AUTOMOC ON
)

target_link_libraries(test_gridcoin-qt PRIVATE
${RUNTIME_LIBS}
    ${QT}::Test
    ${QT}::Widgets
    ${QT}::Gui
    ${QT}::Core
    gridcoinqt
    gridcoin_util
    )

# A statically linked Qt only pulls in its default QPA plugin -- xcb on Linux,
# windows under mingw. Every CI runner exports QT_QPA_PLATFORM=offscreen because
# it has no display, so without this the binary aborts at QApplication
# construction with a "Could not find the Qt platform plugin" error.
# Importing the plugin is additive and a no-op against a shared Qt, where the
# plugin is already a loadable file on disk.
if(TARGET ${QT}::QOffscreenIntegrationPlugin)
    if(COMMAND qt_import_plugins)
        qt_import_plugins(test_gridcoin-qt INCLUDE ${QT}::QOffscreenIntegrationPlugin)
    elseif(COMMAND qt5_import_plugins)
        qt5_import_plugins(test_gridcoin-qt INCLUDE ${QT}::QOffscreenIntegrationPlugin)
    endif()
endif()

add_test(NAME gridcoin_qt_tests COMMAND test_gridcoin-qt)
