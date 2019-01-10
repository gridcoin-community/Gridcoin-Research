#include "http.h"
#include "tinyformat.h"

#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <memory>

// Only for troubleshooting. This should be removed before production.
#include "util.h"

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
}

Http::Http()
{
    curl_global_init(CURL_GLOBAL_ALL);
}

Http::~Http()
{
    curl_global_cleanup();
}

void Http::Download(
        const std::string &url,
        const std::string &destination,
        const std::string &userpass)
{
    ScopedFile fp(fopen(destination.c_str(), "wb"), &fclose);
    if(!fp)
        throw std::runtime_error(
                tfm::format("Error opening target %s: %s (%d)", destination, strerror(errno), errno));

    std::string buffer;
    ScopedCurl curl = GetContext();
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_write_file);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, fp.get());
    curl_easy_setopt(curl.get(), CURLOPT_USERPWD, userpass.c_str());
#ifdef WIN32
    // Disable certificate verification for WCG if on Windows because something doesn't work.
    // This is intended to be a temporary workaround.
    if (url.find("www.worldcommunitygrid.org") != std::string::npos)
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, FALSE);
#endif

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
    headers = curl_slist_append(headers, "User-Agent: curl/7.58.0");
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
#ifdef WIN32
    // Disable certificate verification for WCG if on Windows because something doesn't work.
    // This is intended to be a temporary workaround.
    if (url.find("www.worldcommunitygrid.org") != std::string::npos)
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, FALSE);
#endif

    CURLcode res = curl_easy_perform(curl.get());
    curl_slist_free_all(headers);

    if (res > 0)
        throw std::runtime_error(tfm::format("Failed to get ETag for URL %s: %s", url, curl_easy_strerror(res)));

    // Validate HTTP return code.
    long response_code;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
    EvaluateResponse(response_code, url);

    // Find ETag header.
    std::string etag;
    
    if(fDebug3) _log(logattribute::INFO, "Http::ETag", "Header: \n" + header);
    
    std::istringstream iss(header);
    for (std::string line; std::getline(iss, line);)
    {
        if(size_t pos = line.find("ETag: ") != std::string::npos)
        {
            etag = line.substr(pos+6, line.size());
            etag = etag.substr(1, etag.size() - 3);
            return etag;
        }
    }

    if (etag.empty())
        throw std::runtime_error("No ETag response from project url <urlfile=" + url + ">");

    return std::string();
    
    // This needs to go away for production along with the above external function declaration and the fixup enum to support _log here.
    if (fDebug)
        _log(logattribute::INFO, "curl_http_header", "Captured ETag for project url <urlfile=" + url + ", ETag=" + etag + ">");
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
