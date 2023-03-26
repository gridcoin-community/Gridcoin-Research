# See the file LICENSE for redistribution information. 
#
# Copyright (c) 2007, 2013 Oracle and/or its affiliates.  All rights reserved.
# 
# $Id$
#
# TEST	backup
# TEST 	Test of hotbackup functionality. 
# TEST	
# TEST	Do all the of the following tests with and without 
# TEST	the -c (checkpoint) option; and with and without the
# TEST  transactional bulk loading optimization.  Make sure
# TEST  that -c and -d (data_dir) are not allowed together. 
# TEST
# TEST	(1) Test that plain and simple hotbackup works. 
# TEST	(2) Test with -data_dir (-d). 
# TEST	(3) Test updating an existing hot backup (-u).
# TEST	(4) Test with absolute path. 
# TEST	(5) Test with DB_CONFIG, setting log_dir (-l)
# TEST		and data_dir (-d).
# TEST	(6) DB_CONFIG and update.  
# TEST	(7) Repeat hot backup (non-update) with DB_CONFIG, 
# TEST		DB_CONFIG (-D) and existing directories. 

proc backup { {nentries 1000} } {
	source ./include.tcl
	global util_path

	set omethod "-btree"
	set testfile "foo.db"
	set backupdir "backup"

	# Set up small logs so we quickly create more than one. 
	set log_size 20000
	set env_flags " -create -txn -home $testdir -log_max $log_size"
	set db_flags " -create $omethod -auto_commit $testfile "

	foreach txnmode { normal bulk } {
		if { $txnmode == "bulk" } {
			set bmsg "with bulk optimization"
		} else {
			set bmsg "without bulk optimization"
		}
		foreach option { checkpoint nocheckpoint } {
			if { $option == "checkpoint" } {
				set c "c"
				set msg "with checkpoint"
			} else {
				set c ""
				set msg "without checkpoint"
			}
			puts "Backuptest $bmsg $msg."

			env_cleanup $testdir
			env_cleanup $backupdir 

			set env [eval {berkdb_env} $env_flags]
			set db [eval {berkdb_open} -env $env $db_flags]
			if { $txnmode == "bulk" } {
				set txn [$env txn -txn_bulk]
			} else {
				set txn [$env txn]
			}
			populate $db $omethod $txn $nentries 0 0 
			$txn commit

			# Backup directory is empty before hot backup.
			set files [glob -nocomplain $backupdir/*]
			error_check_good no_files [llength $files] 0

			puts "\tBackuptest.a: Hot backup to directory $backupdir."
			if {[catch { eval exec $util_path/db_hotbackup\
			    -${c}vh $testdir -b $backupdir } res ] } {
				error "FAIL: $res"
			}

			set logfiles [glob $backupdir/log*]
			error_check_bad found_logs [llength $logfiles] 0
			error_check_good found_db [file exists $backupdir/$testfile] 1 

			# If either checkpoint or bulk is in effect, the copy
			# will exactly match the original database.
			if { $option == "checkpoint" || $txnmode == "bulk"} {

			    puts "\tBackuptest.a2: Verify backup matches original file."
			    if {[catch { eval exec $util_path/db_dump\
				-f $testdir/dump1 $testdir/$testfile } res ] } {
				    error "FAIL db_dump: $res"
			    }
			    if {[catch { eval exec $util_path/db_dump\
				-f $testdir/dump2 $backupdir/$testfile } res ] } {
				    error "FAIL db_dump: $res"
			    }
			    error_check_good compare_dump \
				[filecmp $testdir/dump1 $testdir/dump2] 0
			}

			error_check_good db_close [$db close] 0
			error_check_good env_close [$env close] 0
			env_cleanup $testdir
			env_cleanup $backupdir

			puts "\tBackuptest.b: Hot backup with data_dir."
			file mkdir $testdir/data1
			error_check_good db_data_dir\
			    [file exists $testdir/data1/$testfile] 0

			# Create a new env with data_dir.
			set env [eval {berkdb_env_noerr} $env_flags -data_dir data1]
			set db [eval {berkdb_open} -env $env $db_flags]

			if { $txnmode == "bulk" } {
				set txn [$env txn -txn_bulk]
			} else {
				set txn [$env txn]
			}

			populate $db $omethod $txn $nentries 0 0 
			$txn commit

			# Check that data went into data_dir.
			error_check_good db_data_dir\
			    [file exists $testdir/data1/$testfile] 1

			# You may not specify both -d (data_dir) and -c (checkpoint).
			set msg2 "cannot specify -d and -c"	
			if { $option == "checkpoint" } {
				catch {eval exec $util_path/db_hotbackup\
				     -${c}vh $testdir -b $backupdir\
				     -d $testdir/data1} res
				error_check_good c_and_d [is_substr $res $msg2] 1
			} else {
				if {[catch {eval exec $util_path/db_hotbackup\
				    -${c}vh $testdir -b $backupdir\
				    -d $testdir/data1} res] } {
					error "FAIL: $res"
				}
				# Check that logs and db are in backupdir.
				error_check_good db_backup\
				    [file exists $backupdir/$testfile] 1
				set logfiles [glob $backupdir/log*]
				error_check_bad logs_backed_up [llength $logfiles] 0
			}

			# Add more data and try the "update" flag. 
			puts "\tBackuptest.c: Update existing hot backup."

			if { $txnmode == "bulk" } {
				set txn [$env txn -txn_bulk]
			} else {
				set txn [$env txn]
			}

			populate $db $omethod $txn [expr $nentries * 2] 0 0 
			$txn commit

			if { $option == "checkpoint" } {
				catch {eval exec $util_path/db_hotbackup\
				     -${c}vuh $testdir -b backup -d $testdir/data1} res
				error_check_good c_and_d [is_substr $res $msg2] 1
			} else {
				if {[catch {eval exec $util_path/db_hotbackup\
				    -${c}vuh $testdir -b backup\
				    -d $testdir/data1} res] } {
					error "FAIL: $res"
				}
				# There should be more log files now.
				set newlogfiles [glob $backupdir/log*]
				error_check_bad more_logs $newlogfiles $logfiles
			}

			puts "\tBackuptest.d: Hot backup with full path."
			set fullpath [pwd]
			if { $option == "checkpoint" } {
				catch {eval exec $util_path/db_hotbackup\
				    -${c}vh $testdir -b backup\
				    -d $fullpath/$testdir/data1} res
				error_check_good c_and_d [is_substr $res $msg2] 1
			} else {
				if {[catch {eval exec $util_path/db_hotbackup\
				    -${c}vh $testdir -b backup\
				    -d $fullpath/$testdir/data1} res] } {
					error "FAIL: $res"
				}
			}

			error_check_good db_close [$db close] 0
			error_check_good env_close [$env close] 0
			env_cleanup $testdir
			env_cleanup $backupdir

			puts "\tBackuptest.e: Hot backup with DB_CONFIG."
			backuptest_makeconfig
			set msg3 "use of -l with DB_CONFIG file is deprecated"

			set env [eval {berkdb_env_noerr} $env_flags]
			set db [eval {berkdb_open} -env $env $db_flags]

			if { $txnmode == "bulk" } {
				set txn [$env txn -txn_bulk]
			} else {
				set txn [$env txn]
			}

			populate $db $omethod $txn $nentries 0 0 
			$txn commit

			# With checkpoint, this fails.  Without checkpoint, 
			# just look for the warning message.
			if { $option == "checkpoint" } {
				catch {eval exec $util_path/db_hotbackup\
				    -${c}vh $testdir -b $backupdir -l logs\
				    -d $testdir/data1} res
				error_check_good c_and_d [is_substr $res $msg2] 1
			} else {
				catch {eval exec $util_path/db_hotbackup\
				    -${c}vh $testdir -b $backupdir -l logs\
				    -d $testdir/data1} res
				error_check_good l_and_config \
				    [is_substr $res $msg3] 1

				# Check that logs and db are in backupdir.
				error_check_good db_backup\
				    [file exists $backupdir/$testfile] 1
				set logfiles [glob $backupdir/log*]
				error_check_bad logs_backed_up [llength $logfiles] 0
			}

			if { $txnmode == "bulk" } {
				set txn [$env txn -txn_bulk]
			} else {
				set txn [$env txn]
			}

			populate $db $omethod $txn [expr $nentries * 2] 0 0 
			$txn commit

			puts "\tBackuptest.f:\
			    Hot backup update with DB_CONFIG."
			if { $option == "checkpoint" } {
				catch {eval exec $util_path/db_hotbackup\
				    -${c}vuh $testdir -b backup -l logs\
				    -d $testdir/data1} res
				error_check_good c_and_d [is_substr $res $msg2] 1
			} else {
				catch {eval exec $util_path/db_hotbackup\
				    -${c}vuh $testdir -b backup -l logs\
				    -d $testdir/data1} res
				error_check_good l_and_config \
				    [is_substr $res $msg3] 1

				# There should be more log files now.
				set newlogfiles [glob $backupdir/log*]
				error_check_bad more_logs $newlogfiles $logfiles
			}

			# Repeat with directories already there to test cleaning.
			# We are not doing an update this time.
			puts "\tBackuptest.g:\
			    Hot backup with -D (non-update)."
			if { [catch { eval exec $util_path/db_hotbackup\
			    -${c}vh $testdir -b $backupdir -D } res] } {
				error "FAIL: $res"
			}
			# Check that DB_CONFIG file is in backupdir.
			error_check_good found_db_config\
			    [file exists $backupdir/DB_CONFIG] 1
			# Check that db is in backupdir/data1 and not in backupdir.
			error_check_good found_db\
			    [file exists $backupdir/data1/$testfile] 1
			error_check_bad found_db\
			    [file exists $backupdir/$testfile] 1
			# Check that logs are in backupdir/logs and not in backupdir.
			set logfiles [glob $backupdir/logs/log*]
			error_check_bad found_logs [llength $logfiles] 0
			set logfiles [glob $backupdir/log*]
			error_check_good found_logs [llength $logfiles] 1

			# We are not doing an update this time.
			puts "\tBackuptest.g:\
			    Hot backup with DB_CONFIG (non-update)."
			if { [catch { eval exec $util_path/db_hotbackup\
			    -${c}vh $testdir -b $backupdir } res] } {
				error "FAIL: $res"
			}
			# Check that no DB_CONFIG file is in backupdir.
			error_check_bad found_db_config\
			    [file exists $backupdir/DB_CONFIG] 1
			# Check that db is in backupdir.
			error_check_good found_db\
			    [file exists $backupdir/$testfile] 1
			# Check that logs are in backupdir.
			set logfiles [glob $backupdir/log*]
			error_check_good found_logs [expr [llength $logfiles] > 1] 1

			error_check_good db_close [$db close] 0
			error_check_good env_close [$env close] 0
		}
	}
}

proc backuptest_makeconfig { } {
	source ./include.tcl

	file mkdir $testdir/logs
	file mkdir $testdir/data1

	set cid [open $testdir/DB_CONFIG w]
	puts $cid "set_lg_dir logs"
	puts $cid "set_data_dir data1"
	close $cid
}

