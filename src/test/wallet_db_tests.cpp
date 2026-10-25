// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <fs.h>
#include <util.h>
#include <wallet/db.h>

#include <boost/test/unit_test.hpp>

#include <string>

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

BOOST_AUTO_TEST_SUITE_END()
