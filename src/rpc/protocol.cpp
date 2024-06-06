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
#include "util.h"

#include <boost/asio.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/bind/bind.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
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

int ReadHTTPHeaders(std::basic_istream<char>& stream, std::map<std::string, std::string>& mapHeadersRet)
{
    int nLen = 0;
    while (true)
    {
        std::string str;
        std::getline(stream, str);
        if (str.empty() || str == "\r")
            break;
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
                throw std::invalid_argument("Unable to parse content-length value.");
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
    boost::split(vWords, str, boost::is_any_of(" "));
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

    // Strip the \r, which MUST be present according to the HTTP standard.
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
    boost::split(vWords, str, boost::is_any_of(" "));
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
    if (nLen < 0 || nLen > (int)MAX_SIZE)
        return HTTP_INTERNAL_SERVER_ERROR;

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
