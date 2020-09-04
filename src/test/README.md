The sources in this directory are unit test cases. The unit tests uses Boost's unit testing framework. 
Since Gridcoin already uses Boost, Gridcoin uses the framework instead of requiring developers
to configure another framework (reduces barriers to creating unit tests).

The build system is setup to compile an executable called "test_gridcoin"
that runs all of the unit tests. The main source file is called
test_gridcoin.cpp, which simply includes other files that contain the
actual unit tests (outside of a couple required preprocessor
directives). The pattern is to create one test file for each class or
source file for which you want to create unit tests. The file naming
convention is "<source_filename>_tests.cpp" and such files should wrap
their tests in a test suite called "<source_filename>_tests". For an
examples of this pattern, examine uint160_tests.cpp and uint256_tests.cpp.

The tests in transaction_tests.cpp are edge cases of Gridcoin transactions.
They are in their current state not relevant for Gridcoin. Unusual transactions
should be collected again from the gridcoin blockchain and replace
the current test cases.

For further reading, [see Boost's documentation](https://www.boost.org/doc/libs/1_73_0/libs/test/doc/html/boost_test/intro.html)
about how the boost unit test framework works
