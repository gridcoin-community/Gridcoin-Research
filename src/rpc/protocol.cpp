// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "init.h"
#include "sync.h"
#include "node/ui_interface.h"
#include "protocol.h"
#include <key_io.h>
#include "wallet/db.h"
#include <util.h>
#include <util/string.h>

#include <boost/asio.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/bind/bind.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/shared_ptr.hpp>
#include <list>

#include <memory>

//
// HTTP protocol
//
// This ain't Apache.  We're just using HTTP header for the length field
// and to be compatible with other JSON-RPC implementations.
//

std::string HTTPPost(const std::string& strMsg, const std::map<std::string,std::string>& mapRequestHeaders)
{
    std::ostringstream s;
    s << "POST / HTTP/1.1\r\n"
      << "User-Agent: gridcoin-json-rpc/" << FormatFullVersion() << "\r\n"
      << "Host: 127.0.0.1\r\n"
      << "Content-Type: application/json\r\n"
      << "Content-Length: " << strMsg.size() << "\r\n"
      << "Connection: close\r\n"
      << "Accept: application/json\r\n";
    for (auto const& item : mapRequestHeaders)
        s << item.first << ": " << item.second << "\r\n";
    s << "\r\n" << strMsg;

    return s.str();
}

std::string rfc1123Time()
{
    return DateTimeStrFormat("%a, %d %b %Y %H:%M:%S +0000", GetTime());
}

std::string HTTPReply(int nStatus, const std::string& strMsg, bool keepalive)
{
    if (nStatus == HTTP_UNAUTHORIZED)
        return strprintf("HTTP/1.0 401 Authorization Required\r\n"
            "Date: %s\r\n"
            "Server: gridcoin-json-rpc/%s\r\n"
            "WWW-Authenticate: Basic realm=\"jsonrpc\"\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 296\r\n"
            "\r\n"
            "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\r\n"
            "\"http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd\">\r\n"
            "<HTML>\r\n"
            "<HEAD>\r\n"
            "<TITLE>Error</TITLE>\r\n"
            "<META HTTP-EQUIV='Content-Type' CONTENT='text/html; charset=ISO-8859-1'>\r\n"
            "</HEAD>\r\n"
            "<BODY><H1>401 Unauthorized.</H1></BODY>\r\n"
            "</HTML>\r\n", rfc1123Time().c_str(), FormatFullVersion().c_str());
    const char *cStatus;
         if (nStatus == HTTP_OK) cStatus = "OK";
    else if (nStatus == HTTP_BAD_REQUEST) cStatus = "Bad Request";
    else if (nStatus == HTTP_FORBIDDEN) cStatus = "Forbidden";
    else if (nStatus == HTTP_NOT_FOUND) cStatus = "Not Found";
    else if (nStatus == HTTP_INTERNAL_SERVER_ERROR) cStatus = "Internal Server Error";
    else cStatus = "";
    return strprintf(
            "HTTP/1.1 %d %s\r\n"
            "Date: %s\r\n"
            "Connection: %s\r\n"
            "Content-Length: %" PRIszu "\r\n"
            "Content-Type: application/json\r\n"
            "Server: gridcoin-json-rpc/%s\r\n"
            "\r\n"
            "%s",
        nStatus,
        cStatus,
        rfc1123Time(),
        keepalive ? "keep-alive" : "close",
        strMsg.size(),
        FormatFullVersion(),
        strMsg);
}

//! \brief Maximum number of header lines accepted from one request.
//!
//! A legitimate RPC request sends a handful -- host, connection, content-type,
//! content-length, authorization, user-agent. 100 is far beyond that while
//! bounding what an unauthenticated client can insert into mapHeadersRet.
static constexpr size_t MAX_HTTP_HEADER_COUNT = 100;

//! \brief Maximum length of a single header line.
//!
//! 8 KiB, the de facto limit Apache and nginx apply. Bounding the individual line
//! matters as much as bounding the count: std::getline grows its string until a
//! delimiter or EOF, so a client that opens a header and never sends a newline
//! is buffered without limit.
static constexpr size_t MAX_HTTP_HEADER_LINE_BYTES = 8192;

//! \brief Maximum total bytes of header lines accepted from one request.
//!
//! Count times line length would allow 800 KiB, which is far more than the
//! headers of any real request; 32 KiB is generous against a typical few hundred
//! bytes and caps the aggregate independently of how it is divided up.
static constexpr size_t MAX_HTTP_HEADER_TOTAL_BYTES = 32768;

int ReadHTTPHeaders(std::basic_istream<char>& stream, std::map<std::string, std::string>& mapHeadersRet)
{
    int nLen = 0;
    size_t header_count = 0;
    size_t header_bytes = 0;
    bool overlong = false;

    // All three bounds are reachable BEFORE any authentication check, which is
    // what makes them worth having: nothing above this point has established who
    // the client is.
    while (true)
    {
        std::string str;

        if (!ReadLineBounded(stream, str, MAX_HTTP_HEADER_LINE_BYTES, overlong)) {
            // Negative, not a status code: this function returns a CONTENT LENGTH,
            // and the caller rejects a negative one. Returning 400 here would be
            // read as a body of 400 bytes.
            if (overlong) return -1;

            break;
        }

        if (str.empty() || str == "\r")
            break;

        if (++header_count > MAX_HTTP_HEADER_COUNT) return -1;

        header_bytes += str.size();
        if (header_bytes > MAX_HTTP_HEADER_TOTAL_BYTES) return -1;
        std::string::size_type nColon = str.find(":");
        if (nColon != std::string::npos)
        {
            std::string strHeader = str.substr(0, nColon);
            strHeader = TrimString(strHeader);
            strHeader = ToLower(strHeader);
            std::string strValue = str.substr(nColon+1);
            strValue = TrimString(strValue);
            mapHeadersRet[strHeader] = strValue;
            if (strHeader == "content-length" && !ParseInt32(strValue, &nLen)) {
                // Returning rather than throwing: ServiceConnection has no
                // try/catch around this call, so an exception here escapes on a
                // malformed header from an unauthenticated client. Negative
                // because this returns a content length, which the caller
                // rejects when negative.
                return -1;
            }
        }
    }
    return nLen;
}

bool ReadHTTPRequestLine(std::basic_istream<char>& stream, int &proto,
                         std::string& http_method, std::string& http_uri)
{
    std::string str;
    // This strips the \n but does NOT strip any extra \r, such as the \r\n in the HTTP standard field line ending.
    std::getline(stream, str);

    // HTTP request line is space-delimited
    std::vector<std::string> vWords;
    vWords = SplitString(str, ' ');
    if (vWords.size() < 2)
        return false;

    // HTTP methods permitted: GET, POST
    http_method = vWords[0];
    if (http_method != "GET" && http_method != "POST")
        return false;

    // HTTP URI must be an absolute path, relative to current host
    http_uri = vWords[1];
    if (http_uri.size() == 0 || http_uri[0] != '/')
        return false;

    // Parse proto, if present. If not present, return true to shorten processing.
    if (vWords.size() != 3) return true;

    std::string strProto = vWords[2];

    // Strip the \r, which MUST be present according to the HTTP standard. Guarded
    // because the request line "GET / " splits into three words whose third is
    // empty, and pop_back() on an empty string is undefined -- reachable before
    // authentication.
    if (strProto.empty()) return false;

    strProto.pop_back();
    size_t length = strProto.length();

    size_t start_pos = strProto.find("HTTP/1.");

    if (start_pos != std::string::npos && length - start_pos > 7) {
        strProto = strProto.substr(start_pos + 7);

        if (!ParseInt32(strProto, &proto)) {
            return error("%s: Unable to parse protocol in HTTP string: %s", __func__, strProto);
        }
    }

    return true;
}

int ReadHTTPStatus(std::basic_istream<char>& stream, int &proto)
{
    std::string str;
    std::getline(stream, str);
    std::vector<std::string> vWords;
    vWords = SplitString(str, ' ');
    if (vWords.size() < 2)
        return HTTP_INTERNAL_SERVER_ERROR;
    str.pop_back();

    size_t start_pos = str.find("HTTP/1.");

    if (start_pos != std::string::npos && str.length() - start_pos > 7) {
        str = str.substr(start_pos + 7);

        if (!ParseInt32(str, &proto)) {
            error("%s: Unable to parse protocol in HTTP string: %s", __func__, str);
        }
    }

    int status = 0;
    if (!ParseInt32(vWords[1], &status)) {
        error("%s: Unable to parse status: %s", __func__, vWords[1]);
    }

    return status;
}

int ReadHTTPMessage(std::basic_istream<char>& stream, std::map<std::string,
                    std::string>& mapHeadersRet, std::string& strMessageRet,
                    int nProto)
{
    mapHeadersRet.clear();
    strMessageRet = "";

    // Read header
    int nLen = ReadHTTPHeaders(stream, mapHeadersRet);
    // MAX_SIZE is 32 MiB, so this accepted an unauthenticated header field that
    // then drove a 32 MiB allocation below. The worst legitimate request is
    // signrawtransaction fed by consolidatemsunspent output, where the prevtxs
    // array dominates because every multisig input carries its own redeemScript;
    // bounded by MAX_STANDARD_TX_SIZE at roughly 1.8x, that is 400-500 KB. 20x
    // MAX_STANDARD_TX_SIZE leaves several times that.
    if (nLen < 0 || nLen > (int)MAX_RPC_BODY_SIZE)
        return HTTP_BAD_REQUEST;

    // Read message
    if (nLen > 0)
    {
        std::vector<char> vch(nLen);
        stream.read(&vch[0], nLen);
        strMessageRet = std::string(vch.begin(), vch.end());
    }

    std::string sConHdr = mapHeadersRet["connection"];

    if ((sConHdr != "close") && (sConHdr != "keep-alive"))
    {
        if (nProto >= 1)
            mapHeadersRet["connection"] = "keep-alive";
        else
            mapHeadersRet["connection"] = "close";
    }

    return HTTP_OK;
}

//
// JSON-RPC protocol.  Bitcoin speaks version 1.0 for maximum compatibility,
// but uses JSON-RPC 1.1/2.0 standards for parts of the 1.0 standard that were
// unspecified (HTTP errors and contents of 'error').
//
// 1.0 spec: http://json-rpc.org/wiki/specification
// 1.2 spec: http://jsonrpc.org/historical/json-rpc-over-http.html
// https://www.codeproject.com/KB/recipes/JSON_Spirit.aspx
//

std::string JSONRPCRequest(const std::string& strMethod, const UniValue& params, const UniValue& id)
{
    UniValue request(UniValue::VOBJ);
    request.pushKV("method", strMethod);
    request.pushKV("params", params);
    request.pushKV("id", id);
    return request.write() + "\n";
}

UniValue JSONRPCReplyObj(const UniValue& result, const UniValue& error, const UniValue& id)
{
    UniValue reply(UniValue::VOBJ);
    if (!error.isNull())
        reply.pushKV("result", NullUniValue);
    else
        reply.pushKV("result", result);
    reply.pushKV("error", error);
    reply.pushKV("id", id);
    return reply;
}

std::string JSONRPCReply(const UniValue& result, const UniValue& error, const UniValue& id)
{
    UniValue reply = JSONRPCReplyObj(result, error, id);
    return reply.write() + "\n";
}

UniValue JSONRPCError(int code, const std::string& message)
{
    UniValue error(UniValue::VOBJ);
    error.pushKV("code", code);
    error.pushKV("message", message);
    return error;
}
