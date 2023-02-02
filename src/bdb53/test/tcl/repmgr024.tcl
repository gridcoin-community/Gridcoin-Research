# See the file LICENSE for redistribution information.
#
# Copyright (c) 2009, 2013 Oracle and/or its affiliates.  All rights reserved.
#
# TEST	repmgr024
# TEST	Test of group-wide log archiving awareness.
# TEST 	Verify that log archiving will use the ack from the clients in
# TEST	its decisions about what log files are allowed to be archived.
#
proc repmgr024 { { niter 50 } { tnum 024 } args } {
	source ./include.tcl

	if { $is_freebsd_test == 1 } {
		puts "Skipping replication manager test on FreeBSD platform."
		return
	}

	# QNX does not support fork() in a multi-threaded environment.
	if { $is_qnx_test } {
		puts "Skipping repmgr$tnum on QNX."
		return
	}

	set method "btree"
	set args [convert_args $method $args]
	puts "Repmgr$tnum ($method): group wide log archiving."
	repmgr024_sub $method $niter $tnum $args
}

proc repmgr024_sub { method niter tnum largs } {
	global testdir
	global util_path
	global databases_in_memory
	global repfiles_in_memory
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

	env_cleanup $testdir
	file mkdir [set dira $testdir/SITE_A]
	file mkdir [set dirb $testdir/SITE_B]
	file mkdir [set dirc $testdir/SITE_C]
	foreach { porta portb portc } [available_ports 3] {}

	# Log size is small so we quickly create more than one.
	# The documentation says that the log file must be at least
	# four times the size of the in-memory log buffer.
	set pagesize 4096
	append largs " -pagesize $pagesize "
	set log_buf [expr $pagesize * 2]
	set log_max [expr $log_buf * 4]

	set cmda "berkdb_env_noerr -create -txn nosync \
	    $verbargs $repmemargs -rep -thread \
	    -log_buffer $log_buf -log_max $log_max -errpfx SITE_A \
	    -home $dira"
	set enva [eval $cmda]
	# Use quorum ack policy (default, therefore not specified)
	# otherwise it will never wait when
	# the client is closed and we want to give it a chance to
	# wait later in the test.
	$enva repmgr -timeout {connection_retry 5000000} \
	    -local [list 127.0.0.1 $porta] -start master

	set cmdb "berkdb_env_noerr -create -txn nosync \
	    $verbargs $repmemargs -rep -thread \
	    -log_buffer $log_buf -log_max $log_max -errpfx SITE_B \
	    -home $dirb"
	set envb [eval $cmdb]
	$envb repmgr -timeout {connection_retry 5000000} \
	    -local [list 127.0.0.1 $portb] -start client \
	    -remote [list 127.0.0.1 $porta]
	puts "\tRepmgr$tnum.a: wait for client B to sync with master."
	await_startup_done $envb

	set cmdc "berkdb_env_noerr -create -txn nosync \
	    $verbargs $repmemargs -rep -thread \
	    -log_buffer $log_buf -log_max $log_max -errpfx SITE_C \
	    -home $dirc"
	set envc [eval $cmdc]
	$envc repmgr -timeout {connection_retry 5000000} \
	    -local [list 127.0.0.1 $portc] -start client \
	    -remote [list 127.0.0.1 $porta]
	puts "\tRepmgr$tnum.b: wait for client C to sync with master."
	await_startup_done $envc


	# Clobber replication's 30-second anti-archive timer, which will have
	# been started by client sync-up internal init, so that we can do a
	# log_archive in a moment.
	#
	$enva test force noarchive_timeout

	set stop 0
	set start 0
	while { $stop == 0 } {
		# Run rep_test in the master.
		puts "\tRepmgr$tnum.c: Running rep_test in replicated env."
		eval rep_test $method $enva NULL $niter $start 0 0 $largs
		incr start $niter

		set res [eval exec $util_path/db_archive -h $dira]
		if { [llength $res] != 0 } {
			set stop 1
		}
	}
	# Save list of files for later.
	set files_arch $res

	puts "\tRepmgr$tnum.d: Close client."
	$envc close

	# Now that the client closed its connection, verify that
	# we cannot archive files.
	#
	# When a connection is closed, repmgr updates the 30 second
	# noarchive timestamp in order to give the client process a
	# chance to restart and rejoin the group.  We verify that
	# when the connection is closed the master cannot archive.
	# due to the 30-second timer.
	#
	set res [eval exec $util_path/db_archive -h $dira]
	error_check_good files_archivable_closed [llength $res] 0

	#
	# Clobber the 30-second timer and verify we can again archive the
	# files.
	#
	$enva test force noarchive_timeout
	set res [eval exec $util_path/db_archive -h $dira]
	error_check_good files_arch2 $files_arch $res

	set res [eval exec $util_path/db_archive -l -h $dirc]
	set last_client_log [lindex [lsort $res] end]

	set stop 0
	while { $stop == 0 } {
		# Run rep_test in the master.
		puts "\tRepmgr$tnum.e: Running rep_test in replicated env."
		eval rep_test $method $enva NULL $niter $start 0 0 $largs
		incr start $niter

		# We use log_archive when we want to remove log files so
		# that if we are running verbose, we get all of the output
		# we might need.
		#
		# However, we can use db_archive for all of the other uses
		# we need such as getting a list of what log files exist in
		# the environment.
		#
		puts "\tRepmgr$tnum.f: Run log_archive on master."
		set res [$enva log_archive -arch_remove]
		set res [eval exec $util_path/db_archive -l -h $dira]
		if { [lsearch -exact $res $last_client_log] == -1 } {
			set stop 1
		}
	}

	#
	# Get the new last log file for client 1.
	#
	set res [eval exec $util_path/db_archive -l -h $dirb]
	set last_client_log [lindex [lsort $res] end]

	#
	# Set test hook to prevent client 1 from sending any ACKs,
	# but remaining alive.
	#
	puts "\tRepmgr$tnum.g: Turn off acks via test hook on remaining client."
	$envb test abort repmgr_perm

	#
	# Advance logfiles again.
	set stop 0
	while { $stop == 0 } {
		# Run rep_test in the master.
		puts "\tRepmgr$tnum.h: Running rep_test in replicated env."
		eval rep_test $method $enva NULL $niter $start 0 0 $largs
		incr start $niter

		puts "\tRepmgr$tnum.i: Run db_archive on master."
		set res [eval exec $util_path/db_archive -l -h $dira]
		set last_master_log [lindex [lsort $res] end]
		if { $last_master_log != $last_client_log } {
			set stop 1
		}
	}

	#
	# Make sure neither log_archive in same process nor db_archive
	# in a different process show any files to archive.
	#
	error_check_good no_files_log_archive [llength [$enva log_archive]] 0
	set dbarchres [eval exec $util_path/db_archive -h $dira]
	error_check_good no_files_db_archive [llength $dbarchres] 0

	puts "\tRepmgr$tnum.j: Try to archive. Verify it didn't."
	set res [$enva log_archive -arch_remove]
	set res [eval exec $util_path/db_archive -l -h $dira]
	error_check_bad cl1_archive [lsearch -exact $res $last_client_log] -1
	#
	# Turn off test hook preventing acks.  Then run a perm operation
	# so that the client can send its ack.
	#
	puts "\tRepmgr$tnum.k: Enable acks and archive again."
	$envb test abort none
	$enva txn_checkpoint -force
	#
	# Now archive again and make sure files were removed.
	#
	set res [$enva log_archive -arch_remove]
	set res [eval exec $util_path/db_archive -l -h $dira]
	error_check_good cl1_archive [lsearch -exact $res $last_client_log] -1

	$enva close
	$envb close
}
