# See the file LICENSE for redistribution information.
#
# Copyright (c) 1999, 2013 Oracle and/or its affiliates.  All rights reserved.
#
# $Id$
#
# TEST	env007
# TEST	Test DB_CONFIG config file options for berkdb env.
# TEST	1) Make sure command line option is respected
# TEST	2) Make sure that config file option is respected
# TEST	3) Make sure that if -both- DB_CONFIG and the set_<whatever>
# TEST		method is used, only the file is respected.
# TEST	Then test all known config options.
# TEST	Also test config options on berkdb open.  This isn't
# TEST	really env testing, but there's no better place to put it.
proc env007 { } {
	global errorInfo
	global errorCode
	global passwd
	global has_crypto
	source ./include.tcl

	puts "Env007: DB_CONFIG and getters test."
	puts "Env007.a: Test berkdb env options using getters and stat."

	# Set up options we can check via stat or getters.  Structure
	# of the list is:
	# 	0.  Arg used in berkdb env command
	# 	1.  Arg used in DB_CONFIG file
	#	2.  Value assigned in berkdb env command
	#	3.  Value assigned in DB_CONFIG file
	# 	4.  Message output during test
	#	5.  Stat command to run (empty if we can't get the info
	#		from stat).
	# 	6.  String to search for in stat output
	#	7.  Which arg to check in stat (needed for cases where
	#         we set more than one args at a time, but stat can
	# 	    only check one args, like cachesize)
	# 	8.  Arg used in getter
	#
	# The initial values for both locks and lock objects have silently
	# enforced minimums of 50 * #cpus. These values work for up to 8 cpus.
	set rlist {
	{ " -txn_init " "set_memory_init DB_MEM_TRANSACTION" "19" "31"
	    "Env007.a1: Txn Init" "txn_stat"
	    "Initial txns" "0" "get_tx_init" }
    	{ " -txn_max " "set_tx_max" "29" "51"
	    "Env007.a1: Txn Max" "txn_stat"
	    "Maximum txns" "0" "get_tx_max" }
	{ " -lock_locks " "set_memory_init DB_MEM_LOCK" "12407" "12429"
	    "Env007.a2: Lock Init" "lock_stat"
	    "Initial locks" "0" "get_lk_init_locks" }
	{ " -lock_max_locks " "set_lk_max_locks" "1070" "1290"
	    "Env007.a2: Lock Max" "lock_stat"
	    "Maximum locks" "0" "get_lk_max_locks" }
	{ " -lock_lockers " "set_memory_init DB_MEM_LOCKER" "150" "200"
	    "Env007.a3: Init Lockers" "lock_stat"
	    "Initial lockers" "0" "get_lk_init_lockers" }
	{ " -lock_max_lockers " "set_lk_max_lockers" "1500" "2000"
	    "Env007.a3: Max Lockers" "lock_stat"
	    "Maximum lockers" "0" "get_lk_max_lockers" }
	{ " -lock_objects " "set_memory_init DB_MEM_LOCKOBJECT" "12405" "12408"
	    "Env007.a4: Init Objects" "lock_stat"
	    "Initial objects" "0" "get_lk_init_objects" }
	{ " -lock_max_objects " "set_lk_max_objects" "1500" "2000"
	    "Env007.a4: Max Objects" "lock_stat"
	    "Maximum objects" "0" "get_lk_max_objects" }
	{ " -log_buffer " "set_lg_bsize" "65536" "131072"
	    "Env007.a5: Log Bsize" "log_stat"
	    "Log record cache size" "0" "get_lg_bsize" }
	{ " -log_max " "set_lg_max" "8388608" "9437184"
	    "Env007.a6: Log Max" "log_stat"
	    "Current log file size" "0" "get_lg_max" }
	{ " -cachesize " "set_cachesize" "0 536870912 1" "1 0 1"
	    "Env007.a7.0: Cachesize" "mpool_stat"
	    "Cache size (gbytes)" "0" "get_cachesize" }
	{ " -cachesize " "set_cachesize" "0 536870912 1" "1 0 1"
	    "Env007.a7.1: Cachesize" "mpool_stat"
	    "Cache size (bytes)" "1" "get_cachesize" }
	{ " -cachesize " "set_cachesize" "0 536870912 1" "1 0 1"
	    "Env007.a7.2: Cachesize" "mpool_stat"
	    "Number of caches" "2" "get_cachesize" }
	{ " -lock_timeout " "set_lock_timeout" "100" "120"
	    "Env007.a8: Lock Timeout" "lock_stat"
	    "Lock timeout value" "0" "get_timeout lock" }
	{ " -log_regionmax " "set_lg_regionmax" "8388608" "4194304"
	    "Env007.a9: Log Regionmax" ""
	    "Region size" "0" "get_lg_regionmax" }
	{ " -mpool_max_openfd " "set_mp_max_openfd" "17" "27"
	    "Env007.a10: Mmap max openfd" "mpool_stat"
	    "Maximum open file descriptors" "0" "get_mp_max_openfd" }
	{ " -mpool_max_write " "set_mp_max_write" "37 47" "57 67"
	    "Env007.a11.1: Mmap max write" "mpool_stat"
	    "Sleep after writing maximum buffers" "1" "get_mp_max_write" }
	{ " -mpool_mmap_size " "set_mp_mmapsize" "12582912" "8388608"
	    "Env007.a12: Mmapsize" "mpool_stat"
	    "Maximum memory-mapped file size" "0" "get_mp_mmapsize" }
	{ " -shm_key " "set_shm_key" "15" "35"
	    "Env007.a13: Shm Key" ""
	    "" "" "get_shm_key" }
	{ " -tmp_dir " "set_tmp_dir" "." "./TEMPDIR"
	    "Env007.a14: Temp dir" ""
	    "" "" "get_tmp_dir" }
	{ " -txn_timeout " "set_txn_timeout" "100" "120"
	    "Env007.a15: Txn timeout" "lock_stat"
	    "Transaction timeout value" "0" "get_timeout txn" }
	{ " -log_filemode " "set_lg_filemode" "417" "637"
	    "Env007.a16: Log FileMode" "log_stat"
	    "Log file mode" "0" "get_lg_filemode" }
	{" -lock_partitions " "set_lk_partitions" "10" "20"
	    "Env007.a17: Lock Partitions" "lock_stat"
	    "Number of lock table partitions" "0" "get_lk_partitions" }
	{" -mutex_set_align " "mutex_set_align" "8" "16"
	    "Env007.a18: Mutex align" "mutex_stat"
	    "Mutex align" "0" "mutex_get_align" }
	{" -mutex_set_incr " "mutex_set_increment" "1000" "1500"
	    "Env007.a19: Mutex increment" ""
	    "" "" "mutex_get_incr" }
	{" -mutex_set_max " "mutex_set_max" "2000" "2500"
	    "Env007.a20: Mutex max" "mutex_stat"
	    "Mutex max" "0" "mutex_get_max" }
	{" -mutex_set_tas_spins " "mutex_set_tas_spins" "60" "85"
	    "Env007.a21: Mutex tas spins" "mutex_stat"
	    "Mutex TAS spins" "0" "mutex_get_tas_spins" }
	{" -pagesize " "set_mp_pagesize" "4096" "8192"
	    "Env007.a22: Mpool pagesize" ""
	    "" "" "get_mp_pagesize" }
	{" -reg_timeout " "set_reg_timeout" "25000" "35000"
	    "Env007.a23: Register timeout" ""
	    "" "" "get_timeout reg" }
	}

	set e "berkdb_env_noerr -create -mode 0644 -home $testdir -txn "
	set qnxexclude {set_cachesize}

	foreach item $rlist {
		set envarg [lindex $item 0]
		set configarg [lindex $item 1]
		set envval [lindex $item 2]
		set configval [lindex $item 3]
		set msg [lindex $item 4]
		set statcmd [lindex $item 5]
		set statstr [lindex $item 6]
		set index [lindex $item 7]
		set getter [lindex $item 8]

		if { $is_qnx_test &&
		    [lsearch $qnxexclude $configarg] != -1 } {
			puts "\tEnv007.a: Skipping $configarg for QNX"
			continue
		}

		env_cleanup $testdir

		# First verify using just env args
		puts "\t$msg Environment argument only"
		set env [eval $e $envarg {$envval}]
		error_check_good envopen:0 [is_valid_env $env] TRUE
		error_check_good get_envval [eval $env $getter] $envval
		if { $statcmd != "" } {
			set statenvval [lindex $envval $index]
			# log_stat reports the sum of the specified
			# region size and the log buffer size.
			if { $statstr == "Region size" } {
				set lbufsize 32768
				set statenvval [expr $statenvval + $lbufsize]
			}
			env007_check $env $statcmd $statstr $statenvval
		}
		error_check_good envclose:0 [$env close] 0

		env_cleanup $testdir
		env007_make_config $configarg $configval

		# Verify using just config file
		puts "\t$msg Config file only"
		set env [eval $e]
		error_check_good envopen:1 [is_valid_env $env] TRUE
		error_check_good get_configval1 [eval $env $getter] $configval
		if { $statcmd != "" } {
			set statconfigval [lindex $configval $index]
			if { $statstr == "Region size" } {
				set statconfigval \
				    [expr $statconfigval + $lbufsize]
			}
			env007_check $env $statcmd $statstr $statconfigval
		}
		error_check_good envclose:1 [$env close] 0

		# Now verify using env args and config args
		puts "\t$msg Environment arg and config file"
		set env [eval $e $envarg {$envval}]
		error_check_good envopen:2 [is_valid_env $env] TRUE
		# Getter should retrieve config val, not envval.
		error_check_good get_configval2 [eval $env $getter] $configval
		if { $statcmd != "" } {
			env007_check $env $statcmd $statstr $statconfigval
		}
		error_check_good envclose:2 [$env close] 0
	}

	#
	# Test all options that can be set in DB_CONFIG.  Write it out
	# to the file and make sure we can open the env.  This execs
	# the config file code.  Also check with a getter that the
	# expected value is returned.
	#
	puts "\tEnv007.b1: Test berkdb env config options using getters\
	    and env open."

	# The cfglist variable contains options that can be set in DB_CONFIG.
	set cfglist {
	{ "set_data_dir" "." "get_data_dirs" "." }
	{ "add_data_dir" "." "get_data_dirs" "." }
	{ "set_metadata_dir" "." "get_metadata_dir" "."}
	{ "set_create_dir" "." "get_create_dir" "."}
	{ "set_flags" "db_auto_commit" "get_flags" "-auto_commit" }
	{ "set_flags" "db_cdb_alldb" "get_flags" "-cdb_alldb" }
	{ "set_flags" "db_direct_db" "get_flags" "-direct_db" }
	{ "set_flags" "db_dsync_db" "get_flags" "-dsync_db" }
	{ "set_flags" "db_multiversion" "get_flags" "-multiversion" }
	{ "set_flags" "db_nolocking" "get_flags" "-nolock" }
	{ "set_flags" "db_nommap" "get_flags" "-nommap" }
	{ "set_flags" "db_nopanic" "get_flags" "-nopanic" }
	{ "set_flags" "db_overwrite" "get_flags" "-overwrite" }
	{ "set_flags" "db_region_init" "get_flags" "-region_init" }
	{ "set_flags" "db_time_notgranted" "get_flags" "-time_notgranted" }
	{ "set_flags" "db_txn_nosync" "get_flags" "-nosync" }
	{ "set_flags" "db_txn_nowait" "get_flags" "-nowait" }
	{ "set_flags" "db_txn_snapshot" "get_flags" "-snapshot" }
	{ "set_flags" "db_txn_write_nosync" "get_flags" "-wrnosync" }
	{ "set_flags" "db_yieldcpu" "get_flags" "-yield" }
	{ "set_flags" "db_log_inmemory" "log_get_config" "inmemory" }
	{ "set_flags" "db_direct_log" "log_get_config" "direct" }
	{ "set_flags" "db_dsync_log" "log_get_config" "dsync" }
	{ "set_flags" "db_log_autoremove" "log_get_config" "autoremove" }
	{ "set_lg_bsize" "65536" "get_lg_bsize" "65536" }
	{ "set_lg_dir" "." "get_lg_dir" "." }
	{ "set_lg_max" "8388608" "get_lg_max" "8388608" }
	{ "set_lg_regionmax" "262144" "get_lg_regionmax" "262144" }
	{ "set_lk_detect" "db_lock_default" "get_lk_detect" "default" }
	{ "set_lk_detect" "db_lock_expire" "get_lk_detect" "expire" }
	{ "set_lk_detect" "db_lock_maxlocks" "get_lk_detect" "maxlocks" }
	{ "set_lk_detect" "db_lock_maxwrite" "get_lk_detect" "maxwrite" }
	{ "set_lk_detect" "db_lock_minlocks" "get_lk_detect" "minlocks" }
	{ "set_lk_detect" "db_lock_minwrite" "get_lk_detect" "minwrite" }
	{ "set_lk_detect" "db_lock_oldest" "get_lk_detect" "oldest" }
	{ "set_lk_detect" "db_lock_random" "get_lk_detect" "random" }
	{ "set_lk_detect" "db_lock_youngest" "get_lk_detect" "youngest" }
	{ "set_lk_max_lockers" "1500" "get_lk_max_lockers" "1500" }
	{ "set_lk_max_locks" "1290" "get_lk_max_locks" "1290" }
	{ "set_lk_max_objects" "1500" "get_lk_max_objects" "1500" }
	{ "set_lk_partitions" "5" "get_lk_partitions" "5" }
	{ "set_lock_timeout" "100" "get_timeout lock" "100" }
	{ "set_mp_mmapsize" "12582912" "get_mp_mmapsize" "12582912" }
	{ "set_mp_max_write" "10 20" "get_mp_max_write" "10 20" }
	{ "set_mp_max_openfd" "10" "get_mp_max_openfd" "10" }
	{ "set_mp_pagesize" "8192" "get_mp_pagesize" "8192" }
	{ "set_open_flags" "db_private" "get_open_flags" "-private" }
	{ "set_open_flags" "db_private on" "get_open_flags" "-private" }
	{ "set_open_flags" "db_init_rep" "get_open_flags" "-rep" }
	{ "set_open_flags" "db_thread" "get_open_flags" "-thread" }
	{ "set_region_init" "1" "get_flags" "-region_init" }
	{ "set_reg_timeout" "60" "get_timeout reg" "60" }
	{ "set_shm_key" "15" "get_shm_key" "15" }
	{ "set_tas_spins" "15" "get_tas_spins" "15" }
	{ "set_tmp_dir" "." "get_tmp_dir" "." }
	{ "set_tx_max" "31" "get_tx_max" "31" }
	{ "set_txn_timeout" "50" "get_timeout txn" "50" }
	{ "set_verbose" "db_verb_deadlock" "get_verbose deadlock" "on" }
	{ "set_verbose" "db_verb_register" "get_verbose register" "on" }
	{ "set_verbose" "db_verb_replication" "get_verbose rep" "on" }
	{ "set_verbose" "db_verb_rep_elect" "get_verbose rep_elect" "on" }
	{ "set_verbose" "db_verb_rep_lease" "get_verbose rep_lease" "on" }
	{ "set_verbose" "db_verb_rep_misc" "get_verbose rep_misc" "on" }
	{ "set_verbose" "db_verb_rep_msgs" "get_verbose rep_msgs" "on" }
	{ "set_verbose" "db_verb_rep_sync" "get_verbose rep_sync" "on" }
	{ "set_verbose" "db_verb_rep_system" "get_verbose rep_system" "on" }
	{ "set_verbose" "db_verb_repmgr_connfail" 
	    "get_verbose repmgr_connfail" "on" }
	{ "set_verbose" "db_verb_repmgr_misc" "get_verbose repmgr_misc" "on" }
	{ "set_verbose" "db_verb_waitsfor" "get_verbose wait" "on" }
	{ "log_set_config" "db_log_direct" "log_get_config" "direct" }
	{ "log_set_config" "db_log_dsync" "log_get_config" "dsync" }
	{ "log_set_config" "db_log_auto_remove" "log_get_config" "autoremove" }
	{ "log_set_config" "db_log_in_memory" "log_get_config" "inmemory" }
	{ "log_set_config" "db_log_zero" "log_get_config" "zero" }
	{ "mutex_set_align" "8" "mutex_get_align" "8" }
	{ "mutex_set_increment" "100" "mutex_get_incr" "100" }
	{ "mutex_set_max" "1000" "mutex_get_max" "1000" }
	{ "mutex_set_tas_spins" "32" "mutex_get_tas_spins" "32" }
	}

	env_cleanup $testdir
	set e "berkdb_env_noerr -create -mode 0644 -home $testdir \
	    -txn -lock -log -thread"
	set directlist {db_direct_db db_log_direct db_direct_log}

	foreach item $cfglist {
		env_cleanup $testdir
		set configarg [lindex $item 0]
		set configval [lindex $item 1]
		set getter [lindex $item 2]
		set getval [lindex $item 3]

		set extra_cmd {}
		if {$configarg == "set_create_dir"} {
			set extra_cmd "-add_dir $configval"
		}

		env007_make_config $configarg $configval

		# Verify using config file
		puts "\t\tEnv007.b1: $configarg $configval"

		# Unconfigured/unsupported direct I/O is not reported
		# as a failure.
		set directmsg \
		    "direct I/O either not configured or not supported"
		if {[catch { eval $e $extra_cmd} env ]} {
			if { [lsearch $directlist $configval] != -1 && \
			    [is_substr $env $directmsg] == 1 } {
				continue
			} else {
				puts "FAIL: $env"
				continue
			}
		}
		error_check_good envvalid:1 [is_valid_env $env] TRUE

		# get_open_flags returns a whole string of flags, so pick
		# out the one we're looking for.  The other getters are 
		# specific to the designated flag.
		if { $getter == "get_open_flags" } {
			set flags [eval $env $getter]
			error_check_good flag_found [is_substr $flags $getval] 1
		} elseif { $getter == "log_get_config" } {
			error_check_good log_get [eval $env $getter $getval] 1
		} else {	
			error_check_good getter:1 [eval $env $getter] $getval
		}

		error_check_good envclose:1 [$env close] 0
	}

	# Some verbose options send output to stdout that we want
	# to inspect, or don't want to send to the screen -- handle
	# these by running them in their own Tcl shell.
	# 
	# We use the same method as section .b1, but cfglist has one
	# additional argument where we specify the message to look
	# for in the log file.

	puts "\tEnv007.b2: Test berkdb env config options using getters\
	    and env open in a separate Tcl shell."

	# The cfglist variable contains options that can be set in DB_CONFIG.
	set cfglist {
		{ "set_verbose" "db_verb_recovery" \
		    "get_verbose recovery" "on" "No log files found"}
		{ "set_verbose" "db_verb_fileops" \
		    "get_verbose fileops" "on" "fileops" }
		{ "set_verbose" "db_verb_fileops_all" \
		     "get_verbose fileops_all" "on" "fileops: write" }
	}

	foreach item $cfglist {
		env_cleanup $testdir

		set configarg [lindex $item 0]
		set configval [lindex $item 1]
		set getter [lindex $item 2]
		set getval [lindex $item 3]
		set checkmsg [lindex $item 4]

		puts "\t\tEnv007.b2: $configarg $configval"

		set pid [exec $tclsh_path $test_path/wrap.tcl \
		    env007script.tcl $testdir/env007.b2.output $configarg \
		    $configval $getter $getval &]

		tclsleep 2
		watch_procs $pid 5
		error_check_good found_message \
		    [findstring $checkmsg $testdir/env007.b2.output] 1

		# Clean up before the next part.
		fileremove -f $testdir/DB_CONFIG
	}

	puts "\tEnv007.c: Test berkdb env options using getters and env open."
	# The envopenlist variable contains options that can be set using
	# berkdb env.  We always set -mpool.

	set envopenlist {
	{ "-system_mem" "-shm_key 20" "-system_mem" "get_open_flags" }
	{ "-cdb" "" "-cdb" "get_open_flags" }
	{ "-errpfx" "FOO" "FOO" "get_errpfx" }
	{ "-lock" "" "-lock" "get_open_flags" }
	{ "-log" "" "-log" "get_open_flags" }
	{ "" "" "-mpool" "get_open_flags" }
	{ "-txn -rep" "" "-rep" "get_open_flags" }
	{ "-txn" "" "-txn" "get_open_flags" }
	{ "-recover" "-txn" "-recover" "get_open_flags" }
	{ "-recover_fatal" "-txn" "-recover_fatal" "get_open_flags" }
	{ "-register" "-txn -recover" "-register" "get_open_flags" }
	{ "-use_environ" "" "-use_environ" "get_open_flags" }
	{ "-use_environ_root" "" "-use_environ_root" "get_open_flags" }
	{ "" "" "-create" "get_open_flags" }
	{ "-private" ""  "-private" "get_open_flags" }
	{ "-thread" "" "-thread" "get_open_flags" }
	{ "-txn_timestamp" "100000000" "100000000" "get_tx_timestamp" }
	}

	if { $has_crypto == 1 } {
		lappend envopenlist {
		    "-encryptaes" "$passwd" "-encryptaes" "get_encrypt_flags" }
	}

	set e "berkdb_env_noerr -create -mode 0644 -home $testdir"
	set qnxexclude {-system_mem}
	foreach item $envopenlist {
		env_cleanup $testdir
		set envarg [lindex $item 0]
		set envval [lindex $item 1]
		set retval [lindex $item 2]
		set getter [lindex $item 3]

		if { $is_qnx_test &&
		    [lsearch $qnxexclude $envarg] != -1} {
			puts "\t\tEnv007: Skipping $envarg for QNX"
			continue
		}

		puts "\t\tEnv007.c: $envarg $retval"

		# Set up env
		set ret [catch {eval $e $envarg $envval} env]

		if { $ret != 0 } {
			# If the env open failed, it may be because we're on a
			# platform such as HP-UX 10 that won't support mutexes
			# in shmget memory.  Verify that the return value was
			# EINVAL or EOPNOTSUPP and bail gracefully.
			error_check_good \
			    is_shm_test [is_substr $envarg -system_mem] 1
			error_check_good returned_error [expr \
			    [is_substr $errorCode EINVAL] || \
			    [is_substr $errorCode EOPNOTSUPP]] 1
			puts "Warning: platform\
			    does not support mutexes in shmget memory."
			puts "Skipping shared memory mpool test."
		} else {
			error_check_good env_open [is_valid_env $env] TRUE

			# Check that getter retrieves expected retval.
			set get_retval [eval $env $getter]
			if { [is_substr $get_retval $retval] != 1 } {
				puts "FAIL: $retval\
				    should be a substring of $get_retval"
				continue
			}
			error_check_good envclose [$env close] 0

			# The -encryptany flag can only be tested on an existing
			# environment that supports encryption, so do it here.
			if { $has_crypto == 1 } {
				if { $envarg == "-encryptaes" } {
					set env [eval berkdb_env -home $testdir\
					    -encryptany $passwd]
					error_check_good get_encryptany \
					    [eval $env get_encrypt_flags] \
					    "-encryptaes"
					error_check_good envclose [$env close] 0
				}
			}
		}
	}

	puts "\tEnv007.d: Test berkdb env options using set_flags and getters."

	# The flaglist variable contains options that can be set using
	# $env set_flags.
	set flaglist {
	{ "-direct_db" }
	{ "-dsync_db" }
	{ "-nolock" }
	{ "-nommap" }
	{ "-nopanic" }
	{ "-nosync" }
	{ "-overwrite" }
	{ "-panic" }
	{ "-snapshot" }
	{ "-time_notgranted" }
	{ "-wrnosync" }
        { "-hotbackup_in_progress" }
	}
	set e "berkdb_env_noerr -create -mode 0644 -txn -home $testdir"
	set directlist {-direct_db}
	foreach item $flaglist {
		set flag [lindex $item 0]
		env_cleanup $testdir

		puts "\t\tEnv007.d: $flag"
		# Set up env
		set env [eval $e]
		error_check_good envopen [is_valid_env $env] TRUE

		# Use set_flags to turn on new env characteristics.
		#
		# Unconfigured/unsupported direct I/O is not reported
		# as a failure. 
		if {[catch { $env set_flags $flag on } res ]} {
			if { [lsearch $directlist $flag] != -1 && \
			    [is_substr $res $directmsg] == 1 } {
				error_check_good env_close [$env close] 0
				continue
			} else {
				puts "FAIL: $res"
				error_check_good env_close [$env close] 0
				continue
			}
		} else {
			error_check_good "flag $flag on" $res 0
		}

		# Check that getter retrieves expected retval.
		# A call to get_flags with -panic will report 
		# that the env is panicked, so skip over this and 
		# turn it off. 
		if { $flag != "-panic" } {
			set get_retval [eval $env get_flags]
			if { [is_substr $get_retval $flag] != 1 } {
				puts "FAIL: $flag should\
				    be a substring of $get_retval"
				error_check_good env_close [$env close] 0
				continue
			}
		}

		# Use set_flags to turn off env characteristics, make sure
		# they are gone.
		error_check_good "flag $flag off" [$env set_flags $flag off] 0
		set get_retval [eval $env get_flags]
		if { [is_substr $get_retval $flag] == 1 } {
			puts "FAIL: $flag should not be in $get_retval"
			error_check_good env_close [$env close] 0
			continue
		}

		error_check_good envclose [$env close] 0
	}
	puts "\tEnv007.d1: Test berkdb env options using log_set_config and getters."

	# The flaglist variable contains options that can be set using
	# $env log_config.
	set flaglist {
	{ "autoremove" }
	{ "direct" }
	{ "dsync" }
	{ "zero" }
	}
	set e "berkdb_env_noerr -create -txn -mode 0644 -home $testdir"
	set directlist {direct}
	foreach item $flaglist {
		set flag [lindex $item 0]
		env_cleanup $testdir

		# Set up env
		set env [eval $e]
		error_check_good envopen [is_valid_env $env] TRUE

		# Use set_flags to turn on new env characteristics.
		#
		# Unconfigured/unsupported direct I/O is not reported
		# as a failure.
		if {[catch { $env log_config $flag on } res ]} {
			if { [lsearch $directlist $flag] != -1 && \
			    [is_substr $res $directmsg] == 1 } {
				error_check_good env_close [$env close] 0
				continue
			} else {
				puts "FAIL: $res"
				error_check_good env_close [$env close] 0
				continue
			}
		} else {
			error_check_good "flag $flag on" $res 0
		}

		# Check that getter retrieves expected retval.
		set get_retval [eval $env log_get_config $flag]
		if { $get_retval != 1 } {
			puts "FAIL: $flag is not on"
			error_check_good env_close [$env close] 0
			continue
		}
		# Use set_flags to turn off env characteristics, make sure
		# they are gone.
		error_check_good "flag $flag off" [$env log_config $flag off] 0
		set get_retval [eval $env log_get_config $flag]
		if { $get_retval == 1 } {
			puts "FAIL: $flag should off"
			error_check_good env_close [$env close] 0
			continue
		}

		error_check_good envclose [$env close] 0
	}

	puts "\tEnv007.e: Test env get_home."
	env_cleanup $testdir
	# Set up env
	set env [eval $e]
	error_check_good env_open [is_valid_env $env] TRUE
	# Test for correct value.
	set get_retval [eval $env get_home]
	error_check_good get_home $get_retval $testdir
	error_check_good envclose [$env close] 0

	puts "\tEnv007.f: Test that bad config values are rejected."
	set cfglist {
	{ "set_cache_max" "1" }
	{ "set_intermediate_dir_mode" "0644 0666" }
	{ "set_cachesize" "1048576" }
	{ "set_flags" "db_xxx" }
	{ "set_flags" "1" }
	{ "set_flags" "db_txn_nosync x" }
	{ "set_flags" "db_txn_nosync x x1" }
	{ "log_set_config" "db_log_xxx" }
	{ "log_set_config" "db_log_auto_remove x" }
	{ "log_set_config" "db_log_auto_remove x x1" }
	{ "set_lg_bsize" "db_xxx" }
	{ "set_lg_max" "db_xxx" }
	{ "set_lg_regionmax" "db_xxx" }
	{ "set_lock_timeout" "lock 500"}
	{ "set_lk_detect" "db_xxx" }
	{ "set_lk_detect" "1" }
	{ "set_lk_detect" "db_lock_youngest x" }
	{ "set_lk_max_locks" "db_xxx" }
	{ "set_lk_max_lockers" "db_xxx" }
	{ "set_lk_max_objects" "db_xxx" }
	{ "set_mp_max_openfd" "1 2" }
	{ "set_mp_max_write" "1 2 3" }
	{ "set_mp_mmapsize" "db_xxx" }
	{ "set_open_flags" "db_private db_thread db_init_rep" }
	{ "set_open_flags" "db_private x" }
	{ "set_open_flags" "db_xxx" }
	{ "set_region_init" "db_xxx" }
	{ "set_region_init" "db_xxx 1" }
	{ "set_region_init" "100" }
	{ "set_reg_timeout" "reg 5000" }
	{ "set_shm_key" "db_xxx" }
	{ "set_shm_key" ""}
	{ "set_shm_key" "11 12 13"}
	{ "set_tas_spins" "db_xxx" }
	{ "set_tx_max" "db_xxx" }
	{ "set_txn_timeout" "txn 5000" }
	{ "set_verbose" "db_xxx" }
	{ "set_verbose" "1" }
	{ "set_verbose" "db_verb_recovery x" }
	{ "set_verbose" "db_verb_recovery x x1" }
	}

	set e "berkdb_env_noerr -create -mode 0644 \
	    -home $testdir -log -lock -txn "
	foreach item $cfglist {
		set configarg [lindex $item 0]
		set configval [lindex $item 1]

		env007_make_config $configarg $configval

		#  verify using just config file
		set stat [catch {eval $e} ret]
		error_check_good envopen $stat 1
		error_check_good error [is_substr $errorCode EINVAL] 1
	}

	puts "\tEnv007.g: Config name error set_xxx"
	set e "berkdb_env_noerr -create -mode 0644 \
	    -home $testdir -log -lock -txn "
	env007_make_config "set_xxx" 1
	set stat [catch {eval $e} ret]
	error_check_good envopen $stat 1
	error_check_good error [is_substr $errorInfo \
		    "unrecognized name-value pair"] 1

	puts "\tEnv007.h: Test berkdb open flags and getters."
	# Check options that we configure with berkdb open and
	# query via getters.  Structure of the list is:
	# 	0.  Flag used in berkdb open command
	#	1.  Value specified to flag
	#	2.  Specific method, if needed
	# 	3.  Arg used in getter
	set olist {
	{ "-minkey" "4" " -btree " "get_bt_minkey" }
	{ "-cachesize" "0 1048576 1" "" "get_cachesize" }
	{ "" "FILENAME DBNAME" "" "get_dbname" }
	{ "" "" "" "get_env" }
	{ "-errpfx" "ERROR:" "" "get_errpfx" }
	{ "" "-chksum" "" "get_flags" }
	{ "-delim" "58" "-recno" "get_re_delim" }
	{ "" "-dup" "" "get_flags" }
	{ "" "-dup -dupsort" "" "get_flags" }
	{ "" "-dup" "-hash" "get_flags" }
	{ "" "-dup -dupsort" "-hash" "get_flags" }
	{ "" "-recnum" "" "get_flags" }
	{ "" "-revsplitoff" "" "get_flags" }
	{ "" "-revsplitoff" "-hash" "get_flags" }
	{ "" "-inorder" "-queue" "get_flags" }
	{ "" "-renumber" "-recno" "get_flags" }
	{ "" "-snapshot" "-recno" "get_flags" }
	{ "" "-create" "" "get_open_flags" }
	{ "" "-create -read_uncommitted" "" "get_open_flags" }
	{ "" "-create -excl" "" "get_open_flags" }
	{ "" "-create -nommap" "" "get_open_flags" }
	{ "" "-create -thread" "" "get_open_flags" }
	{ "" "-create -truncate" "" "get_open_flags" }
	{ "-ffactor" "40" " -hash " "get_h_ffactor" }
	{ "-lorder" "4321" "" "get_lorder" }
	{ "-nelem" "10000" " -hash " "get_h_nelem" }
	{ "-pagesize" "4096" "" "get_pagesize" }
	{ "-extent" "4" "-queue" "get_q_extentsize" }
	{ "-len" "20" "-recno" "get_re_len" }
	{ "-pad" "0" "-recno" "get_re_pad" }
	{ "-source" "include.tcl" "-recno" "get_re_source" }
	{ "-heap_regionsize" "1000" "-heap" "get_heap_regionsize" }
	}

	set o "berkdb_open_noerr -create -mode 0644"
	foreach item $olist {
		cleanup $testdir NULL
		set flag [lindex $item 0]
		set flagval [lindex $item 1]
		set method [lindex $item 2]
		if { $method == "" } {
			set method " -btree "
		}
		set getter [lindex $item 3]

		puts "\t\tEnv007.h: $flag $flagval $method"

		# Check that open is successful with the flag.
		# The option -cachesize requires grouping for $flagval.
		if { $flag == "-cachesize" } {
			set ret [catch {eval $o $method $flag {$flagval}\
			    $testdir/a.db} db]
		} else {
			set ret [catch {eval $o $method $flag $flagval\
			    $testdir/a.db} db]
		}
		if { $ret != 0 } {
			# If the open failed, it may be because we're on a
			# platform such as HP-UX 10 that won't support
			# locks in process-local memory.
			# Verify that the return value was EOPNOTSUPP
			# and bail gracefully.
			error_check_good \
			    is_thread_test [is_substr $flagval -thread] 1
			error_check_good returned_error [expr \
			    [is_substr $errorCode EINVAL] || \
			    [is_substr $errorCode EOPNOTSUPP]] 1
			puts "Warning: platform does not support\
			    locks inside process-local memory."
			puts "Skipping test of -thread flag."
		} else {
			error_check_good dbopen:0 [is_valid_db $db] TRUE

			# Check that getter retrieves the correct value.
			# Cachesizes under 500MB are adjusted upward to
			# about 25% so just make sure we're in the right
			# ballpark, between 1.2 and 1.3 of the original value.
			if { $flag == "-cachesize" } {
				set retval [eval $db $getter]
				set retbytes [lindex $retval 1]
				set setbytes [lindex $flagval 1]
				error_check_good cachesize_low [expr\
				    $retbytes > [expr $setbytes * 6 / 5]] 1
				error_check_good cachesize_high [expr\
				    $retbytes < [expr $setbytes * 13 / 10]] 1
			} else {
				error_check_good get_flagval \
				    [eval $db $getter] $flagval
			}
			error_check_good dbclose:0 [$db close] 0
		}
	}

	puts "\tEnv007.i: Test berkdb_open -rdonly."
	# This test is done separately because -rdonly can only be specified
	# on an already existing database.
	set flag "-rdonly"
	set db [eval berkdb_open $flag $testdir/a.db]
	error_check_good open_rdonly [is_valid_db $db] TRUE

	error_check_good get_rdonly [eval $db get_open_flags] $flag
	error_check_good dbclose:0 [$db close] 0

	puts "\tEnv007.j: Test berkdb open flags and getters\
	    requiring environments."
	# Check options that we configure with berkdb open and
	# query via getters.  Structure of the list is:
	# 	0.  Flag used in berkdb open command
	#	1.  Value specified to flag
	#	2.  Specific method, if needed
	# 	3.  Arg used in getter
	# 	4.  Additional flags needed in setting up env

	set elist {
	{ "" "-auto_commit" "" "get_open_flags" "" }
	{ "" "-notdurable" "" "get_flags" "" }
	}

	if { $has_crypto == 1 } {
		lappend elist \
		    { "" "-encrypt" "" "get_flags" "-encryptaes $passwd" }
	}

	set e "berkdb_env -create -home $testdir -txn "
	set o "berkdb_open -create -btree -mode 0644 "
	foreach item $elist {
		env_cleanup $testdir
		set flag [lindex $item 0]
		set flagval [lindex $item 1]
		set method [lindex $item 2]
		if { $method == "" } {
			set method " -btree "
		}
		set getter [lindex $item 3]
		set envflag [lindex $item 4]

		puts "\t\tEnv007.j: $flag $flagval"

		# Check that open is successful with the flag.
		set env [eval $e $envflag]
		set db [eval $o -env $env $flag $flagval a.db]
		error_check_good dbopen:0 [is_valid_db $db] TRUE

		# Check that getter retrieves the correct value
		set get_flagval [eval $db $getter]
		error_check_good get_flagval [is_substr $get_flagval $flagval] 1
		error_check_good dbclose [$db close] 0
		error_check_good envclose [$env close] 0
	}

	puts "\tEnv007.k: Test berkdb_open DB_TXN_NOSYNC and DB_TXN_WRITE_NOSYNC."
	# Test all combinations of DB_TXN_NOSYNC and DB_TXN_WRITE_NOSYNC. If we're
	# setting both of them, the previous setting would be cleared.
	set cfglist {
	{ "db_txn_nosync" "on"  "db_txn_write_nosync" "on"  "-nosync" "0" "-wrnosync" "1"}
	{ "db_txn_nosync" "off" "db_txn_write_nosync" "on"  "-nosync" "0" "-wrnosync" "1"}
	{ "db_txn_nosync" "on"  "db_txn_write_nosync" "off" "-nosync" "1" "-wrnosync" "0"}
	{ "db_txn_nosync" "off" "db_txn_write_nosync" "off" "-nosync" "0" "-wrnosync" "0"}
	{ "db_txn_write_nosync" "on"  "db_txn_nosync" "on"  "-wrnosync" "0" "-nosync" "1"}
	{ "db_txn_write_nosync" "off" "db_txn_nosync" "on"  "-wrnosync" "0" "-nosync" "1"}
	{ "db_txn_write_nosync" "on"  "db_txn_nosync" "off" "-wrnosync" "1" "-nosync" "0"}
	{ "db_txn_write_nosync" "off" "db_txn_nosync" "off" "-wrnosync" "0" "-nosync" "0"}
	}

	foreach item $cfglist {
		env_cleanup $testdir
		set cfg1 [lindex $item 0]
		set val1 [lindex $item 1]
		set cfg2 [lindex $item 2]
		set val2 [lindex $item 3]
		set chk_cfg1 [lindex $item 4]
		set chk_val1 [lindex $item 5]
		set chk_cfg2 [lindex $item 6]
		set chk_val2 [lindex $item 7]

		env007_append_config "w" "set_flags" "$cfg1" "$val1"
		env007_append_config "a" "set_flags" "$cfg2" "$val2"

		set env [eval $e]
		error_check_good envopen:1 [is_valid_env $env] TRUE

		# Check flags
		set flags [eval $env "get_flags"]
		error_check_good flag_found [is_substr $flags $chk_cfg1] $chk_val1
		error_check_good flag_found [is_substr $flags $chk_cfg2] $chk_val2
		error_check_good envclose [$env close] 0
	}
}

proc env007_check { env statcmd statstr testval } {
	set stat [$env $statcmd]
	set checked 0
	foreach statpair $stat {
		if {$checked == 1} {
			break
		}
		set statmsg [lindex $statpair 0]
		set statval [lindex $statpair 1]
		if {[is_substr $statmsg $statstr] != 0} {
			set checked 1
			error_check_good $statstr:ck $statval $testval
		}
	}
	error_check_good $statstr:test $checked 1
}

proc env007_make_config { carg cval } {
	global testdir

	set cid [open $testdir/DB_CONFIG w]
	puts $cid "$carg $cval"
	close $cid
}

proc env007_append_config { mode carg cval onoff } {
	global testdir

	set cid [open $testdir/DB_CONFIG $mode]
	puts $cid "$carg $cval $onoff"
	close $cid
}

proc env007_eval_env { e } {
	eval $e 
}
