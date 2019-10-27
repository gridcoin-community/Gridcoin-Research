#pragma once

#include <fs.h>

#include <string>
#include <stdexcept>

//!
//! \brief Struct for snapshot download progress updates.
//!
struct struct_SnapshotStatus{
    bool SnapshotDownloadComplete = false;
    bool SnapshotDownloadFailed = false;
    int64_t SnapshotDownloadSpeed;
    int SnapshotDownloadProgress;
    long long SnapshotDownloadSize = 0;
    long long SnapshotDownloadAmount = 0;
};

extern struct_SnapshotStatus DownloadStatus;

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
    //! \brief Constructor.
    //!
    //! \param Defaultly this will be true but for snapshot we need to use our own curl to avoid issues with a crash from progress monitor.
    //!        In the destructor we check this as well so when snapshot download being used we don't get seg faulted out over clean up.
    //!
    Http(bool Scoped = true);

    //!
    //! \brief Destructor.
    //!
    ~Http();

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
    //! Downlaods the headers for \p url and attempts to find the ETag.
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
    //! \brief Fetch github release information.
    //!
    //! Downloads the json data from github that contains information about latest releases.
    //!
    //! \param url URL to fetch json from.
    //! \throws HttpException on invalid server response.
    //!
    std::string GetLatestVersionResponse(
            const std::string& url);
    //!
    //! \brief Download Snapshot with progress updates.
    //!
    //! Downloads the snapshot from the latest snapshot.zip hosted on download.gridcoin.us.
    //!
    //! \param url URL of the snapshot.zip.
    //! \param destination Destination of the snapshot file.
    //!
    //! \throws HttpException on invalid server response.
    //!
    void DownloadSnapshot(
            const std::string& url,
            const std::string& destination);
    //!
    //! \brief Fetch the sha256sum from snapshot server.
    //!
    //! Fetches the snapshot sha256 from snapshot server.
    //!
    //! \param url URL of the snapshot.zip.sha256
    //!
    //! \return the sha256sum from file
    //!
    //! \throws HttpException on invalid server response.
    //!
    std::string GetSnapshotSHA256(const std::string& url);

private:
    void EvaluateResponse(int code, const std::string& url);
    /** Downloading snapshot has a progress update function which collaspes when using scoped curl **/
    bool fScoped;
};
