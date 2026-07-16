add_executable(test_gridcoin-qt
    test_main.cpp
    bitcoinunitstests.cpp
    uritests.cpp
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

add_test(NAME gridcoin_qt_tests COMMAND test_gridcoin-qt)
