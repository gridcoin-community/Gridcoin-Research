# See the file LICENSE for redistribution information.
#
# Copyright (c) 2009, 2013 Oracle and/or its affiliates.  All rights reserved.
#
# TEST repmgr108
# TEST Subordinate connections and processes should not trigger elections.

proc repmgr108 { } {
	source ./include.tcl

	set tnum "108"
	puts "Repmgr$tnum: Subordinate\
	    connections and processes should not trigger elections."

	env_cleanup $testdir

	foreach {mport cport} [available_ports 2] {}
	file mkdir [set mdir $testdir/MASTER]
	file mkdir [set cdir $testdir/CLIENT]

	make_dbconfig $mdir \
            [list [list repmgr_site 127.0.0.1 $mport db_local_site on]]
	make_dbconfig $cdir \
            [list [list repmgr_site 127.0.0.1 $cport db_local_site on] \
                 [list repmgr_site 127.0.0.1 $mport db_bootstrap_helper on]]

	puts "\tRepmgr$tnum.a: Set up a pair of sites, two processes each."
	set cmds {
		"home $mdir"
		"output $testdir/m1output"
		"open_env"
		"start master"
	}
	set m1 [open_site_prog [subst $cmds]]

	set cmds {
		"home $mdir"
		"output $testdir/m2output"
		"open_env"
		"start master"
	}
	set m2 [open_site_prog [subst $cmds]]

	set cmds {
		"home $cdir"
		"output $testdir/c1output"
		"open_env"
		"start client"
	}
	set c1 [open_site_prog [subst $cmds]]

	set cmds {
		"home $cdir"
		"output $testdir/c2output"
		"open_env"
		"start client"
	}
	set c2 [open_site_prog [subst $cmds]]

	set cenv [berkdb_env -home $cdir]
	await_startup_done $cenv

	puts "\tRepmgr$tnum.b: Stop master's subordinate process (pause)."
	close $m2

	# Pause to let client notice the connection loss.
	tclsleep 3

	# The client main process is still running, but it shouldn't care about
	# a connection loss to the master's subordinate process.

	puts "\tRepmgr$tnum.c:\
	    Stop client's main process, then master's main process (pause)."
	close $c1
	tclsleep 2
	close $m1
	tclsleep 3

	# If the client main process were still running, it would have reacted
	# to the loss of the master by calling for an election.  However, with
	# only the client subordinate process still running, he cannot call for
	# an election.  So, we should see no elections ever having been
	# started.
	# 
	set election_count [stat_field $cenv rep_stat "Elections held"]
	puts "\tRepmgr$tnum.d: Check election count ($election_count)."
	error_check_good no_elections $election_count 0

	$cenv close
	close $c2
}
