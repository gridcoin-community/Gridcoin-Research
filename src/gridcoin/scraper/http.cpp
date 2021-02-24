// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gridcoin/scraper/http.h"
#include "tinyformat.h"
#include "util.h"

#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <boost/thread.hpp>

#if LIBCURL_VERSION_NUM >= 0x073d00
/* In libcurl 7.61.0, support was added for extracting the time in plain
   microseconds. Lets make this so we can allow a user using own depends
   for compile. */
#define timetype curl_off_t
#define timeopt CURLINFO_TOTAL_TIME_T
#define progressinterval 3000000
#else
#define timetype double
#define timeopt CURLINFO_TOTAL_TIME
#define progressinterval 1
#endif

struct_SnapshotStatus DownloadStatus;

enum class logattribute {
    // Can't use ERROR here because it is defined already in windows.h.
    ERR,
    INFO,
    WARNING,
    CRITICAL
};

extern void _log(logattribute eType, const std::string& sCall, const std::string& sMessage);

namespace
{
    size_t curl_write_file(void* ptr, size_t size, size_t nmemb, FILE* fp)
    {
        return fwrite(ptr, size, nmemb, fp);
    }

    size_t curl_write_string(void* ptr, size_t size, size_t nmemb, void* userp)
    {
        std::string& str = *static_cast<std::string*>(userp);
        str.append(static_cast<char*>(ptr), size * nmemb);
        return size * nmemb;
    }

    typedef std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> ScopedCurl;
    typedef std::unique_ptr<FILE, decltype(&fclose)> ScopedFile;

    ScopedCurl GetContext()
    {
        ScopedCurl curl(curl_easy_init(), &curl_easy_cleanup);

        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_PROXY, "");
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_UNRESTRICTED_AUTH, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 0);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 10000L);
        curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 60L);

        return curl;
    }

    struct progress {
      timetype lastruntime;
      CURL *curl;
    };

    static int newerprogress_callback(void *ptr, curl_off_t downtotal, curl_off_t downnow, curl_off_t uptotal, curl_off_t uplnow)
    {
        struct progress *pg = (struct progress*)ptr;
        CURL *curl = pg->curl;

        // Thread inturrupting
        try
        {
            boost::this_thread::interruption_point();
            // Set this once.
            if (DownloadStatus.SnapshotDownloadSize == 0)
                DownloadStatus.SnapshotDownloadSize = downtotal;

            timetype currenttime = 0;
            curl_easy_getinfo(curl, timeopt, &currenttime);

            // Update every 1 second
            if ((currenttime - pg->lastruntime) >= progressinterval)
            {
                pg->lastruntime = currenttime;

                double speed;
                CURLcode result;

                result = curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &speed);

                // Download speed update
                if (result == CURLE_OK)
                {

#if LIBCURL_VERSION_NUM >= 0x073700
                    if (speed > 0)
                        DownloadStatus.SnapshotDownloadSpeed = (int64_t)speed;

                    else {
                        DownloadStatus.SnapshotDownloadSpeed = 0;

                    }
#else
                    // Not supported by libcurl
                    DownloadStatus.SnapshotDownloadSpeed = -1;
#endif
                    if (DownloadStatus.SnapshotDownloadSize > 0 && (downnow > 0))
                    {
                        DownloadStatus.SnapshotDownloadProgress = ((downnow / (double)DownloadStatus.SnapshotDownloadSize) * 100);
                        DownloadStatus.SnapshotDownloadAmount = downnow;
                    }
                }
            }
        }

        catch (boost::thread_interrupted&)
        {
            return 1;
        }

        return 0;
    };

#if LIBCURL_VERSION_NUM < 0x072000
    static int olderprogress_callback(void *ptr, double downtotal, double downnow, double uptotal, double upnow)
    {
        return newerprogress_callback(ptr, (curl_off_t)downtotal, (curl_off_t)downnow, (curl_off_t)uptotal, (curl_off_t)upnow);
    };

#endif

    //!
    //! \brief Parse an etag value from an HTTP header field.
    //!
    //! This will parse etag values from headers in these standard formats:
    //!
    //!   ETag: "12345"
    //!   ETag: W/"12345"
    //!
    //! The name of the header is matched in a case-insensitive fashion. This
    //! function will return an empty string for non-standard, malformed, and
    //! non-etag headers. It removes the quotes from the output and ignores a
    //! space after the colon that separates the header name from the value.
    //!
    //! \param header Entire HTTP header field that includes the name and value.
    //!
    //! \return The parsed etag value or an empty string if the supplied header
    //! contains no standard etag content.
    //!
    std::string ParseEtag(const std::string& header)
    {
        if (header.size() <= 8 || header[4] != ':') {
            return std::string();
        }

        constexpr char expected[] = "etag";
        constexpr int32_t to_upper = 32;

        for (size_t i = 0; i < 4; ++i) {
            if (header[i] != expected[i] && header[i] != expected[i] - to_upper) {
                return std::string();
            }
        }

        const size_t start_quote = header.find('"', 5);
        const size_t end_quote = header.find('"', start_quote + 1);

        if (start_quote == std::string::npos || end_quote == std::string::npos) {
            return std::string();
        }

        return header.substr(start_quote + 1, end_quote - start_quote - 1);
    }
} // anonymous namespace

Http::CurlLifecycle::CurlLifecycle()
{
    curl_global_init(CURL_GLOBAL_ALL);
}

Http::CurlLifecycle::~CurlLifecycle()
{
    curl_global_cleanup();
}

Http::CurlLifecycle Http::curl_lifecycle;

void Http::Download(
        const std::string &url,
        const fs::path &destination,
        const std::string &userpass)
{
    ScopedFile fp(fsbridge::fopen(destination, "wb"), &fclose);
    if (!fp)
        throw std::runtime_error(
                tfm::format("Error opening target %s: %s (%d)", destination, strerror(errno), errno));

    std::string buffer;
    ScopedCurl curl = GetContext();
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_write_file);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, fp.get());
    curl_easy_setopt(curl.get(), CURLOPT_USERPWD, userpass.c_str());

    CURLcode res = curl_easy_perform(curl.get());
    if (res > 0)
        throw std::runtime_error(tfm::format("Failed to download file %s: %s", url, curl_easy_strerror(res)));
}

std::string Http::GetEtag(
        const std::string &url,
        const std::string &userpass)
{
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "User-Agent: curl/7.63.0");
    std::string header;
    std::string buffer;

    ScopedCurl curl = GetContext();
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_write_string);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &header);
    curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl.get(), CURLOPT_USERPWD, userpass.c_str());

    CURLcode res = curl_easy_perform(curl.get());
    curl_slist_free_all(headers);

    if (res > 0)
        throw std::runtime_error(tfm::format("Failed to get ETag for URL %s: %s", url, curl_easy_strerror(res)));

    // Validate HTTP return code.
    long response_code;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
    EvaluateResponse(response_code, url);

    _log(logattribute::INFO, "Http::ETag", "Header: \n" + header);

    std::istringstream iss(header);
    for (std::string line; std::getline(iss, line);)
    {
        std::string etag = ParseEtag(line);

        if (!etag.empty())
        {
            return etag;
        }
    }

    throw std::runtime_error("No ETag response from project url <urlfile=" + url + ">");
}

std::string Http::GetLatestVersionResponse()
{
    std::string buffer;
    std::string header;
    std::string url = GetArg("-updatecheckurl", "https://api.github.com/repos/gridcoin-community/Gridcoin-Research/releases/latest");

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "User-Agent: curl/7.63.0");

    ScopedCurl curl = GetContext();
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_write_string);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &header);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl.get());

    if (res > 0)
        throw std::runtime_error(tfm::format("Failed to get version response from URL %s: %s", url, curl_easy_strerror(res)));

    curl_slist_free_all(headers);

    // Validate HTTP return code.
    long response_code;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
    EvaluateResponse(response_code, url);

    // Return the Http response
    return buffer;
}

void Http::DownloadSnapshot()
{
    std::string url = GetArg("-snapshoturl", "https://snapshot.gridcoin.us/snapshot.zip");

    fs::path destination = GetDataDir() / "snapshot.zip";

    ScopedFile fp(fsbridge::fopen(destination, "wb"), &fclose);

    if (!fp)
    {
        DownloadStatus.SnapshotDownloadFailed = true;

        throw std::runtime_error(
                tfm::format("Snapshot Downloader: Error opening target %s: %s (%d)", destination.string(), strerror(errno), errno));
    }

    std::string buffer;
    std::string header;

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "User-Agent: curl/7.63.0");

    CURL* curl;
    curl = curl_easy_init();

    struct progress fileprogress;

    fileprogress.lastruntime = 0;
    fileprogress.curl = curl;
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_PROXY, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 10000L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp.get());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

#if LIBCURL_VERSION_NUM >= 0x072000
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, newerprogress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &fileprogress);
#else
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, olderprogress_callback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &fileprogress);
#endif

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res > 0)
    {
        if (res == CURLE_ABORTED_BY_CALLBACK)
            return;

        else
        {
            DownloadStatus.SnapshotDownloadFailed = true;

            throw std::runtime_error(tfm::format("Snapshot Downloader: Failed to download file %s: %s", url, curl_easy_strerror(res)));
        }
    }

    // Validate HTTP return code.
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    EvaluateResponse(response_code, url);

    DownloadStatus.SnapshotDownloadComplete = true;

    return;
}

std::string Http::GetSnapshotSHA256()
{
    std::string buffer;
    std::string header;
    std::string url = GetArg("-snapshotsha256url", "https://snapshot.gridcoin.us/snapshot.zip.sha256");

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "User-Agent: curl/7.63.0");

    ScopedCurl curl = GetContext();
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_write_string);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &header);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl.get());

    curl_slist_free_all(headers);

    if (res > 0)
    {
       LogPrintf("Snapshot (GetSnapshotSHA256):  Failed to SHA256SUM of snapshot.zip for URL %s: %s", url, curl_easy_strerror(res));

       return "";
    }

    // Validate HTTP return code.
    long response_code;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
    EvaluateResponse(response_code, url);

    if (buffer.empty())
    {
        LogPrintf("Snapshot (GetSnapshotSHA256): Failed to receive SHA256SUM from url: %s", url);

        return "";
    }

    size_t loc = buffer.find(" ");

    if (loc == std::string::npos)
    {
        LogPrintf("Snapshot (GetSnapshotSHA256): Malformed SHA256SUM from url: %s", url);

        return "";
    }

    else
    {
        LogPrint(BCLog::LogFlags::VERBOSE, "Snapshot Downloader: Receives SHA256SUM of %s", buffer.substr(0, loc));

        return buffer.substr(0, loc);
    }
}

void Http::EvaluateResponse(int code, const std::string& url)
{
    // Check code to make sure we have success as even a response is considered an OK
    // We check only on head requests since they give us the information we need
    // Codes we send back true and wait for other HTTP/ code is 301, 302, 307 and 308 since these are follows
    if (code == 200 ||
        code == 301 ||
        code == 302 ||
        code == 307 ||
        code == 308)
        return;
    else if (code == 400)
        throw HttpException(tfm::format("Server returned a http code of Bad Request <url=%s, code=%d>", url, code));
    else if (code == 401)
        throw HttpException(tfm::format("Server returned a http code of Unauthorized <url=%s, code=%d>", url, code));
    else if (code == 403)
        throw HttpException(tfm::format("Server returned a http code of Forbidden <url=%s, code=%d>", url, code));
    else if (code == 404)
        throw HttpException(tfm::format("Server returned a http code of Not Found <url=%s, code=%d>", url, code));

    throw HttpException(tfm::format("Server returned a http code <url=%s, code=%d>", url, code));
}
