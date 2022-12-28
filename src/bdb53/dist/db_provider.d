/*
 * DO NOT EDIT: automatically built by dist/s_include.
 * Oracle Berkeley DB DTrace Provider
 */
#include "dbdefs.d"

provider bdb {
/*
 * 
 * dist/events.in - This description of Oracle Berkeley DB's internal
 * events hierarchy is processed by 'dist/s_include' to generate the
 * platform-independant file dist/db_provider.d. The 'configure' step on the
 * target operating system generate platform-specific include file.
 * 
 * 	s_include	->	dist/db_provider.d
 * 	configure	->	<build_directory>/db_provider.h
 * 
 * There are two kinds of entries in events.in, describing either an event class
 * or an individual event. The entries starting in the first column are class
 * names, consisting of a single word. The class's individual events follow,
 * described as if they were an ANSI C function signature;
 * 
 * Events are listed grouped by their relation to one another, rather than
 * alphabetically. For instance allocation and free events are adjacent.
 * New, unrelated events are placed at the end of their event class.
 * 
 * Copyright (c) 2011, 2013 Oracle and/or its affiliates.  All rights reserved.
 * 
 */

/* The alloc class covers the allocation of "on disk" database pages. */
    /*
     * An attempt to allocate a database page of type 'type' for database 'db'
     * returned 'ret'. If the allocation succeeded then ret is 0, pgno is the
     * location of the new page, and pg is the address of the new page.
     * Details of the page can be extracted from the pg pointer.
     */
    probe alloc__new(char *file, char *db, unsigned pgno, unsigned type,
    	struct _db_page *pg, int ret);
    /*
     * An attempt to free the page 'pgno' of 'db' returned 'ret'.
     * When successful the page is returned to the free list
     * or the file is truncated.
     */
    probe alloc__free(char *file, char *db, unsigned pgno, unsigned ret);
    /*
     * A btree split of pgno in db is being attempted. The parent page number
     * and the level in the btree are also provided.
     */
    probe alloc__btree_split(char *file, char *db, unsigned pgno, unsigned parent,
    	unsigned level);

/*
 * These DB API calls provide the name of the file and database being accessed.
 * In-memory databases will have a NULL (0) file name address. The db name will
 * be null unless subdatabases are in use.
 */
    /*
     * The database or file name was opened. The 20 byte unique fileid can be
     * used to keep track of databases as they are created and destroyed.
     */
    probe db__open(char *file, char *db, unsigned flags, uint8_t *fileid);
    /* The database or file name was closed. */
    probe db__close(char *file, char *db, unsigned flags, uint8_t *fileid);
    /* An attempt is being made to open a cursor on the database or file. */
    probe db__cursor(char *file, char *db, unsigned txnid, unsigned flags,
    	uint8_t *fileid);
    /* An attempt is being made to get data from a db. */
    probe db__get(char *file, char *db, unsigned txnid, DBT *key, DBT *data,
    	unsigned flags);
    /* An attempt is being made to put data to a db. */
    probe db__put(char *file, char *db, unsigned txnid, DBT *key, DBT *data,
    	unsigned flags);
    /* An attempt is being made to delete data from a db. */
    probe db__del(char *file, char *db, unsigned txnid, DBT *key, unsigned flags);

/*
 * The lock class monitors the transactional consistency locks: page, record,
 * and database. It also monitors the non-transactional file handle locks.
 */
    /*
     * The thread is about to suspend itself because another locker already has
     * a conflicting lock on object 'lock'.  The lock DBT's data points to
     * a __db_ilock structure, except for the atypical program which uses
     * application specific locking.
     */
    probe lock__suspend(DBT *lock, db_lockmode_t lock_mode);
    /* The thread is awakening from a suspend. */
    probe lock__resume(DBT *lock, db_lockmode_t lock_mode);
    /* The lock is being freed. */
    probe lock__put(struct __sh_dbt *lock, unsigned flags);
    /*
     * The lock would have been freed except that its refcount was greater
     * than 1.
     */
    probe lock__put_reduce_count(struct __sh_dbt *lock, unsigned flags);

    /*
     * These lock counters are included by --enable-perfmon-statistics.
     */

    /*
     * The locker_id's lock request in lock_obj is about to be aborted in
     * order to resolve a deadlock. The lock region's st_ndeadlocks has
     * been incremented.
     */
    probe lock__deadlock(unsigned st_ndeadlocks, unsigned locker_id,
        struct __sh_dbt *lock_obj);
    /*
     * A DB_LOCK_NOWAIT lock request by locker_id would have had to wait.
     * The lock regions's st_lock_nowait has been incremented and
     * the request returns DB_LOCK_NOTGRANTED.
     */
    probe lock__nowait_notgranted(unsigned count, DBT *lock, unsigned locker_id);
    probe lock__request(unsigned request_count, DBT *lock, unsigned locker_id);
    probe lock__upgrade(unsigned upgrade_count, DBT *lock, unsigned locker_id);
    /*
     * A lock is being stolen from one partition for another one
     * The 'from' lock partition's st_locksteals has been incremented.
     */
    probe lock__steal(unsigned st_locksteals, unsigned from, unsigned to);
    /*
     * A lock object is being stolen from one partition for another one.
     * The 'from' lock partition's st_objectsteals has been incremented.
     */
    probe lock__object_steal(unsigned st_objectsteals, unsigned from, unsigned to);
    /* A lock wait expired due to the lock request timeout. */
    probe lock__locktimeout(unsigned st_nlocktimeouts, const DBT *lock);
    /* A lock wait expired due to the transaction's timeout. */
    probe lock__txntimeout(unsigned st_ntxntimeouts, const DBT *lock);
    /*
     * The allocation or deallocation of the locker id changed the number
     * of active locker identifiers.
     */
    probe lock__nlockers(unsigned active, unsigned locker_id);
    /*
     * The allocation of the locker id set a new maximum
     * number of active locker identifiers.
     */
    probe lock__maxnlockers(unsigned new_max_active, unsigned locker_id);

/* Log - Transaction log  */
    probe log__read(unsigned read_count, unsigned logfile);

/*
 * The mpool class monitors the allocation and management of memory,
 * including the cache.
 */
    /* Read a page from file into buf. */
    probe mpool__read(char *file, unsigned pgno, struct __bh *buf);
    /* Write a page from buf to file. */
    probe mpool__write(char *file, unsigned pgno, struct __bh *buf);
    /*
     * This is an attempt to allocate size bytes from region_id.
     * The reg_type is one of the reg_type_t enum values.
     */
    probe mpool__env_alloc(unsigned size, unsigned region_id, unsigned reg_type);
    /* The page is about to be removed from the cache. */
    probe mpool__evict(char *file, unsigned pgno, struct __bh *buf);
    /*
     * The memory allocator has incremented wrap_count after searching through
     * the entire region without being able to fulfill the request for
     * alloc_len bytes. As wrap_count increases the library makes more effort
     * to allocate space.
     */
    probe mpool__alloc_wrap(unsigned alloc_len, int region_id, int wrap_count,
    	int put_counter);

    /*
     * These mpool counters are included by --enable-perfmon-statistics.
     */

    /* The eviction of a clean page from a cache incremented st_ro_evict. */
    probe mpool__clean_eviction(unsigned st_ro_evict, unsigned region_id);
    /*
     * The eviction of a dirty page from a cache incremented st_rw_evict.
     * The page has already been written out.
     */
    probe mpool__dirty_eviction(unsigned st_rw_evict, unsigned region_id);
    /* An attempt to allocate memory from region_id failed. */
    probe mpool__fail(unsigned failure_count, unsigned alloc_len, unsigned region_id);
    probe mpool__free(unsigned freed_count, unsigned alloc_len, unsigned region_id);
    probe mpool__longest_search(unsigned count, unsigned alloc_len, unsigned region_id);
    probe mpool__free_frozen(unsigned count, char *file, unsigned pgno);
    probe mpool__freeze(unsigned hash_frozen, unsigned pgno);
    /* A search for pgno of file incremented st_hash_searches. */
    probe mpool__hash_search(unsigned st_hash_searches, char *file, unsigned pgno);
    /*
     * A search for pgno of file increased st_hash_examined by the number
     * of hash buckets examined.
     */
    probe mpool__hash_examined(unsigned st_hash_examined, char *file, unsigned pgno);
    /* A search for pgno of file set a new maximum st_hash_longest value. */
    probe mpool__hash_longest(unsigned st_hash_longest, char *file, unsigned pgno);
    /*
     * A file's st_map count was incremented after a page was mapped into
     * memory.  The mapping might have caused disk I/O.
     */
    probe mpool__map(unsigned st_map, char *file, unsigned pgno);
    /*
     * The hit count was incremented because pgno from file was found
     * in the cache.
     */
    probe mpool__hit(unsigned st_cache_hit, char *file, unsigned pgno);
    /*
     * The miss count was incremented because pgno from file was
     * not already present in the cache.
     */
    probe mpool__miss(unsigned st_cache_miss, char *file, unsigned pgno);
    /*
     * The st_page_create field was incremented because
     * the pgno of file was created in the cache.
     */
    probe mpool__page_create(unsigned st_page_create, char *file, unsigned pgno);
    /*
     * The st_page_in field was incremented because
     * the pgno from file was read into the cache.
     */
    probe mpool__page_in(unsigned st_page_in, char *file, unsigned pgno);
    /*
     * The st_page_out field was incremented because
     * the pgno from file was written out.
     */
    probe mpool__page_out(unsigned st_page_out, char *file, unsigned pgno);
    probe mpool__thaw(unsigned count, char *file, unsigned pgno);
    probe mpool__alloc(unsigned success_count, unsigned len, unsigned region_id);
    probe mpool__nallocs(unsigned st_alloc, unsigned alloc_len);
    probe mpool__alloc_buckets(unsigned st_alloc_buckets, unsigned region_id);
    probe mpool__alloc_max_buckets(unsigned max, unsigned region_id);
    probe mpool__alloc_max_pages(unsigned max, unsigned region_id);
    probe mpool__alloc_pages(unsigned count, unsigned region_id);

    probe mpool__backup_spins(unsigned spins, char *file, unsigned pgno);

    /*
     * The aggressiveness of a buffer cache allocation increases as more
     * involved methods are needed in order to free up the requested space
     * in the cache with the indicated region_id.
     * aggressive: the agressiveness of an allocation request was increased.
     * max_aggressive: the agressiveness of an allocation request was increased
     * to a new maximum.
     */
    probe mpool__aggressive(unsigned st_alloc_aggressive, unsigned region_id);
    probe mpool__max_aggressive(unsigned st_alloc_max_aggr, unsigned region_id);

/*
 * The mutex category monitors includes shared latches.  The alloc_id value
 * is one of the MTX_XXX definitions from dbinc/mutex.h
 */
    /*
     * This thread is about to suspend itself because a thread has the
     * mutex or shared latch locked in a mode which conflicts with the
     * this request.
     */
    probe mutex__suspend(unsigned mutex, unsigned excl, unsigned alloc_id,
    	struct __db_mutex_t *mutexp);
    /*
     * The thread is returning from a suspend and will attempt to obtain
     * the mutex or shared latch again. It might need to suspend again.
     */
    probe mutex__resume(unsigned mutex, unsigned excl, unsigned alloc_id,
    	struct __db_mutex_t *mutexp);

    /*
     * These mutex counters are included by --enable-perfmon-statistics.
     */

    /*
     * Increment the count of times that the mutex was free when trying
     * to lock it.
     */
    probe mutex__set_nowait(unsigned mutex_set_nowait, unsigned mutex);
    /*
     * Increment the count of times that the mutex was busy when trying
     * to lock it.
     */
    probe mutex__set_wait(unsigned mutex_set_wait, unsigned mutex);
    /*
     * Increment the count of times that the shared latch was free
     * when trying to get a shared lock on it.
     */
    probe mutex__set_rd_nowait(unsigned mutex_set_rd_nowait, unsigned mutex);
    /*
     * Increment the count of times that the shared latch was already
     * exclusively latched when trying to get a shared lock on it.
     */
    probe mutex__set_rd_wait(unsigned mutex_set_rd_wait, unsigned mutex);
    /*
     * Increment the count of times that a hybrid mutex had to block
     * on its condition variable.  n a busy system this might happen
     * several times before the corresponding hybrid_wakeup.
     */
    probe mutex__hybrid_wait(unsigned hybrid_wait, unsigned mutex);
    /*
     * Increment the count of times that a hybrid mutex finished
     * one or more waits for its condition variable.
     */
    probe mutex__hybrid_wakeup(unsigned hybrid_wakeup, unsigned mutex);

/*
 * The race events are triggered when the interactions between two threads
 * causes a rarely executed code path to be taken. They are used primarily
 * to help test and diagnose race conditions, though if they are being
 * triggered too frequently it could result in performance degradation.
 * They are intended for use by Berkeley DB engineers.
 */
    /* A Btree search needs to wait for a page lock and retry from the top. */
    probe race__bam_search(char *file, char *db, int errcode, struct _db_page *pg,
    	struct _db_page *parent, unsigned flags);
    /*
     * A record was not found searching an off-page duplicate tree.
     * Retry the search.
     */
    probe race__dbc_get(char *file, char *db, int errcode, unsigned flags, DBT *key);
    /*
     * The thread could not immediately open and lock the file handle
     * without waiting. The thread will close, wait, and retry.
     */
    probe race__fop_file_setup(char *file, int errcode, unsigned flags);
    /* A get next or previous in a recno db had to retry. */
    probe race__ramc_get(char *file, char *db, struct _db_page *pg, unsigned flags);

/* The statistics counters for replication are for internal use. */
    probe rep__log_queued(unsigned count, DB_LSN *lsn);
    probe rep__pg_duplicated(unsigned eid, unsigned pgno, unsigned file, unsigned count);
    probe rep__pg_record(unsigned count, unsigned eid);
    probe rep__pg_request(unsigned count, unsigned eid);
    probe rep__election_won(unsigned count, unsigned generation);
    probe rep__election(unsigned count, unsigned generation);
    probe rep__log_request(unsigned count, unsigned eid);
    probe rep__master_change(unsigned count, unsigned eid);

/* The txn category covers the basic transaction operations. */
    /* A transaction was successfully begun. */
    probe txn__begin(unsigned txnid, unsigned flags);
    /* A transaction is starting to commit. */
    probe txn__commit(unsigned txnid, unsigned flags);
    /*
     * The transaction is starting to prepare, flushing the log
     * so that a future commit can be guaranteed to succeed.
     * The global identifier field is 128 bytes long.
     */
    probe txn__prepare(unsigned txnid, uint8_t *gid);
    /* The transaction is about to abort. */
    probe txn__abort(unsigned txnid);

    /*
     * These txn counters are included by --enable-perfmon-statistics.
     */

    /* Beginning the transaction incremented st_nbegins. */
    probe txn__nbegins(unsigned st_nbegins, unsigned txnid);
    /* Aborting the transaction incremented st_naborts. */
    probe txn__naborts(unsigned st_naborts, unsigned txnid);
    /* Committing the transaction incremented st_ncommits. */
    probe txn__ncommits(unsigned st_ncommits, unsigned txnid);
    /*
     * Beginning or ending the transaction updated the number of active
     * transactions.
     */
    probe txn__nactive(unsigned st_nactive, unsigned txnid);
    /*
     * The creation of the transaction set a new maximum number
     * of active transactions.
     */
    probe txn__maxnactive(unsigned st_maxnactive, unsigned txnid);
    probe txn__nsnapshot(unsigned st_nsnapshot, unsigned txnid);
    probe txn__maxnsnapshot(unsigned st_maxnsnapshot, unsigned txnid);
};

#pragma D attributes Evolving/Evolving/Common provider bdb provider
#pragma D attributes Private/Private/Common provider bdb module
#pragma D attributes Private/Private/Common provider bdb function
#pragma D attributes Evolving/Evolving/Common provider bdb name
#pragma D attributes Evolving/Evolving/Common provider bdb args

