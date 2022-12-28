# See the file LICENSE for redistribution information.
#
# Copyright (c) 2007, 2013 Oracle and/or its affiliates.  All rights reserved.
#
# $Id$
#
# TEST	repmgr030
# TEST	repmgr multiple client-to-client peer test.
# TEST
# TEST	Start an appointed master and three clients. The third client
# TEST	configures the other two clients as peers and delays client
# TEST	sync.  Add some data and confirm that the third client uses first
# TEST	client as a peer.  Close the master so that the first client now
# TEST	becomes the master.  Add some more data and confirm that the
# TEST	third client now uses the second client as a peer.
# TEST
# TEST	Run for btree only because access method shouldn't matter.
# TEST
proc repmgr030 { { niter 100 } { tnum "030" } args } {

	source ./include.tcl

	if { $is_freebsd_test == 1 } {
		puts "Skipping replication manager test on FreeBSD platform."
		return
	}

	set method "btree"
	set args [convert_args $method $args]

	puts "Repmgr$tnum ($method): repmgr multiple c2c peer test."
	repmgr030_sub $method $niter $tnum $args
}

proc repmgr030_sub { method niter tnum largs } {
	global testdir
	global rep_verbose
	global verbose_type
	set nsites 4

	set verbargs ""
	if { $rep_verbose == 1 } {
		set verbargs " -verbose {$verbose_type on} "
	}

	env_cleanup $testdir
	set ports [available_ports $nsites]
	set omethod [convert_method $method]

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR
	set clientdir2 $testdir/CLIENTDIR2
	set clientdir3 $testdir/CLIENTDIR3

	file mkdir $masterdir
	file mkdir $clientdir
	file mkdir $clientdir2
	file mkdir $clientdir3

	# Open a master.
	puts "\tRepmgr$tnum.a: Start a master."
	set ma_envcmd "berkdb_env_noerr -create $verbargs \
	    -errpfx MASTER -home $masterdir -txn -rep -thread"
	set masterenv [eval $ma_envcmd]
	$masterenv repmgr -ack all -pri 100 \
	    -local [list 127.0.0.1 [lindex $ports 0]] \
	    -start master

	# Open three clients, setting first two as peers of the third and
	# configuring third for delayed sync.
	puts "\tRepmgr$tnum.b: Start three clients, third with two peers."
	set cl_envcmd "berkdb_env_noerr -create $verbargs \
	    -errpfx CLIENT -home $clientdir -txn -rep -thread"
	set clientenv [eval $cl_envcmd]
	$clientenv repmgr -ack all -pri 80 \
	    -local [list 127.0.0.1 [lindex $ports 1]] \
	    -remote [list 127.0.0.1 [lindex $ports 0]] \
	    -remote [list 127.0.0.1 [lindex $ports 2]] \
	    -remote [list 127.0.0.1 [lindex $ports 3]] \
	    -start client
	await_startup_done $clientenv

	set cl2_envcmd "berkdb_env_noerr -create $verbargs \
	    -errpfx CLIENT2 -home $clientdir2 -txn -rep -thread"
	set clientenv2 [eval $cl2_envcmd]
	$clientenv2 repmgr -ack all -pri 50 \
	    -local [list 127.0.0.1 [lindex $ports 2]] \
	    -remote [list 127.0.0.1 [lindex $ports 0]] \
	    -remote [list 127.0.0.1 [lindex $ports 1]] \
	    -remote [list 127.0.0.1 [lindex $ports 3]] \
	    -start client
	await_startup_done $clientenv2

	set cl3_envcmd "berkdb_env_noerr -create $verbargs \
	    -errpfx CLIENT3 -home $clientdir3 -txn -rep -thread"
	set clientenv3 [eval $cl3_envcmd]
	$clientenv3 repmgr -ack all -pri 50 \
	    -local [list 127.0.0.1 [lindex $ports 3]] \
	    -remote [list 127.0.0.1 [lindex $ports 0]] \
	    -remote [list 127.0.0.1 [lindex $ports 1] peer] \
	    -remote [list 127.0.0.1 [lindex $ports 2] peer] \
	    -start client
	await_startup_done $clientenv3

	# Internally, repmgr does the following to determine the peer
	# to use: it scans the internal list of remote sites, selecting
	# the first one that is marked as a peer and that is not the
	# current master.

	puts "\tRepmgr$tnum.c: Configure third client for delayed sync."
	$clientenv3 rep_config {delayclient on}

	puts "\tRepmgr$tnum.d: Check third client used first client as peer."
	set creqs [stat_field $clientenv rep_stat "Client service requests"]
	set c2reqs [stat_field $clientenv2 rep_stat "Client service requests"]
	error_check_good got_client_reqs [expr {$creqs > 0}] 1
	error_check_good no_client2_reqs [expr {$c2reqs == 0}] 1

	puts "\tRepmgr$tnum.e: Run some transactions at master."
	eval rep_test $method $masterenv NULL $niter 0 0 0 $largs

	puts "\tRepmgr$tnum.f: Shut down master, first client takes over."
	error_check_good masterenv_close [$masterenv close] 0
	await_expected_master $clientenv

	puts "\tRepmgr$tnum.g: Run some more transactions at new master."
	eval rep_test $method $clientenv NULL $niter $niter 0 0 $largs

	puts "\tRepmgr$tnum.h: Sync delayed third client."
	error_check_good rep_sync [$clientenv3 rep_sync] 0
	# Give sync requests a bit of time to show up in stats.
	tclsleep 1

	puts "\tRepmgr$tnum.i: Check third client used second client as peer."
	set c2reqs [stat_field $clientenv2 rep_stat "Client service requests"]
	error_check_good got_client2_reqs [expr {$c2reqs > 0}] 1

	puts "\tRepmgr$tnum.j: Verifying client database contents."
	rep_verify $clientdir $clientenv $clientdir2 $clientenv2 1 1 1
	rep_verify $clientdir $clientenv $clientdir3 $clientenv3 1 1 1

	error_check_good client3_close [$clientenv3 close] 0
	error_check_good client2_close [$clientenv2 close] 0
	error_check_good client_close [$clientenv close] 0
}
