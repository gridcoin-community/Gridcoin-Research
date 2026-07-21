# CMake generated Testfile for 
# Source directory: /home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/src/test
# Build directory: /home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_win64_test/src/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(gridcoin_tests "/usr/bin/wine" "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_win64_test/src/test/test_gridcoin.exe")
set_tests_properties(gridcoin_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/src/test/CMakeLists.txt;144;add_test;/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/src/test/CMakeLists.txt;0;")
subdirs("data")
