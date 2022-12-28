# See the file LICENSE for redistribution information.
#
# Copyright (c) 2004, 2013 Oracle and/or its affiliates.  All rights reserved.
#
# $Id$
#
# TEST	rep052
# TEST	Test of replication with NOWAIT.
# TEST
# TEST	One master, one client.  After initializing
# TEST	everything normally, close client and let the
# TEST	master get ahead -- far enough that the master
# TEST 	no longer has the client's last log file.
# TEST	Reopen the client and turn on NOWAIT.
# TEST	Process a few messages to get the client into
# TEST	recovery mode, and verify that lockout occurs
# TEST 	on a txn API call (txn_begin) and an env API call.
# TEST	Process all the messages and verify that lockout
# TEST 	is over.

proc rep052 { method { niter 200 } { tnum "052" } args } {

	source ./include.tcl
	global databases_in_memory
	global repfiles_in_memory
	global env_private

	# Valid for all access methods.
	if { $checking_valid_methods } {
		return "ALL"
	}

	set args [convert_args $method $args]
	set saved_args $args

	# This test needs to set its own pagesize.
	set pgindex [lsearch -exact $args "-pagesize"]
	if { $pgindex != -1 } {
		puts "Rep$tnum: skipping for specific pagesizes"
		return
	}

	set logsets [create_logsets 2]
	set saved_args $args

	# Set up for on-disk or in-memory databases.
	set msg "using on-disk databases"
	if { $databases_in_memory } {
		set msg "using named in-memory databases"
		if { [is_queueext $method] } { 
			puts -nonewline "Skipping rep$tnum for method "
			puts "$method with named in-memory databases."
			return
		}
	}

	set msg2 "and on-disk replication files"
	if { $repfiles_in_memory } {
		set msg2 "and in-memory replication files"
	}

	set msg3 ""
	if { $env_private } {
		set msg3 "with private env"
	}

	# Run the body of the test with and without recovery.  Skip
	# recovery with in-memory logging - it doesn't make sense.
	foreach r $test_recopts {
		foreach l $logsets {
			set logindex [lsearch -exact $l "in-memory"]
			if { $r == "-recover" && $logindex != -1 } {
				puts "Skipping rep$tnum for -recover\
				    with in-memory logs."
				continue
			}
			set envargs ""
			set args $saved_args
			puts "Rep$tnum ($method $envargs $r $args):\
			    Test lockouts with REP_NOWAIT\
			    $msg $msg2 $msg3."
			puts "Rep$tnum: Master logs are [lindex $l 0]"
			puts "Rep$tnum: Client logs are [lindex $l 1]"
			rep052_sub $method $niter $tnum $envargs \
			    $l $r $args
		}
	}
}

proc rep052_sub { method niter tnum envargs logset recargs largs } {
	global testdir
	global util_path
	global databases_in_memory
	global repfiles_in_memory
	global env_private
	global rep_verbose
	global verbose_type

	set verbargs ""
	if { $rep_verbose == 1 } {
		set verbargs " -verbose {$verbose_type on} "
	}

	set repmemargs ""
	if { $repfiles_in_memory } {
		set repmemargs "-rep_inmem_files "
	}

	set privargs ""
	if { $env_private == 1 } {
		set privargs " -private "
	}

	env_cleanup $testdir

	replsetup $testdir/MSGQUEUEDIR

	set masterdir $testdir/MASTERDIR
	set clientdir $testdir/CLIENTDIR

	file mkdir $masterdir
	file mkdir $clientdir

	# Log size is small so we quickly create more than one.
	# The documentation says that the log file must be at least
	# four times the size of the in-memory log buffer.
	set pagesize 4096
	append largs " -pagesize $pagesize "
	set log_max [expr $pagesize * 8]

	set m_logtype [lindex $logset 0]
	set c_logtype [lindex $logset 1]

	# In-memory logs cannot be used with -txn nosync.
	set m_txnargs [adjust_txnargs $m_logtype]
	set c_txnargs [adjust_txnargs $c_logtype]
	set m_logargs [adjust_logargs $m_logtype]
	set c_logargs [adjust_logargs $c_logtype]

	# Open a master.
	repladd 1
	set ma_envcmd "berkdb_env_noerr -create $m_txnargs $verbargs \
	    $repmemargs $privargs \
	    $m_logargs -log_max $log_max $envargs -errpfx MASTER \
	    -home $masterdir -rep_transport \[list 1 replsend\]"
	set masterenv [eval $ma_envcmd $recargs -rep_master]
	$masterenv rep_limit 0 0

	# Open a client
	repladd 2
	set cl_envcmd "berkdb_env_noerr -create $c_txnargs $verbargs \
	    $repmemargs $privargs \
	    $c_logargs -log_max $log_max $envargs -errpfx CLIENT \
	    -home $clientdir -rep_transport \[list 2 replsend\]"
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	$clientenv rep_limit 0 0

	# Bring the clients online by processing the startup messages.
	set envlist "{$masterenv 1} {$clientenv 2}"
	process_msgs $envlist

	# Clobber replication's 30-second anti-archive timer, which will have
	# been started by client sync-up internal init, so that we can do a
	# log_archive in a moment.
	#
	$masterenv test force noarchive_timeout

	# Run rep_test in the master (and update client).
	puts "\tRep$tnum.a: Running rep_test in replicated env."
	set start 0
	eval rep_test $method $masterenv NULL $niter $start $start 0 $largs
	incr start $niter
	process_msgs $envlist

	# Find out what exists on the client before closing.  We'll need
	# to loop until the first master log file > last client log file.
	set last_client_log [get_logfile $clientenv last]

	# Create a gap requiring internal initialization.
	set flags ""
	set cid 2
	set dbhandle NULL
	set start [push_master_ahead $method $masterenv $masterdir $m_logtype \
	    $clientenv $cid $dbhandle $start $niter $flags $largs]

	puts "\tRep$tnum.e: Reopen client."
	env_cleanup $clientdir
	set clientenv [eval $cl_envcmd $recargs -rep_client]
	error_check_good client_env [is_valid_env $clientenv] TRUE
	$clientenv rep_limit 0 0
	set envlist "{$masterenv 1} {$clientenv 2}"

	# Turn on nowait.
	$clientenv rep_config {nowait on}

	# Process messages a few times, just enough to get client
	# into lockout/recovery mode, but not enough to complete recovery.
	set iter 3
	for { set i 0 } { $i < $iter } { incr i } {
		set nproced [proc_msgs_once $envlist NONE err]
	}

	puts "\tRep$tnum.f: Verify we are locked out of txn API calls."
	if { [catch { set txn [$clientenv txn] } res] } {
		error_check_good txn_lockout [is_substr $res "DB_REP_LOCKOUT"] 1
	} else {
		error "FAIL:[timestamp] Not locked out of txn API calls."
	}

	puts "\tRep$tnum.g: Verify we are locked out of env API calls."
	if { [catch { set stat [$clientenv lock_stat] } res] } {
		error_check_good env_lockout [is_substr $res "DB_REP_LOCKOUT"] 1
	} else {
		error "FAIL:[timestamp] Not locked out of env API calls."
	}

	# Now catch up and make sure we're not locked out anymore.
	process_msgs $envlist

	puts "\tRep$tnum.h: No longer locked out of txn API calls."
	if { [catch { set txn [$clientenv txn] } res] } {
		puts "FAIL: unable to start txn: $res"
	} else {
		error_check_good txn_no_lockout [$txn commit] 0
	}

	puts "\tRep$tnum.i: No longer locked out of env API calls."
	if { [catch { set stat [$clientenv rep_stat] } res] } {
		puts "FAIL: unable to make env call: $res"
	}

	puts "\tRep$tnum.h: Verify logs and databases"
	rep_verify $masterdir $masterenv $clientdir $clientenv 1 1 1

	error_check_good masterenv_close [$masterenv close] 0
	error_check_good clientenv_close [$clientenv close] 0
	replclose $testdir/MSGQUEUEDIR
}
