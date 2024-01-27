add_executable(test_gridcoin-qt
    test_main.cpp
    uritests.cpp
)

set_target_properties(test_gridcoin-qt PROPERTIES
    AUTOMOC ON
)

target_link_libraries(test_gridcoin-qt PRIVATE
    ${RUNTIME_LIBS}
    Qt5::Test
    Qt5::Widgets
    gridcoin_util
    gridcoinqt
)

add_test(NAME gridcoin_qt_tests COMMAND test_gridcoin-qt)
