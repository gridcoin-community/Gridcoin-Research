# See the file LICENSE for redistribution information.
#
# Copyright (c) 2006, 2013 Oracle and/or its affiliates.  All rights reserved.
#
# $Id$
#
# TEST	rep065
# TEST	Tests replication running with different versions.
# TEST	This capability is introduced with 4.5.
# TEST
# TEST	Start a replication group of 1 master and N sites, all
# TEST	running some historical version greater than or equal to 4.4.
# TEST	Take down a client and bring it up again running current.
# TEST	Run some upgrades, make sure everything works.
# TEST
# TEST	Each site runs the tcllib of its own version, but uses
# TEST	the current tcl code (e.g. test.tcl).
proc rep065 { method { nsites 3 } args } {
	source ./include.tcl
	global repfiles_in_memory
	global noenv_messaging
	set noenv_messaging 1

	#
	# Skip all methods but btree - we don't use the method, as we
	# run over all of them with varying versions.
	#
	if { $checking_valid_methods } {
		set test_methods { btree }
		return $test_methods
	}

	if { [is_btree $method] == 0 } {
		puts "Rep065: Skipping for method $method."
		return
	}

	set msg2 "and on-disk replication files"
	if { $repfiles_in_memory } {
		set msg2 "and in-memory replication files"
	}

	# Make the list of {method version} pairs to test.
	#
	set mvlist [method_version]
	set mvlen [llength $mvlist]
	puts "Rep065: Testing the following $mvlen method/version pairs:"
	puts "Rep065: $mvlist"
	puts "Rep065: $msg2"
	set count 1
	set total [llength $mvlist]
	set slist [setup_sites $nsites]
	foreach i $mvlist {
		puts "Rep065: Test iteration $count of $total: $i"
		rep065_sub $count $i $nsites $slist
		incr count
	}
	set noenv_messaging 0
}

proc rep065_sub { iter mv nsites slist } {
	source ./include.tcl
	global machids
	global util_path
	set machids {}
	set method [lindex $mv 0]
	set vers [lindex $mv 1]

	puts "\tRep065.$iter.a: Set up."
	# Whatever directory we started this process from is referred
	# to as the controlling directory.  It will contain the message
	# queue and start all the child processes.
	set controldir [pwd]
	env_cleanup $controldir/$testdir
	replsetup_noenv $controldir/$testdir/MSGQUEUEDIR

	# Set up the historical build directory.  The master will start
	# running with historical code.
	#
	# This test presumes we are running in the current build
	# directory and that the expected historical builds are
	# set up in a similar fashion.  If they are not, quit gracefully.

	set pwd [pwd]
	set homedir [file dirname [file dirname $pwd]]
	#
	# Cannot use test_path because that is relative to the current
	# directory (which will often be the old release directory).
	# We need to send in the pathname to the reputils path to the
	# current directory and that will be an absolute pathname.
	#
	set reputils_path $pwd/../test/tcl
	set histdir $homedir/$vers/build_unix
	if { [file exists $histdir] == 0 } {
		puts -nonewline "Skipping iteration $iter: cannot find"
		puts " historical version $vers."
		return
	}
	if { [file exists $histdir/db_verify] == 0 } {
		puts -nonewline "Skipping iteration $iter: historical version"
		puts " $vers is missing some executables.  Is it built?"
		return
	}

	set histtestdir $histdir/TESTDIR

	env_cleanup $histtestdir
	set markerdir $controldir/$testdir/MARKER
	file delete -force $markerdir

	# Create site directories.  They start running in the historical
	# directory, too.  They will be upgraded to the current version
	# first.
	set allids { }
	for { set i 0 } { $i < $nsites } { incr i } {
		set siteid($i) [expr $i + 1]
		set sid $siteid($i)
		lappend allids $sid
		set histdirs($sid) $histtestdir/SITE.$i
		set upgdir($sid) $controldir/$testdir/SITE.$i
		file mkdir $histdirs($sid)
		file mkdir $histdirs($sid)/DATADIR
		file mkdir $upgdir($sid)
		file mkdir $upgdir($sid)/DATADIR
	}

	# Open master env running 4.4.
	#
	# We know that slist has all sites starting in the histdir.
	# So if we encounter an upgrade value, we upgrade that client
	# from the hist dir.
	#
	set count 1
	foreach sitevers $slist {
		puts "\tRep065.b.$iter.$count: Run with sitelist $sitevers."
		#
		# Delete the marker directory each iteration so that
		# we don't find old data in there.
		#
		file delete -force $markerdir
		file mkdir $markerdir
		#
		# Get the chosen master index from the list of sites.
		#
		set mindex [get_master $nsites $sitevers]
		set meid [expr $mindex + 1]

		#
		# Kick off the test processes.  We need 1 test process
		# per site and 1 message process per site.
		#
		set pids {}
		for { set i 0 } { $i < $nsites } { incr i } {
			set upg [lindex $sitevers $i]
			set sid $siteid($i)
			#
			# If we are running "old" set up an array
			# saying if this site has run old/new yet.
			# The reason is that we want to "upgrade"
			# only the first time we go from old to new,
			# not every iteration through this loop.
			#
			if { $upg == 0 } {
				puts -nonewline "\t\tRep065.b: Test: Old site $i"
				set sitedir($i) $histdirs($sid)
				set already_upgraded($i) 0
			} else {
				puts -nonewline "\t\tRep065.b: Test: Upgraded site $i"
				set sitedir($i) $upgdir($sid)
				if { $already_upgraded($i) == 0 } {
					upg_repdir $histdirs($sid) $sitedir($i)
				}
				set already_upgraded($i) 1
			}
			if { $sid == $meid } {
				set state MASTER
				set runtest [list REPTEST $method 15 10]
				puts " (MASTER)"
			} else {
				set state CLIENT
				set runtest {REPTEST_GET}
				puts " (CLIENT)"
			}
			lappend pids [exec $tclsh_path $test_path/wrap.tcl \
			    rep065script.tcl \
			    $controldir/$testdir/$count.S$i.log \
		      	    SKIP \
			    START $state \
			    $runtest \
			    $sid $allids $controldir \
			    $sitedir($i) $reputils_path &]
			lappend pids [exec $tclsh_path $test_path/wrap.tcl \
			    rep065script.tcl \
			    $controldir/$testdir/$count.S$i.msg \
		    	    SKIP \
			    PROCMSGS $state \
		    	    NULL \
			    $sid $allids $controldir \
			    $sitedir($i) $reputils_path &]
		}

		watch_procs $pids 20
		#
		# At this point, clean up any message files.  The message
		# system leads to a significant number of duplicate
		# requests.  If the master site handled them after the
		# client message processes exited, then there can be
		# a large number of "dead" message files waiting for
		# non-existent clients.  Just clean up everyone.
		#
		for { set i 0 } { $i < $nsites } { incr i } {
			replclear_noenv $siteid($i)
		}

		#
		# Kick off the verification processes.  These just walk
		# their own logs and databases, so we don't need to have
		# a message process.  We need separate processes because
		# old sites need to use old utilities.
		#
		set pids {}
		puts "\tRep065.c.$iter.$count: Verify all sites."
		for { set i 0 } { $i < $nsites } { incr i } {
			if { $siteid($i) == $meid } {
				set state MASTER
			} else {
				set state CLIENT
			}
			lappend pids [exec $tclsh_path $test_path/wrap.tcl \
			    rep065script.tcl \
			    $controldir/$testdir/$count.S$i.ver \
		      	    SKIP \
			    VERIFY $state \
		    	    {LOG DB} \
			    $siteid($i) $allids $controldir \
			    $sitedir($i) $reputils_path &]
		}

		watch_procs $pids 10
		#
		# Now that each site created its verification files,
		# we can now verify everyone.
		#
		for { set i 0 } { $i < $nsites } { incr i } {
			if { $i == $mindex } {
				continue
			}
			puts \
	"\t\tRep065.c: Verify: Compare databases master and client $i"
			error_check_good db_cmp \
			    [filecmp $sitedir($mindex)/VERIFY/dbdump \
			    $sitedir($i)/VERIFY/dbdump] 0
			set upg [lindex $sitevers $i]
			# !!!
			# Although db_printlog works and can read old logs,
			# there have been some changes to the output text that
			# makes comparing difficult.  One possible solution
			# is to run db_printlog here, from the current directory
			# instead of from the historical directory.
			#
			if { $upg == 0 } {
				puts \
	"\t\tRep065.c: Verify: Compare logs master and client $i"
				error_check_good log_cmp \
				    [filecmp $sitedir($mindex)/VERIFY/prlog \
				    $sitedir($i)/VERIFY/prlog] 0
			} else {
				puts \
	"\t\tRep065.c: Verify: Compare LSNs master and client $i"
				error_check_good log_cmp \
				    [filecmp $sitedir($mindex)/VERIFY/loglsn \
				    $sitedir($i)/VERIFY/loglsn] 0
			}
		}

		#
		# At this point we have a master and sites all up to date
		# with each other.  Now, one at a time, upgrade the sites
		# to the current version and start everyone up again.
		incr count
	}
}

proc setup_sites { nsites } {
	#
	# Set up a list that goes from 0 to $nsites running
	# upgraded.  A 0 represents running old version and 1
	# represents running upgraded.  So, for 3 sites it will look like:
	# { 0 0 0 } { 1 0 0 } { 1 1 0 } { 1 1 1 }
	#
	set sitelist {}
	for { set i 0 } { $i <= $nsites } { incr i } {
		set l ""
		for { set j 1 } { $j <= $nsites } { incr j } {
			if { $i < $j } {
				lappend l 0
			} else {
				lappend l 1
			}
		}
		lappend sitelist $l
	}
	return $sitelist
}

proc upg_repdir { histdir upgdir } {
	global util_path

	#
	# Upgrade a site to the current version.  This entails:
	# 1.  Removing any old files from the upgrade directory.
	# 2.  Copy all old version files to upgrade directory.
	# 3.  Remove any __db files from upgrade directory except __db.rep*gen.
	# 4.  Force checkpoint in new version.
	file delete -force $upgdir

	# Recovery was run before as part of upgradescript.
	# Archive dir by copying it to upgrade dir.
	file copy -force $histdir $upgdir
	set dbfiles [glob -nocomplain $upgdir/__db*]
	foreach d $dbfiles {
		if { $d == "$upgdir/__db.rep.gen" ||
		    $d == "$upgdir/__db.rep.egen" } {
			continue
		}
		file delete -force $d
	}
	# Force current version checkpoint
	set stat [catch {eval exec $util_path/db_checkpoint -1 -h $upgdir} r]
	if { $stat != 0 } {
		puts "CHECKPOINT: $upgdir: $r"
	}
	error_check_good stat_ckp $stat 0
}

proc get_master { nsites verslist } {
	error_check_good vlist_chk [llength $verslist] $nsites
	#
	# When we can, simply run an election to get a new master.
	# We then verify we got an old client.
	#
	# For now, randomly pick among the old sites, or if no old
	# sites just randomly pick anyone.
	#
	set old_count 0
	# Pick 1 out of N old sites or 1 out of nsites if all upgraded.
	foreach i $verslist {
		if { $i == 0 } {
			incr old_count
		}
	}
	if { $old_count == 0 } {
		set old_count $nsites
	}
	set master [berkdb random_int 0 [expr $old_count - 1]]
	#
	# Since the Nth old site may not be at the Nth place in the
	# list unless we used the entire list, we need to loop to find
	# the right index to return.
	if { $old_count == $nsites } {
		return $master
	}
	set ocount 0
	set index 0
	foreach i $verslist {
		if { $i == 1 } {
			incr index
			continue
		}
		if { $ocount == $master } {
			return $index
		}
		incr ocount
		incr index
	}
	#
	# If we get here there is a problem in the code.
	#
	error "FAIL: get_master problem"
}

proc method_version { } {

	set mv {}

	# Set up version 5.2, which adds the method 'heap'.  Since 
	# heap is new, we'd like to test it heavily.  Always test a 
	# 5.2/heap pair, plus one other 5.2 with a random non-heap 
	# version.  Here's the hard-coded one:
	set db52 "db-5.2.36"
	lappend mv [list heap $db52]

	set methods {btree rbtree recno frecno rrecno queue queueext hash}
	set methods_len [expr [llength $methods] - 1]
	set midx [berkdb random_int 0 $methods_len]
	set method [lindex $methods $midx]
	lappend mv [list $method $db52]

	# Now take care of versions 4.4 though 5.1, which share
	# the same list of eight valid methods.
	set remaining_methods $methods
	set methods_len [expr [llength $remaining_methods] - 1]

	set versions {db-5.1.25 db-5.0.32 \
	    db-4.8.30 db-4.7.25 db-4.6.21 db-4.5.20 db-4.4.20}
	set remaining_versions $versions
	set versions_len [expr [llength $remaining_versions] - 1]

	# Walk through the list of methods and the list of versions and
	# pair them at random.  Stop when either list is empty.
	while { $versions_len >= 0 && $methods_len >= 0 } {
		set vidx [berkdb random_int 0 $versions_len]
		set midx [berkdb random_int 0 $methods_len]

		set version [lindex $remaining_versions $vidx]
		set method [lindex $remaining_methods $midx]

		set remaining_versions [lreplace $remaining_versions $vidx $vidx]
		set remaining_methods [lreplace $remaining_methods $midx $midx]

		incr versions_len -1
		incr methods_len -1

		lappend mv [list $method $version]
	}

	# If there are remaining versions, randomly assign any of 
	# the original methods to each one.
	while { $versions_len >= 0 } {

		set methods_len [expr [llength $methods] - 1]
		set midx [berkdb random_int 0 $methods_len]

		set version [lindex $remaining_versions 0]
		set method [lindex $methods $midx]

		set remaining_versions [lreplace $remaining_versions 0 0]
		incr versions_len -1

		lappend mv [list $method $version]
	}

	# If there are remaining methods, randomly assign any of 
	# the original versions to each one.
	while { $methods_len >= 0 } { 

		set versions_len [expr [llength $versions] - 1]
		set vidx [berkdb random_int 0 $versions_len]

		set version [lindex $versions $vidx]
		set method [lindex $remaining_methods 0]

		set remaining_methods [lreplace $remaining_methods 0 0]
		incr methods_len -1

		lappend mv [list $method $version]
	}
	return $mv
}
