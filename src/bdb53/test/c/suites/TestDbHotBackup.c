/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2011, 2013 Oracle and/or its affiliates.  All rights reserved.
 *
 * $Id$
 */

/*
 * A C Unit test for db_hotbackup APIs [#20451]
 *
 * Different testing environments:
 * without any configuration,
 * have partitioned databases,
 * have multiple add_data_dir configured,
 * have set_lg_dir configured,
 * with queue extent files.
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "CuTest.h"
#include "test_util.h"

struct handlers {
	DB_ENV *dbenvp;
	DB *dbp;
};

typedef enum {
	SIMPLE_ENV = 1,
	PARTITION_DB = 2,
	MULTI_DATA_DIR = 3,
	SET_LOG_DIR = 4,
	QUEUE_DB = 5
} ENV_CONF_T;

static int setup_test(ENV_CONF_T);
static int open_dbp(DB_ENV **, DB **, ENV_CONF_T);
static int store_records(DB *, ENV_CONF_T);
static int cleanup_test(DB_ENV *, DB *);
static int backup_db(CuTest *, DB_ENV *, const char *, u_int32_t, int);
static int backup_env(CuTest *, DB_ENV *, u_int32_t, int);
static int make_dbconfig(ENV_CONF_T);
static int verify_db(ENV_CONF_T);
static int verify_log(ENV_CONF_T);
static int verify_dbconfig(ENV_CONF_T);
static int cmp_files(const char *, const char *);
int backup_open(DB_ENV *, const char *, const char *, void **);
int backup_write(DB_ENV *, u_int32_t, u_int32_t, u_int32_t, u_int8_t *, void *);
int backup_close(DB_ENV *, const char *, void *);

#define BACKUP_DIR "BACKUP"
#define BACKUP_DB "backup.db"
#define LOG_DIR "LOG"
#define NPARTS 3

char *data_dirs[3] = {"DATA1", "DATA2", NULL};

int TestDbHotBackupSuiteSetup(CuSuite *suite) {
	return (0);
}

int TestDbHotBackupSuiteTeardown(CuSuite *suite) {
	return (0);
}

int TestDbHotBackupTestSetup(CuTest *ct) {
	struct handlers *info;

	if ((info = calloc(1, sizeof(*info))) == NULL)
		return (ENOMEM);
	ct->context = info;
	setup_envdir(TEST_ENV, 1);
	setup_envdir(BACKUP_DIR, 1);
	return (0);
}

int TestDbHotBackupTestTeardown(CuTest *ct) {
	struct handlers *info;
	DB_ENV *dbenv;
	DB *dbp;

	if (ct->context == NULL)
		return (EINVAL);
	info = ct->context;
	dbenv = info->dbenvp;
	dbp = info->dbp;
	/* Close all handles and clean the directories. */
	CuAssert(ct, "cleanup_test", cleanup_test(dbenv, dbp) == 0);
	free(info);
	ct->context = NULL;
	return (0);
}

int TestDbHotBackupSimpleEnv(CuTest *ct) {
	DB_ENV *dbenv;
	DB *dbp;
	ENV_CONF_T envconf;
	struct handlers *info;
	char **names;
	int cnt, has_callback;
	u_int32_t flag;

	envconf = SIMPLE_ENV;
	info = ct->context;
	has_callback = 0;
	flag = DB_EXCL;

	/* Step 1: set up test by making relative directories. */
	CuAssert(ct, "setup_test", setup_test(envconf) == 0);

	/* Step 2: open db handle. */
	CuAssert(ct,"open_dbp", open_dbp(&dbenv, &dbp, envconf) == 0);
	info->dbenvp = dbenv;
	info->dbp = dbp;

	/* Step 3: store records into db. */
	CuAssert(ct,"store_records", store_records(dbp, envconf) == 0);
	CuAssert(ct, "DB->sync", dbp->sync(dbp, 0) == 0);

	/* Step 4: backup only the db file without callbacks. */
	CuAssert(ct, "backup_env",
	    backup_db(ct, dbenv, BACKUP_DB, flag, has_callback) == 0);

	/* Step 5: check backup result. */
	/* 5a: dump the db and verify the content is same. */
	CuAssert(ct, "verify_db", verify_db(envconf) == 0);

	/* 5b: no other files are in backupdir. */
	CuAssert(ct, "__os_dirlist",
	    __os_dirlist(NULL, BACKUP_DIR, 0, &names, &cnt) == 0);
	CuAssert(ct, "too many files in backupdir", cnt == 1);

	return (0);
}

int TestDbHotBackupPartitionDB(CuTest *ct) {
	DB_ENV *dbenv;
	DB *dbp;
	ENV_CONF_T envconf;
	struct handlers *info;
	int has_callback;
	u_int32_t flag;

	envconf = PARTITION_DB;
	info = ct->context;
	has_callback = 0;
	flag = DB_BACKUP_CLEAN | DB_CREATE | DB_BACKUP_SINGLE_DIR;

	/* Step 1: set up test by making relative directories. */
	CuAssert(ct, "setup_test", setup_test(envconf) == 0);

	/* Step 2: open db handle. */
	CuAssert(ct,"open_dbp", open_dbp(&dbenv, &dbp, envconf) == 0);
	info->dbenvp = dbenv;
	info->dbp = dbp;

	/* Step 3: store records into db. */
	CuAssert(ct,"store_records", store_records(dbp, envconf) == 0);
	CuAssert(ct, "DB->sync", dbp->sync(dbp, 0) == 0);

	/* Step 4: backup the whole environment into a single directory. */
	CuAssert(ct, "backup_env",
	    backup_env(ct, dbenv, flag, has_callback) == 0);

	/* Step 5: check backup result. */
	/* 5a: dump the db and verify the content is same. */
	CuAssert(ct, "verify_db", verify_db(envconf) == 0);

	/* 5b: verify that creation directory is not in backupdir. */
	CuAssert(ct, "__os_exist", __os_exists(NULL, "BACKUP/DATA1", 0) != 0);

	/* 5c: verify that log files are in backupdir. */
	CuAssert(ct, "verify_log", verify_log(envconf) == 0);

	/* 5d: verify that DB_CONFIG is not in backupdir*/
	CuAssert(ct, "verify_dbconfig", verify_dbconfig(envconf) == 0);

	return (0);
}

int TestDbHotBackupMultiDataDir(CuTest *ct) {
	DB_ENV *dbenv;
	DB *dbp;
	ENV_CONF_T envconf;
	struct handlers *info;
	int has_callback;
	u_int32_t flag;

	envconf = MULTI_DATA_DIR;
	info = ct->context;
	has_callback = 0;
	flag = DB_BACKUP_CLEAN | DB_CREATE | DB_BACKUP_FILES;

	/* Step 1: set up test by making relative directories. */
	CuAssert(ct, "setup_test", setup_test(envconf) == 0);

	/* Step 2: open db handle. */
	CuAssert(ct,"open_dbp", open_dbp(&dbenv, &dbp, envconf) == 0);
	info->dbenvp = dbenv;
	info->dbp = dbp;

	/* Step 3: store records into db. */
	CuAssert(ct,"store_records", store_records(dbp, envconf) == 0);
	CuAssert(ct, "DB->sync", dbp->sync(dbp, 0) == 0);

	/* Step 4: backup the whole environment without callbacks. */
	CuAssert(ct, "backup_env",
	    backup_env(ct, dbenv, flag, has_callback) == 0);	

	/* Step 5: check backup result. */
	/* 5a: dump the db and verify the content is same. */
	CuAssert(ct, "verify_db", verify_db(envconf) == 0);

	/* 5b: verify that data_dirs are in backupdir. */
	CuAssert(ct, "__os_exist", __os_exists(NULL, "BACKUP/DATA1", 0) == 0);
	CuAssert(ct, "__os_exist", __os_exists(NULL, "BACKUP/DATA2", 0) == 0);

	/* 5c: verify that log files are in backupdir. */
	CuAssert(ct, "verify_log", verify_log(envconf) == 0);

	/* 5d: verify that DB_CONFIG is in backupdir. */
	CuAssert(ct, "verify_dbconfig", verify_dbconfig(envconf) == 0);

	return (0);
}

int TestDbHotBackupSetLogDir(CuTest *ct) {
	DB_ENV *dbenv;
	DB *dbp;
	ENV_CONF_T envconf;
	struct handlers *info;
	int has_callback = 1;
	u_int32_t flag;

	envconf = SET_LOG_DIR;
	info = ct->context;
	has_callback = 1;
	flag = DB_BACKUP_CLEAN | DB_CREATE | DB_BACKUP_FILES;

	/* Step 1: set up test by making relative directories. */
	CuAssert(ct, "setup_test", setup_test(envconf) == 0);

	/* Step 2: open db handle. */
	CuAssert(ct,"open_dbp", open_dbp(&dbenv, &dbp, envconf) == 0);
	info->dbenvp = dbenv;
	info->dbp = dbp;

	/* Step 3: store records into db. */
	CuAssert(ct,"store_records", store_records(dbp, envconf) == 0);
	CuAssert(ct, "DB->sync", dbp->sync(dbp, 0) == 0);

	/* Step 4: backup a whole environment with callbacks. */
	CuAssert(ct, "backup_env",
	    backup_env(ct, dbenv, flag, has_callback) == 0);

	/* Step 5: check backup result. */
	/* 5a: dump the db and verify the content is same. */
	CuAssert(ct, "verify_db", verify_db(envconf) == 0);

	/* 5b: verify that log files are in backupdir/log_dir. */
	CuAssert(ct, "verify_log", verify_log(envconf) == 0);

	/* 5c: verify that DB_CONFIG is in backupdir*/
	CuAssert(ct, "verify_dbconfig", verify_dbconfig(envconf) == 0);

	return (0);
}

int TestDbHotBackupQueueDB(CuTest *ct) {
	DB_ENV *dbenv;
	DB *dbp;
	ENV_CONF_T envconf;
	struct handlers *info;
	int has_callback;
	u_int32_t flag;

	envconf = QUEUE_DB;
	info = ct->context;
	has_callback = 0;
	flag = DB_BACKUP_CLEAN | DB_CREATE;

	/* Step 1: set up test by making relative directories. */
	CuAssert(ct, "setup_test", setup_test(envconf) == 0);

	/* Step 2: open db handle. */
	CuAssert(ct,"open_dbp", open_dbp(&dbenv, &dbp, envconf) == 0);
	info->dbenvp = dbenv;
	info->dbp = dbp;

	/* Step 3: store records into db. */
	CuAssert(ct,"store_records", store_records(dbp, envconf) == 0);
	CuAssert(ct, "DB->sync", dbp->sync(dbp, 0) == 0);

	/* Step 4: backup the whole environment without callbacks. */
	CuAssert(ct, "backup_env",
	    backup_env(ct, dbenv, flag, has_callback) == 0);

	/* Step 5: check backup result. */
	/* 5a: dump the db and verify the content is same. */
	CuAssert(ct, "verify_db", verify_db(envconf) == 0);

	/* 5b: verify that log files are in backupdir. */
	CuAssert(ct, "verify_log", verify_log(envconf) == 0);

	/* 5c: vertify that DB_CONFIG is not in backupdir. */
	CuAssert(ct, "verify_dbconfig", verify_dbconfig(envconf) == 0);

	return (0);
}

static int
setup_test(envconf)
	ENV_CONF_T envconf;
{
	char path[1024];
	int i, ret;

	/* Make directories based on config. */
	switch (envconf) {
	case SIMPLE_ENV:
		break;
	case PARTITION_DB:
		snprintf(path, sizeof(path),"%s%c%s",
		    TEST_ENV, PATH_SEPARATOR[0], data_dirs[0]);
		if ((ret = setup_envdir(path, 1)) != 0)
			return (ret);
		break;
	case MULTI_DATA_DIR:
		for (i = 0; i < 2; i++) {
			snprintf(path, sizeof(path),"%s%c%s",
			    TEST_ENV, PATH_SEPARATOR[0], data_dirs[i]);
			if ((ret = setup_envdir(path, 1)) != 0)
				return (ret);
		}
		break;
	case SET_LOG_DIR:
		snprintf(path, sizeof(path),"%s%c%s",
		    TEST_ENV, PATH_SEPARATOR[0], LOG_DIR);
		if ((ret = setup_envdir(path, 1)) != 0)
			return (ret);
		break;
	case QUEUE_DB:
		break;
	default:
		return (EINVAL);
	}

	/* Make DB_CONFIG for PARTITION_DB, MULT_DATA_DIR and SET_LOG_DIR. */
	if(envconf >= 2 && envconf <= 4)
		make_dbconfig(envconf);

	return (0);
}

static int
open_dbp(dbenvp, dbpp, envconf)
	DB_ENV **dbenvp;
	DB **dbpp;
	ENV_CONF_T envconf;
{
	DB_ENV *dbenv;
	DB *dbp;
	DBT key1, key2, keys[2];
	DBTYPE dtype;
	int i, ret, value1, value2;

	dbenv = NULL;
	dbp = NULL;
	dtype = DB_BTREE;
	ret = 0;

	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr, "db_env_create: %s\n", db_strerror(ret));
		return (ret);
	}

	*dbenvp = dbenv;

	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, "TestDbHotBackup");

	/* Configure the environment. */
	switch (envconf) {
	case SIMPLE_ENV:
	case PARTITION_DB:
		break;
	/* Add data directories. */
	case MULTI_DATA_DIR:
		for (i = 0; i < 2; i++) {
			if ((ret = dbenv->add_data_dir(dbenv,
			    data_dirs[i])) != 0) {
				fprintf(stderr, "DB_ENV->add_data_dir: %s\n",
				    db_strerror(ret));
				return (ret);
			}
		}
		break;
	/* Set log directory. */
	case SET_LOG_DIR:
		if ((ret = dbenv->set_lg_dir(dbenv, LOG_DIR)) != 0) {
			fprintf(stderr, "DB_ENV->set_lg_dir: %s\n",
			    db_strerror(ret));
			return (ret);
		}
		break;
	case QUEUE_DB:
		dtype = DB_QUEUE;
		break;
	default:
		return (EINVAL);
	}

	/* Open the environment. */
	if ((ret = dbenv->open(dbenv, TEST_ENV, DB_CREATE | DB_INIT_LOCK |
	    DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN, 0)) != 0) {
		fprintf(stderr, "DB_ENV->open: %s\n", db_strerror(ret));
		return (ret);
	}

	/* Configure the db handle. */
	if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
		fprintf(stderr, "db_create: %s\n", db_strerror(ret));
		return (ret);
	}

	*dbpp = dbp;

	dbp->set_errfile(dbp, stderr);
	dbp->set_errpfx(dbp, "TestDbHotBackup");

	/* Set db creation directory for PARTTION_DB and MULTI_DATA_DRI. */
	if (envconf == PARTITION_DB || envconf == MULTI_DATA_DIR) {
		if ((ret = dbp->set_create_dir(dbp, data_dirs[0])) != 0) {
			fprintf(stderr, "DB_ENV->add_data_dir: %s\n",
			    db_strerror(ret));
			return (ret);
		}
	}

	/* Set partition. */
	if (envconf == PARTITION_DB) {
		value1 = 8;
		key1.data = &value1;
		key1.size = sizeof(value1);
		value2 = 16;
		key2.data = &value2;
		key2.size = sizeof(value2);
		keys[0] = key1;
		keys[1] = key2;
		if ((ret = dbp->set_partition(dbp, NPARTS, keys, NULL)) != 0) {
			dbp->err(dbp, ret, "DB->set_partition");
			return (ret);
		}
	}

	/* Set queue record length and extent size. */
	if (envconf == QUEUE_DB) {
		if ((ret = dbp->set_re_len(dbp, 50)) != 0) {
			dbp->err(dbp, ret, "DB->set_re_len");
			return (ret);
		}
		if ((ret = dbp->set_q_extentsize(dbp, 1)) != 0) {
			dbp->err(dbp, ret, "DB->set_q_extentsize");
			return (ret);
		}
	}
	/* Set flag for Btree. */
	else {
		if ((ret = dbp->set_flags(dbp, DB_DUPSORT)) != 0) {
			dbp->err(dbp, ret, "DB->set_flags");
			return (ret);
		}
	}
	
	if ((ret = dbp->set_pagesize(dbp, 512)) != 0) {
		dbp->err(dbp, ret, "DB->set_pagesize");
		return (ret);
	}

	/*Open the db handle. */
	if ((ret = dbp->open(dbp, NULL, BACKUP_DB,
		    NULL, dtype, DB_CREATE, 0644)) != 0) {
		dbp->err(dbp, ret, "%s: DB->open", BACKUP_DB);
		return (ret);
	}

	return (0);
}

static int
store_records(dbp, envconf)
	DB *dbp;
	ENV_CONF_T envconf;
{
	DBT key, data;
	int i, ret;
	size_t num;
	u_int32_t flag;

	char *buf = "abcdefghijefghijklmnopqrstuvwxyz";
	num = strlen(buf);
	flag = envconf == QUEUE_DB ? DB_APPEND : 0;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	for (i = 0; i < num; i++) {
		key.data = &i;
		key.size = sizeof(i);

		data.data = &buf[i];
		data.size = sizeof(char);

		if ((ret = dbp->put(dbp, NULL, &key, &data, flag)) != 0) {
			dbp->err(dbp, ret, "DB->put");
			return (ret);
		}
	}
	return (ret);
}

static int
cleanup_test(dbenv, dbp)
	DB_ENV *dbenv;
	DB *dbp;
{
	int ret, t_ret;

	ret = 0;;
	if (dbp != NULL && (ret = dbp->close(dbp, 0)) != 0) 
		fprintf(stderr, "DB->close: %s\n", db_strerror(ret));

	if (dbenv != NULL && (t_ret = dbenv->close(dbenv, 0)) != 0) {
		fprintf(stderr, "DB_ENV->close: %s\n", db_strerror(t_ret));
		ret = t_ret;
	}

	teardown_envdir(TEST_ENV);
	teardown_envdir(BACKUP_DIR);

	return (ret);
}

/*
 * backup_env:
 * 	CuTest *ct
 *	DB_ENV *dbenv: the environment to backup
 *	u_int32_t flags: hotbackup flags
 *	int has_callback: 0 if not use callback, 1 otherwise
*/
static int
backup_env(ct, dbenv, flags, has_callback)
	CuTest *ct;
	DB_ENV *dbenv;
	u_int32_t flags;
	int has_callback;
{
	if (has_callback != 0) {
		CuAssert(ct, "DB_ENV->set_backup_callbacks",
		    dbenv->set_backup_callbacks(dbenv, backup_open,
		    backup_write, backup_close) == 0);
	}
	CuAssert(ct, "DB_ENV->backup",
	    dbenv->backup(dbenv, BACKUP_DIR, flags) == 0);
	return (0);
}

/*
 * backup_db:
 * 	CuTest *ct
 * 	DB_ENV *dbenv: the environment to backup
 * 	const char *dname: the name of db file to backup
 * 	u_int32_t flags: hot_backup flags
 * 	int has_callback: 0 if not use callback, 1 otherwise
*/

static int
backup_db(ct, dbenv, dname, flags, has_callback)
	CuTest *ct;
	DB_ENV *dbenv;
	const char *dname;
	u_int32_t flags;
	int has_callback;
{
	if (has_callback != 0) {
		CuAssert(ct, "DB_ENV->set_backup_callbacks",
		    dbenv->set_backup_callbacks(dbenv, backup_open,
		    backup_write, backup_close) == 0);
	}
	CuAssert(ct, "DB_ENV->dbbackup",
	    dbenv->dbbackup(dbenv, dname, BACKUP_DIR, flags) == 0);
	return (0);
}

static int
verify_db(envconf)
	ENV_CONF_T envconf;
{
	char buf1[100], buf2[100], path1[100], path2[100], pfx[10];
	char **names1, **names2;
	int cnt1, cnt2, i, m_cnt, ret, t_cnt1, t_cnt2;

	names1 = names2 = NULL;
	cnt1 = cnt2 = i = m_cnt = ret = t_cnt1 = t_cnt2  = 0;

	/* Get the data directory paths. */
	if (envconf == PARTITION_DB) {
		snprintf(path1, sizeof(path1), "%s%c%s",
		    TEST_ENV, PATH_SEPARATOR[0], data_dirs[0]);
		snprintf(path2, sizeof(path2), "%s", BACKUP_DIR);
	} else if (envconf == MULTI_DATA_DIR) {
		snprintf(path1, sizeof(path1), "%s%c%s",
		    TEST_ENV, PATH_SEPARATOR[0], data_dirs[0]);
		snprintf(path2, sizeof(path2), "%s%c%s",
		    BACKUP_DIR, PATH_SEPARATOR[0], data_dirs[0]);
	} else {
		snprintf(path1, sizeof(path1), "%s", TEST_ENV);
		snprintf(path2, sizeof(path2), "%s", BACKUP_DIR);
	}

	/* Define the prefix of partition db and queue extent files. */
	if (envconf == PARTITION_DB)
		snprintf(pfx, sizeof(pfx), "%s", "__dbp.");
	else if (envconf == QUEUE_DB)
		snprintf(pfx, sizeof(pfx), "%s", "__dbq.");
	else
		pfx[0] = '\0';

	/* Get the lists of db file, partition db files and queue extent. */
	if ((ret = __os_dirlist(NULL, path1, 0, &names1, &cnt1)) != 0)
		return (ret);
	if ((ret = __os_dirlist(NULL, path2, 0, &names2, &cnt2)) != 0)
		return (ret);

	/* Get the numbers of db files. */
	m_cnt = cnt1 > cnt2 ? cnt1 : cnt2;
	t_cnt1 = cnt1;
	t_cnt2 = cnt2;
	for (i = 0; i < m_cnt; i++) {
		if (i < cnt1 &&
		    strncmp(names1[i], BACKUP_DB, strlen(BACKUP_DB)) != 0 &&
		    (strlen(pfx) > 0 ?
		    strncmp(names1[i], pfx, strlen(pfx)) != 0 : 1)) {
			    t_cnt1--;
			    names1[i] = NULL;
		}
		if (i < cnt2 &&
		    strncmp(names2[i], BACKUP_DB, strlen(BACKUP_DB)) != 0 &&
		    (strlen(pfx) > 0 ?
		    strncmp(names2[i], pfx, strlen(pfx)) != 0 : 1)) {
			    t_cnt2--;
			    names2[i] = NULL;
		}		
	}
	if ((ret = t_cnt1 == t_cnt2 ? 0 : EINVAL) != 0)
		return (ret);

	/* Compare each db file. */
	for (i = 0; i < cnt1; i++) {
		if (names1[i] == NULL)
			continue;
		snprintf(buf1, sizeof(buf1), "%s%c%s",
		    path1, PATH_SEPARATOR[0], names1[i]);
		snprintf(buf2, sizeof(buf2), "%s%c%s",
		    path2, PATH_SEPARATOR[0], names1[i]);
		if ((ret = cmp_files(buf1, buf2)) != 0)
			break;
	}

	return (ret);
}

static int
verify_log(envconf)
	ENV_CONF_T envconf;
{
	char buf1[100], buf2[100], lg1[100], lg2[100], pfx[10];
	char **names1, **names2;
	int cnt1, cnt2, i, m_cnt, ret, t_cnt1, t_cnt2;

	cnt1 = cnt2 = i = m_cnt = ret = t_cnt1 = t_cnt2 = 0;

	/* Get the log paths. */
	if (envconf == SET_LOG_DIR) {
		snprintf(lg1, sizeof(lg1),
		    "%s%c%s", TEST_ENV, PATH_SEPARATOR[0], LOG_DIR);
		snprintf(lg2, sizeof(lg2),
		    "%s%c%s", BACKUP_DIR, PATH_SEPARATOR[0], LOG_DIR);
	}
	else {
		snprintf(lg1, sizeof(lg1), "%s", TEST_ENV);
		snprintf(lg2, sizeof(lg2), "%s", BACKUP_DIR);
	}

	/* Define the prefix of log file. */
	snprintf(pfx, sizeof(pfx), "%s", "log.");

	/* Get the lists of log files. */
	if ((ret = __os_dirlist(NULL, lg1, 0, &names1, &cnt1)) != 0)
		return (ret);
	if ((ret = __os_dirlist(NULL, lg2, 0, &names2, &cnt2)) != 0)
		return (ret);

	/* Get the numbers of log files. */
	m_cnt = cnt1 > cnt2 ? cnt1 : cnt2;
	t_cnt1 = cnt1;
	t_cnt2 = cnt2;
	for (i = 0; i < m_cnt; i++) {
		if (i < cnt1 &&
		    strncmp(names1[i], pfx, strlen(pfx)) != 0) {
			    t_cnt1--;
			    names1[i] = NULL;
		}
		if (i < cnt2 &&
		    strncmp(names2[i], pfx, strlen(pfx)) != 0) {
			    t_cnt2--;
			    names2[i] = NULL;
		}		
	}
	if ((ret = t_cnt1 == t_cnt2 ? 0 : EINVAL) != 0)
		return (ret);

	/* Compare each log file. */
	for (i = 0; i < cnt1; i++) {
		if (names1[i] == NULL)
			continue;
		snprintf(buf1, sizeof(buf1), "%s%c%s",
		    lg1, PATH_SEPARATOR[0], names1[i]);
		snprintf(buf2, sizeof(buf2), "%s%c%s",
		    lg2, PATH_SEPARATOR[0], names1[i]);
		if ((ret = cmp_files(buf1, buf2)) != 0)
			break;
	}

	return (ret);
}

static int
verify_dbconfig(envconf)
	ENV_CONF_T envconf;
{
	char *path1, *path2;
	int ret;

	path1 = path2 = NULL;
	ret = 0;

	if ((ret = __os_calloc(NULL, 1024, 1, &path1)) != 0)
		goto err;
	if ((ret = __os_calloc(NULL, 1024, 1, &path2)) != 0)
		goto err;

	switch(envconf) {
	/* DB_CONFIG is not in backupdir for this test cases. */
	case SIMPLE_ENV:
	case PARTITION_DB:
	case QUEUE_DB:
		if((ret = __os_exists(NULL, "BACKUP/DB_CONFIG", 0)) != 0)
			return (0);
		break;
	/* DB_CONFIG is in backupdir for MULTI_DATA_DIR and SET_LOG_DIR. */
	case MULTI_DATA_DIR:
	case SET_LOG_DIR:
		snprintf(path1, 1024, "%s%c%s",
		    TEST_ENV, PATH_SEPARATOR[0], "DB_CONFIG");
		snprintf(path2, 1024, "%s%c%s",
		    BACKUP_DIR, PATH_SEPARATOR[0], "DB_CONFIG");
		if ((ret = cmp_files(path1, path2)) != 0)
			goto err;
		break;
	default:
		return (EINVAL);
	}

err:
	if (path1 != NULL)
		__os_free(NULL, path1);
	if (path2 != NULL)
		__os_free(NULL, path2);
	return (ret);
}

int backup_open(dbenv, dbname, target, handle)
	DB_ENV *dbenv;
	const char *dbname;
	const char *target;
	void **handle;
{
	DB_FH *fp;
	const char *t_target;
	char str[1024];
	u_int32_t flags;
	int ret;

	fp = NULL;
	flags = DB_OSO_CREATE;
	ret = 0;

	if (target == NULL)
		t_target = BACKUP_DIR;
	else
		t_target = target;
	sprintf(str, "%s%s%s", t_target, "/", dbname);

	if ((ret = __os_open(NULL, str, 0, flags, DB_MODE_600, &fp)) != 0)
		return (ret);

	*handle = fp;

	return (ret);
}

int backup_write(dbenv, gigs, offset, size, buf, handle)
	DB_ENV *dbenv;
	u_int32_t gigs, offset, size;
	u_int8_t *buf;
	void *handle;
{
	DB_FH *fp;
	size_t nw;
	int ret;

	ret = 0;
	fp = (DB_FH *)handle;

	if (size <= 0)
		return (EINVAL);

	if ((ret = __os_write(NULL, fp, buf, size, &nw)) != 0)
		return (ret);
	if (nw != size)
		ret = EIO;

	return (ret);
}

int backup_close(dbenv, dbname, handle)
	DB_ENV *dbenv;
	const char *dbname;
	void *handle;
{
	DB_FH *fp;
	int ret;

	fp = (DB_FH *)handle;
	ret = 0;

	ret = __os_closehandle(NULL, fp);

	return (ret);

}

static int
make_dbconfig(envconf)
	ENV_CONF_T envconf;
{
	const char *path = "TESTDIR/DB_CONFIG";
	char str[1024];
	FILE *fp;

	if (envconf >= PARTITION_DB && envconf <= SET_LOG_DIR)
		fp = fopen(path, "w");
	else
		return (0);

	switch(envconf) {
	case PARTITION_DB:
	case MULTI_DATA_DIR:
		snprintf(str, 1024, "%s", "set_data_dir DATA1");
		break;
	case SET_LOG_DIR:
		snprintf(str, 1024, "%s", "set_lg_dir LOG");
		break;
	default:
		return (EINVAL);
	}

	fputs(str, fp);
	fclose(fp);

	return (0);
}

static int
cmp_files(name1, name2)
	const char *name1;
	const char *name2;
{
	DB_FH *fhp1, *fhp2;
	void *buf1, *buf2;
	size_t nr1, nr2;
	int ret, t_ret;

	fhp1 = fhp2 = NULL;
	buf1 = buf2 = NULL;
	nr1 = nr2 = 0;
	ret = t_ret = 0;

	/* Allocate memory for buffers. */
	if ((ret = __os_calloc(NULL, MEGABYTE, 1, &buf1)) != 0)
		goto err;
	if ((ret = __os_calloc(NULL, MEGABYTE, 1, &buf2)) != 0)
		goto err;

	/* Open the input files. */
	if ( (ret = __os_open(NULL, name1, 0, DB_OSO_RDONLY, 0, &fhp1)) != 0 ||
	    (t_ret = __os_open(NULL, name2, 0, DB_OSO_RDONLY, 0, &fhp2)) != 0) {
		    if (ret == 0)
			    ret = t_ret;
		    goto err;
	}

	/* Read and compare the file content. */
	while ((ret = __os_read(NULL, fhp1, buf1, MEGABYTE, &nr1)) == 0 &&
	    nr1 > 0 && (ret = __os_read(NULL, fhp2,
	    buf2, MEGABYTE, &nr2)) == 0 && nr2 > 0) {
		    if (nr1 != nr2) {
			    ret = EINVAL;
			    break;
		    }
		    if ((ret = memcmp(buf1, buf2, nr1)) != 0)
			    break;
	}
	if(ret == 0 && nr1 > 0 && nr2 > 0 && nr1 != nr2)
		ret = EINVAL;

err:
	if (buf1 != NULL)
		__os_free(NULL, buf1);
	if (buf2 != NULL)
		__os_free(NULL, buf2);
	if (fhp1 != NULL && (t_ret = __os_closehandle(NULL, fhp1)) != 0)
		ret = t_ret;
	if (fhp2 != NULL && (t_ret = __os_closehandle(NULL, fhp2)) != 0)
		ret = t_ret;
	return (ret);
}
