// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/scraper/http.h"
#include <fs.h>
#include <util/system.h>

#include <boost/test/unit_test.hpp>

#include <curl/curl.h>

#include <stdexcept>
#include <string>

namespace {

//! Nothing has to exist at this URL: Http refuses its scheme before curl
//! opens anything.
const std::string FILE_URL = "file:///http_tests_nonexistent";

//! Http puts curl's error text in the exception it throws.
bool IsUnsupportedProtocol(const std::runtime_error& e)
{
    return std::string(e.what()).find(curl_easy_strerror(CURLE_UNSUPPORTED_PROTOCOL)) != std::string::npos;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(http_tests)

// Gridcoin fetches only over HTTP and HTTPS, so Http refuses any other URL.

BOOST_AUTO_TEST_CASE(it_refuses_a_file_url_for_a_download)
{
    const fs::path destination = GetDataDir() / "http_tests_download";

    Http http;

    BOOST_CHECK_EXCEPTION(http.Download(FILE_URL, destination, ""), std::runtime_error, IsUnsupportedProtocol);

    boost::system::error_code ec;
    fs::remove(destination, ec);
}

BOOST_AUTO_TEST_CASE(it_refuses_a_file_url_for_a_download_to_a_string)
{
    Http http;

    BOOST_CHECK_EXCEPTION(http.DownloadToString(FILE_URL), std::runtime_error, IsUnsupportedProtocol);
}

BOOST_AUTO_TEST_CASE(it_refuses_a_file_url_for_an_etag)
{
    Http http;

    BOOST_CHECK_EXCEPTION(http.GetEtag(FILE_URL, ""), std::runtime_error, IsUnsupportedProtocol);
}

BOOST_AUTO_TEST_SUITE_END()
