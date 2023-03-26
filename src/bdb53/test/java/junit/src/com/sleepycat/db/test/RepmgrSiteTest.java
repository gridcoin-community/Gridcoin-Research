/*-
 * See the file LICENSE for redistribution information.
 * 
 * Copyright (c) 2011, 2013 Oracle and/or its affiliates.  All rights reserved.
 * 
 * $Id$
 * 
 * A test program for Java APIs changes due to group membership.[#19878]
 */
package com.sleepycat.db.test;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Test;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import java.io.File;
import com.sleepycat.db.*;

public class RepmgrSiteTest extends EventHandlerAdapter
{
    static String host = "localhost";
    static String homedirName = "";
    static long port = 30100;
    static int maxLoopWait = 30;

    File homedir;
    EnvironmentConfig envConfig;

    @BeforeClass public static void ClassInit() {
        TestUtils.loadConfig(null);
    }

    @AfterClass public static void ClassShutdown() {
    }

    @Before public void PerTestInit()
        throws Exception {
        homedirName = TestUtils.BASETEST_DBDIR + File.separator + "TESTDIR";

        TestUtils.removeDir(homedirName + File.separator + "client2");
        TestUtils.removeDir(homedirName + File.separator + "client1");
        TestUtils.removeDir(homedirName + File.separator + "master");
        TestUtils.removeDir(homedirName);

        homedir = new File(homedirName);
        homedir.mkdir();

        envConfig = initEnvConfig();

        port++;
    }

    @After public void PerTestShutdown()
        throws Exception {
    }

    private EnvironmentConfig initEnvConfig()
    {
        EnvironmentConfig envCon = new EnvironmentConfig();
        envCon.setErrorStream(TestUtils.getErrorStream());
        envCon.setErrorPrefix("RepmgrSiteTest test");
        envCon.setAllowCreate(true);
        envCon.setRunRecovery(true);
        envCon.setThreaded(true);
        envCon.setInitializeLocking(true);
        envCon.setInitializeLogging(true);
        envCon.setInitializeCache(true);
        envCon.setTransactional(true);
        envCon.setInitializeReplication(true);

        return envCon; 
    }

    private boolean waitForStartUpDone(Environment env) throws Exception
    {
        // Ensure the site reach a status of "start-up done",
        // if not, pause and retry status for a max loop times.
        ReplicationStats rs = null;
        int i = 0;

        do {
            java.lang.Thread.sleep(2000);
            rs = env.getReplicationStats(StatsConfig.DEFAULT);
        } while (!rs.getStartupComplete() && i++ < maxLoopWait);

        return (rs.getStartupComplete());
    }

    @Test public void testNoLocalSite() throws Exception
    {
        Environment env = new Environment(homedir, envConfig);
        assertNull(env.getReplicationManagerLocalSite());
        env.close();
    }

    @Test public void testGroupCreator() throws Exception
    {
        // Start master via group creator plus REP_ELECTION.
        ReplicationManagerSiteConfig mConf =
            new ReplicationManagerSiteConfig(host, port);
        mConf.setLocalSite(true);
        mConf.setGroupCreator(true);
        envConfig.addReplicationManagerSite(mConf);

        Environment env = new Environment(homedir, envConfig);
        env.replicationManagerStart(4, ReplicationManagerStartPolicy.REP_ELECTION);

        // Ensure the configuration of local site is correct:
        //    host, port, localsite, group creator.
        ReplicationManagerSite dbsite = env.getReplicationManagerLocalSite();
        assertEquals(host, dbsite.getAddress().host);
        assertEquals(port, dbsite.getAddress().port);
        assertTrue(dbsite.getLocalSite());
        assertTrue(dbsite.getGroupCreator());

        ReplicationStats rs = env.getReplicationStats(StatsConfig.DEFAULT);
        assertEquals(ReplicationStats.REP_MASTER, rs.getStatus());

        dbsite.close();
        env.close();
    }

    @Test public void testLocalSite() throws Exception
    {
        // Start master via REP_MASTER flag without group creator.
        ReplicationManagerSiteConfig mConf =
            new ReplicationManagerSiteConfig(host, port);
        mConf.setLocalSite(true);
        envConfig.addReplicationManagerSite(mConf);

        Environment env = new Environment(homedir, envConfig);
        env.replicationManagerStart(4, ReplicationManagerStartPolicy.REP_MASTER);

        // Ensure the configuration of local site is correct:
        //    host, port, localsite.
        ReplicationManagerSite dbsite = env.getReplicationManagerLocalSite();
        assertEquals(host, dbsite.getAddress().host);
        assertEquals(port, dbsite.getAddress().port);
        assertTrue(dbsite.getLocalSite());
        assertTrue(!dbsite.getGroupCreator());

        dbsite.close();
        env.close();
    }

    @Test public void testRepmgrSiteConfig() throws Exception
    {
        // Start up master.
        File mHomeDir = new File(homedirName + File.separator + "master");
        mHomeDir.mkdir();
        ReplicationManagerSiteConfig mConf =
            new ReplicationManagerSiteConfig(host, port);
        mConf.setLocalSite(true);
        mConf.setGroupCreator(true);
        envConfig.addReplicationManagerSite(mConf);

        Environment mEnv = new Environment(mHomeDir, envConfig);
        // Ensure the configuration of site before repmgr start is correct.
        ReplicationManagerSite mSite =
            mEnv.getReplicationManagerSite(host, port);
        assertEquals(host, mSite.getAddress().host);
        assertEquals(port, mSite.getAddress().port);
        assertTrue(mSite.getLocalSite());
        assertTrue(mSite.getGroupCreator());
        mSite.close();

        mEnv.replicationManagerStart(4, ReplicationManagerStartPolicy.REP_MASTER);
        long mPort = port;

        // Client 1 starts up with master as its bootstrap helper.
        ReplicationManagerSiteConfig hConf =
            new ReplicationManagerSiteConfig(host, mPort);
        hConf.setBootstrapHelper(true);
        assertTrue(hConf.getBootstrapHelper());

        // Start up client 1.
        File cHomeDir1 = new File(homedirName + File.separator + "client1");
        cHomeDir1.mkdir();

        port++;
        ReplicationManagerSiteConfig cConf1 =
            new ReplicationManagerSiteConfig(host, port);
        cConf1.setLocalSite(true);
        long cPort1 = port;

        EnvironmentConfig cEnvConfig1 = initEnvConfig();
        cEnvConfig1.addReplicationManagerSite(cConf1);
        cEnvConfig1.addReplicationManagerSite(hConf);

        Environment cEnv1 = new Environment(cHomeDir1, cEnvConfig1);
        cEnv1.replicationManagerStart(4, ReplicationManagerStartPolicy.REP_CLIENT);
        assertTrue(waitForStartUpDone(cEnv1));

        // Ensure the configuration of client 1 after repmgr start is correct.
        ReplicationManagerSite cSite1 = cEnv1.getReplicationManagerLocalSite();
        assertEquals(host, cSite1.getAddress().host);
        assertEquals(cPort1, cSite1.getAddress().port);
        assertTrue(cSite1.getLocalSite());
        assertTrue(!cSite1.getBootstrapHelper());
        assertTrue(!cSite1.getPeer());
        cSite1.close();

        // Ensure the master in client 1's environment has correct configuration.
        ReplicationManagerSite mInCEnv1 =
            cEnv1.getReplicationManagerSite(host, mPort);
        assertEquals(host, mInCEnv1.getAddress().host);
        assertEquals(mPort, mInCEnv1.getAddress().port);
        assertTrue(!mInCEnv1.getLocalSite());
        assertTrue(mInCEnv1.getBootstrapHelper());
        assertTrue(!mInCEnv1.getPeer());
        mInCEnv1.close();

        cEnv1.close();
        mEnv.close();
    }

    @Test public void testRepmgrSitebyEid() throws Exception
    {
        // Get repmgr site by eid
        ReplicationManagerSiteConfig lConf =
            new ReplicationManagerSiteConfig(host, port);
        lConf.setLocalSite(true);
        lConf.setGroupCreator(true);
        envConfig.addReplicationManagerSite(lConf);

        Environment env = new Environment(homedir, envConfig);
        env.replicationManagerStart(4, ReplicationManagerStartPolicy.REP_MASTER);

        ReplicationManagerSite dbsite = env.getReplicationManagerLocalSite();
        assertTrue(dbsite.getLocalSite());

        // Ensure the get site by eid works well.
        ReplicationManagerSite site =
            env.getReplicationManagerSite(dbsite.getEid());
        assertEquals(host, site.getAddress().host);
        assertEquals(port, site.getAddress().port);

        site.close();
        dbsite.close();
        env.close();
    }

    @Test (expected=IllegalArgumentException.class) 
    public void testRepmgrSiteClose() throws Exception
    {
        // Start up master.
        ReplicationManagerSiteConfig lConf =
            new ReplicationManagerSiteConfig(host, port);
        lConf.setLocalSite(true);
        lConf.setGroupCreator(true);
        envConfig.addReplicationManagerSite(lConf);

        Environment env = new Environment(homedir, envConfig);
        env.replicationManagerStart(4, ReplicationManagerStartPolicy.REP_MASTER);

        ReplicationManagerSite dbsite =
            env.getReplicationManagerSite(host, port);
        dbsite.close();

        // Ensure the DbSite handle cannot work after close.
        dbsite.getLocalSite();

        env.close();
    }

    @Test public void testRepmgrSiteRemove() throws Exception
    {
        // Start up master.
        File mHomeDir = new File(homedirName + File.separator + "master");
        mHomeDir.mkdir();

        ReplicationManagerSiteConfig mConf =
            new ReplicationManagerSiteConfig(host, port);
        mConf.setLocalSite(true);
        mConf.setGroupCreator(true);
        envConfig.addReplicationManagerSite(mConf);

        Environment mEnv = new Environment(mHomeDir, envConfig);
        mEnv.replicationManagerStart(4, ReplicationManagerStartPolicy.REP_MASTER);
        long mPort = port;

        // All clients start up with master as bootstrap helper.
        ReplicationManagerSiteConfig hConf =
            new ReplicationManagerSiteConfig(host, mPort);
        hConf.setBootstrapHelper(true);

        // Start up Client 1.
        File cHomeDir1 = new File(homedirName + File.separator + "client1");
        cHomeDir1.mkdir();

        port++;
        ReplicationManagerSiteConfig cConf1 =
            new ReplicationManagerSiteConfig(host, port);
        cConf1.setLocalSite(true);
        long cPort1 = port;

        EnvironmentConfig cEnvConfig1 = initEnvConfig();
        cEnvConfig1.addReplicationManagerSite(cConf1);
        cEnvConfig1.addReplicationManagerSite(hConf);
        Environment cEnv1 = new Environment(cHomeDir1, cEnvConfig1);
        cEnv1.replicationManagerStart(4, ReplicationManagerStartPolicy.REP_CLIENT);
        assertTrue(waitForStartUpDone(cEnv1));

        // Start up Client 2.
        File cHomeDir2 = new File(homedirName + File.separator + "client2");
        cHomeDir2.mkdir();

        port++;
        ReplicationManagerSiteConfig cConf2 =
            new ReplicationManagerSiteConfig(host, port);
        cConf2.setLocalSite(true);
        long cPort2 = port;

        EnvironmentConfig cEnvConfig2 = initEnvConfig();
        cEnvConfig2.addReplicationManagerSite(cConf2);
        cEnvConfig2.addReplicationManagerSite(hConf);
        Environment cEnv2 = new Environment(cHomeDir2, cEnvConfig2);
        cEnv2.replicationManagerStart(4, ReplicationManagerStartPolicy.REP_CLIENT);
        assertTrue(waitForStartUpDone(cEnv2));

        // Before remove, we have total 3 sites in group.
        int nSites = mEnv.getReplicationNumSites();
        assertEquals(3, nSites);

        // Remove client 2 via client 1.
         cEnv1.getReplicationManagerSite(host, cPort2).remove();

        // After remove, master environment has only 1 remote site.
        ReplicationManagerSiteInfo[] siteLists2 =
            mEnv.getReplicationManagerSiteList();
        assertEquals(1, siteLists2.length);

        cEnv2.close();
        cEnv1.close();
        mEnv.close();
    }
}
