/*-
 * Automatically built by dist/s_java_stat.
 * Only the javadoc comments can be edited.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002, 2013 Oracle and/or its affiliates.  All rights reserved.
 */

package com.sleepycat.db;

/**
Replication Manager statistics for a database environment.
*/
public class ReplicationManagerStats {
    // no public constructor
    /* package */ ReplicationManagerStats() {}

    private long st_perm_failed;
    /**
    The number of times a PERM message originating at this site did
    not receive sufficient acknowledgement from clients, according to the
    configured acknowledgement policy and acknowledgement timeout.
    */
    public long getPermFailed() {
        return st_perm_failed;
    }

    private long st_msgs_queued;
    /**
    The number of messages queued due to a network delay.
    */
    public long getMsgsQueued() {
        return st_msgs_queued;
    }

    private long st_msgs_dropped;
    /**
    The number of messages discarded due to queue length overflows.
    */
    public long getMsgsDropped() {
        return st_msgs_dropped;
    }

    private long st_connection_drop;
    /**
    The number of existing connections that have been dropped since the
    statistics were last reset.
    */
    public long getConnectionDrop() {
        return st_connection_drop;
    }

    private long st_connect_fail;
    /**
    The number of times new connection attempts have failed.
    */
    public long getConnectFail() {
        return st_connect_fail;
    }

    private long st_elect_threads;
    /** 
	Number of currently active election threads.
    */
    public long getElectThreads() {
        return st_elect_threads;
    }

    private long st_max_elect_threads;
    /** 
	Election threads for which space is reserved.
    */
    public long getMaxElectThreads() {
        return st_max_elect_threads;
    }

    /**
    For convenience, the ReplicationManagerStats class has a toString method
    that lists all the data fields.
    */
    public String toString() {
        return "ReplicationManagerStats:"
            + "\n  st_perm_failed=" + st_perm_failed
            + "\n  st_msgs_queued=" + st_msgs_queued
            + "\n  st_msgs_dropped=" + st_msgs_dropped
            + "\n  st_connection_drop=" + st_connection_drop
            + "\n  st_connect_fail=" + st_connect_fail
            + "\n  st_elect_threads=" + st_elect_threads
            + "\n  st_max_elect_threads=" + st_max_elect_threads
            ;
    }
}
