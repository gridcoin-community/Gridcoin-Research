# See the file LICENSE for redistribution information.
#
# Copyright (c) 2010, 2013 Oracle and/or its affiliates.  All rights reserved.
#
# $Id$

source ./include.tcl
source $test_path/test.tcl
source $test_path/testutils.tcl
source $test_path/reputils.tcl

set dirC [lindex $argv 0]
set portC [lindex $argv 1]
set rv [lindex $argv 2]

proc in_sync_state { d } {
	global util_path
	set stat [exec $util_path/db_stat -N -r -R A -h $d]
	puts "stat is $stat"
	set in_page [is_substr $stat "SYNC_PAGE"]
	puts "value is $in_page"
	return $in_page
}

puts "Start site C"
set envC [berkdb env -create -errpfx C -home $dirC -txn -rep -thread \
	      -recover -verbose [list rep $rv]]
$envC repmgr -local [list 127.0.0.1 $portC] -start elect

puts "Wait until it gets into SYNC_PAGES state"
while {![in_sync_state $dirC]} {
	tclsleep 1
}
