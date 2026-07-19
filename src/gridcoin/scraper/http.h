// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_SCRAPER_HTTP_H
#define GRIDCOIN_SCRAPER_HTTP_H

#include <fs.h>

#include <string>
#include <stdexcept>
#include "sync.h"

//!
//! \brief Progress/status of the local blockchain-data cleanup used by the
//! blockchain reset. (The former snapshot download/SHA256 status was removed
//! with the snapshot feature.)
//!
class SnapshotStatus
{
public:
    void Reset()
    {
        LOCK(cs_lock);

        CleanupBlockchainDataProgress = 0;
        CleanupBlockchainDataComplete = false;
        CleanupBlockchainDataFailed = false;
    }

    int GetCleanupBlockchainDataProgress()
    {
        LOCK(cs_lock);

        return CleanupBlockchainDataProgress;
    }

    bool GetCleanupBlockchainDataComplete()
    {
        LOCK(cs_lock);

        return CleanupBlockchainDataComplete;
    }

    bool GetCleanupBlockchainDataFailed()
    {
        LOCK(cs_lock);

        return CleanupBlockchainDataFailed;
    }

    void SetCleanupBlockchainDataProgress(int CleanupBlockchainDataProgress_in)
    {
        LOCK(cs_lock);

        CleanupBlockchainDataProgress = CleanupBlockchainDataProgress_in;
    }

    void SetCleanupBlockchainDataComplete(bool CleanupBlockchainDataComplete_in)
    {
        LOCK(cs_lock);

        CleanupBlockchainDataComplete = CleanupBlockchainDataComplete_in;
    }

    void SetCleanupBlockchainDataFailed(bool CleanupBlockchainDataFailed_in)
    {
        LOCK(cs_lock);

        CleanupBlockchainDataFailed = CleanupBlockchainDataFailed_in;
    }

private:
    CCriticalSection cs_lock;

    int CleanupBlockchainDataProgress = 0;
    bool CleanupBlockchainDataComplete = false;
    bool CleanupBlockchainDataFailed = false;
};

extern SnapshotStatus DownloadStatus;

//!
//! \brief HTTP exception.
//!
//! Used to signal an unexpected server response.
//!
class HttpException : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

//!
//! \brief Scraper HTTP handler.
//!
//! A rudimentary implementation of an HTTP handler used by the scraper when
//! downloading stat files or fetching ETags.
//!
//! \todo If needed this class can be exposed and refined. Alternatively it
//! can be replaced with curlpp.
//!
class Http
{
public:
    //!
    //! \brief Download file from server.
    //!
    //! Attempts to download \p url to \p destination using optional HTTP
    //! credentials.
    //!
    //! \param url URL to download.
    //! \param destination Destination path, including filename.
    //! \param userpass Optional HTTP credentials.
    //! \throws std::runtime_error if \p destination cannot be opened.
    //!
    void Download(
            const std::string& url,
            const fs::path& destination,
            const std::string& userpass = "");

    //!
    //! \brief Fetch ETag for URL.
    //!
    //! Downloads the headers for \p url and attempts to find the ETag.
    //!
    //! \param url URL to fetch ETag from.
    //! \param userpass Optional HTTP credentials.
    //! \return ETag for \p url.
    //! \throws HttpException on invalid server response.
    //! \throws std::runtime_error if ETag cannot be found.
    //!
    std::string GetEtag(
            const std::string& url,
            const std::string& userpass = "");

    //!
    //! \brief Fetch GitHub release information.
    //!
    //! Downloads the json data from GitHub that contains information about latest releases.
    //!
    //! \throws HttpException on invalid server response.
    //!
    std::string GetLatestVersionResponse();

    //!
    //! \brief Download URL content to a string.
    //!
    //! Fetches \p url content into a string buffer using optional HTTP
    //! credentials. Validates the HTTP response code.
    //!
    //! \param url URL to download.
    //! \param userpass Optional HTTP credentials.
    //! \return The content of the URL as a string.
    //! \throws HttpException on invalid server response.
    //! \throws std::runtime_error on download failure.
    //!
    std::string DownloadToString(
            const std::string& url,
            const std::string& userpass = "");

private:
    //!
    //! \brief RAII wrapper around libcurl's initialization/cleanup functions.
    //!
    struct CurlLifecycle
    {
        CurlLifecycle();
        ~CurlLifecycle();
    };

    //!
    //! \brief Manages the libcurl lifecycle by invoking initialization and
    //! cleanup functions.
    //!
    //! The static lifetime of this object ensures that curl_global_init() is
    //! called at the beginning of the program before it starts other threads
    //! and that curl_global_cleanup() is called when the program ends.
    //!
    static CurlLifecycle curl_lifecycle;

    void EvaluateResponse(int code, const std::string& url);
};

#endif // GRIDCOIN_SCRAPER_HTTP_H
