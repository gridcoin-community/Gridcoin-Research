// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <fs.h>
#include <util.h>
#include <wallet/db.h>

#include <boost/test/unit_test.hpp>

#include <string>

#ifndef WIN32
#include <sys/stat.h>
#endif

BOOST_AUTO_TEST_SUITE(wallet_db_tests)

// Berkeley DB takes narrow paths: the environment's home and log directories,
// and database file names relative to the home. The wallet gives it UTF-8
// (fs::path::string()), and on Windows the bundled copy reads that as UTF-8 only
// because it is built with UNICODE (src/bdb53/CMakeLists.txt). Built without it,
// it read every path in the system code page, so this environment failed to
// open and the database file got another name. The names mix a character that
// code page 1252 has, a CJK pair, and one outside the BMP.
BOOST_AUTO_TEST_CASE(environment_and_database_open_under_non_ascii_names)
{
    const fs::path dir = GetDataDir() / "wallet_db_tests_t\xC3\xABst_\xE6\x97\xA5\xE6\x9C\xAC_\xF0\x9F\x98\x80";
    const std::string file = "w\xC3\xABllet_\xE6\x97\xA5\xE6\x9C\xAC_\xF0\x9F\x98\x80.dat";
    fs::create_directories(dir);
    {
        CDBEnv env;
        BOOST_REQUIRE(env.Open(dir));

        Db db(&env.dbenv, 0);
        BOOST_REQUIRE_EQUAL(db.open(nullptr, file.c_str(), "main", DB_BTREE, DB_CREATE | DB_AUTO_COMMIT, 0), 0);
        char key[] = "key";
        char value[] = "value";
        Dbt k(key, sizeof(key));
        Dbt v(value, sizeof(value));
        BOOST_CHECK_EQUAL(db.put(nullptr, &k, &v, 0), 0);
        BOOST_CHECK_EQUAL(db.close(0), 0);

        env.Close();
    }

    // The files are where the wallet's own fs::path calls look for them.
    BOOST_CHECK(fs::is_regular_file(dir / file));
    BOOST_CHECK(fs::is_directory(dir / "database"));
    BOOST_CHECK(!fs::is_empty(dir / "database"));

    // CDBEnv::Open() never closes its error file (db.log), and Windows does not
    // delete an open file, so the directory may have to stay until the process
    // exits. It is inside the test's own data directory.
    boost::system::error_code ec;
    fs::remove_all(dir, ec);
}

#ifndef WIN32
// The file Berkeley DB writes for CDB::Rewrite (encryption, keypool rewrite) takes the mode of
// the wallet file it replaces before it is moved into place. The in-memory wallet database of
// the test environment cannot run a rewrite on a file, so the step Rewrite calls is checked on
// two files. Production change that would fail this: Rewrite not carrying the wallet file's
// permissions onto the file BDB created.
BOOST_AUTO_TEST_CASE(rewrite_keeps_the_mode_of_the_wallet_file)
{
    const fs::path dir = GetDataDir() / "wallet_db_tests_rewrite_mode";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path wallet = dir / "wallet.dat";
    const fs::path rewritten = dir / "wallet.dat.rewrite";
    for (const fs::path& path : {wallet, rewritten}) {
        fsbridge::ofstream out(path, std::ios::binary);
        out << "bdb";
    }
    BOOST_REQUIRE_EQUAL(::chmod(wallet.c_str(), 0640), 0);
    // What BDB gives a file it creates with the environment's mode.
    BOOST_REQUIRE_EQUAL(::chmod(rewritten.c_str(), 0600), 0);

    BOOST_CHECK(CDB::KeepRewrittenFilePermissions(wallet, rewritten));

    struct stat st;
    BOOST_REQUIRE_EQUAL(::stat(rewritten.c_str(), &st), 0);
    // mode_t is 16 bits on macOS, where the masked mode is an int; compare it unsigned.
    BOOST_CHECK_EQUAL(static_cast<unsigned>(st.st_mode & 07777), 0640U);
    BOOST_REQUIRE_EQUAL(::stat(wallet.c_str(), &st), 0);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(st.st_mode & 07777), 0640U);

    // No wallet file: nothing to keep, and not a failure.
    fs::remove(wallet);
    BOOST_CHECK(CDB::KeepRewrittenFilePermissions(wallet, rewritten));

    fs::remove_all(dir);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
