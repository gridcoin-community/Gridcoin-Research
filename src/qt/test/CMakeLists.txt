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
# windows under mingw. test_main.cpp defaults QT_QPA_PLATFORM to offscreen when
# the environment does not set it, so this binary selects offscreen ITSELF and
# needs the plugin whether or not a workflow exported the variable -- including
# for a developer running ctest locally on a machine that does have a display.
# (Three of the eight bare ctest invocations across the workflows set nothing.)
# Without the plugin the binary aborts at QApplication construction with a
# "Could not find the Qt platform plugin" error. Importing it is additive and a
# no-op against a shared Qt, where the plugin is already a file on disk.
#
# Done here rather than by extending the Q_IMPORT_PLUGIN block in test_main.cpp
# because that block cannot fire in this build: its QT_QPA_PLATFORM_XCB /
# _WINDOWS / _COCOA / _MINIMAL guards are Bitcoin Core autoconf defines that
# Gridcoin's CMake port never carried over. `git grep QT_QPA_PLATFORM_` finds
# them only in qt/bitcoin.cpp and qt/test/test_main.cpp, both consumers, with no
# definition anywhere in the tree -- so every one of those imports is behind a
# macro that is never defined. Extending it would have added a second dead
# branch. qt_import_plugins works because CMake knows whether the plugin target
# exists, which is what the guard below asks it.
if(TARGET ${QT}::QOffscreenIntegrationPlugin)
    if(COMMAND qt_import_plugins)
        qt_import_plugins(test_gridcoin-qt INCLUDE ${QT}::QOffscreenIntegrationPlugin)
    elseif(COMMAND qt5_import_plugins)
        qt5_import_plugins(test_gridcoin-qt INCLUDE ${QT}::QOffscreenIntegrationPlugin)
    endif()
endif()

add_test(NAME gridcoin_qt_tests COMMAND test_gridcoin-qt)
