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
//! \brief Struct for snapshot download progress updates.
//!
class SnapshotStatus
{
public:
    void Reset()
    {
        LOCK(cs_lock);

        SnapshotDownloadComplete = false;
        SnapshotDownloadFailed = false;
        SnapshotDownloadSpeed = 0;
        SnapshotDownloadProgress = 0;
        SnapshotDownloadSize = 0;
        SnapshotDownloadAmount = 0;
        SHA256SUMProgress = 0;
        SHA256SUMComplete = false;
        SHA256SUMFailed = false;
        CleanupBlockchainDataProgress = 0;
        CleanupBlockchainDataComplete = false;
        CleanupBlockchainDataFailed = false;
    }

    bool GetSnapshotDownloadComplete()
    {
        LOCK(cs_lock);

        return SnapshotDownloadComplete;
    }

    bool GetSnapshotDownloadFailed()
    {
        LOCK(cs_lock);

        return SnapshotDownloadFailed;
    }

    int64_t GetSnapshotDownloadSpeed()
    {
        LOCK(cs_lock);

        return SnapshotDownloadSpeed;
    }

    int GetSnapshotDownloadProgress()
    {
        LOCK(cs_lock);

        return SnapshotDownloadProgress;
    }

    long long GetSnapshotDownloadSize()
    {
        LOCK(cs_lock);

        return SnapshotDownloadSize;
    }

    long long GetSnapshotDownloadAmount()
    {
        LOCK(cs_lock);

        return SnapshotDownloadAmount;
    }

    int GetSHA256SUMProgress()
    {
        LOCK(cs_lock);

        return SHA256SUMProgress;
    }

    bool GetSHA256SUMComplete()
    {
        LOCK(cs_lock);

        return SHA256SUMComplete;
    }

    bool GetSHA256SUMFailed()
    {
        LOCK(cs_lock);

        return SHA256SUMFailed;
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

    void SetSnapshotDownloadComplete(bool SnapshotDownloadComplete_in)
    {
        LOCK(cs_lock);

        SnapshotDownloadComplete = SnapshotDownloadComplete_in;
    }

    void SetSnapshotDownloadFailed(bool SnapshotDownloadFailed_in)
    {
        LOCK(cs_lock);

        SnapshotDownloadFailed = SnapshotDownloadFailed_in;
    }

    void SetSnapshotDownloadSpeed(int64_t SnapshotDownloadSpeed_in)
    {
        LOCK(cs_lock);

        SnapshotDownloadSpeed = SnapshotDownloadSpeed_in;
    }

    void SetSnapshotDownloadProgress(int SnapshotDownloadProgress_in)
    {
        LOCK(cs_lock);

        SnapshotDownloadProgress = SnapshotDownloadProgress_in;
    }

    void SetSnapshotDownloadSize(long long SnapshotDownloadSize_in)
    {
        LOCK(cs_lock);

        SnapshotDownloadSize = SnapshotDownloadSize_in;
    }

    void SetSnapshotDownloadAmount(long long SnapshotDownloadAmount_in)
    {
        LOCK(cs_lock);

        SnapshotDownloadAmount = SnapshotDownloadAmount_in;
    }

    void SetSHA256SUMProgress(int SHA256SumProgress_in)
    {
        LOCK(cs_lock);

        SHA256SUMProgress = SHA256SumProgress_in;
    }

    void SetSHA256SUMComplete(bool SHA256SUMComplete_in)
    {
        LOCK(cs_lock);

        SHA256SUMComplete = SHA256SUMComplete_in;
    }

    void SetSHA256SUMFailed(bool SHA256SUMFailed_in)
    {
        LOCK(cs_lock);

        SHA256SUMFailed = SHA256SUMFailed_in;
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

    bool SnapshotDownloadComplete = false;
    bool SnapshotDownloadFailed = false;
    int64_t SnapshotDownloadSpeed = 0;
    int SnapshotDownloadProgress = 0;
    long long SnapshotDownloadSize = 0;
    long long SnapshotDownloadAmount = 0;
    int SHA256SUMProgress = 0;
    bool SHA256SUMComplete = false;
    bool SHA256SUMFailed = false;
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
    //! \brief Download Snapshot with progress updates.
    //!
    //! Downloads the snapshot from the latest snapshot.zip hosted on download.gridcoin.us.
    //!
    //! \throws HttpException on invalid server response.
    //!
    void DownloadSnapshot();
    //!
    //! \brief Fetch the sha256sum from snapshot server.
    //!
    //! Fetches the snapshot sha256 from snapshot server.
    //!
    //! \return the sha256sum from file
    //!
    //! \throws HttpException on invalid server response.
    //!
    std::string GetSnapshotSHA256();

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
