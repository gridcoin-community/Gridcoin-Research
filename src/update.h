#ifndef UPDATE_H
#define UPDATE_H


#pragma once
#include "netbase.h"
#include "net.h"

#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <ostream>

class update
{
protected:
    // HTTPS data
    std::stringstream reply;
    std::string strippedreply = "";

private:
    // SSL
    int statuscode = 0;
    int sendlen = 0;
    int recvlen = 0;

    SSL* ssl = nullptr;
    const SSL_METHOD* meth = nullptr;
    SSL_CTX* ctx = nullptr;
    CService address;

    // Socket
    int sock = 0;
    int s = 0;
    socklen_t socklen;
    char buffer[16384];

    struct sockaddr_in sa;

    // Private functions
    bool sslinit()
    {
        SSL_library_init();
        SSLeay_add_ssl_algorithms();
        SSL_load_error_strings();

        // functions for client method, etc change in 1.1.0 openssl
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        meth = TLS_client_method();
#elif (OPENSSL_VERSION_NUMBER < 0x10100000L)
        meth = TLSv1_2_client_method();
#endif

        ctx = SSL_CTX_new(meth);
        ssl = SSL_new(ctx);

        if (ssl)
            return true;

        else
            return false;
    }

    bool addresslookup()
    {
        const char httpsaddress[] = "api.github.com";

        if(!Lookup(httpsaddress, address, 0, true))
            return false;

        return true;
    }

    bool socketinit()
    {
        s = socket(AF_INET, SOCK_STREAM, 0);

        if (!s)
            return false;

        sa.sin_family = AF_INET;
        sa.sin_port = htons(443);
        inet_pton(AF_INET, address.ToStringIP().c_str(), &(sa.sin_addr));

        socklen = sizeof(sa);

        return true;
    }

    bool socketconnect()
    {
        if (connect(s, (struct sockaddr*)&sa, socklen) < 0)
            return false;

        sock = SSL_get_fd(ssl);
        SSL_set_fd(ssl, s);

        statuscode = SSL_connect(ssl);

        if (statuscode <= 0)
            return false;

        return true;
    }

    void clearstatus()
    {
        closessl();
        closeusedsocket();
        ssl = nullptr;
        meth = nullptr;
        ctx = nullptr;
        sock = 0;
        s = 0;
    }

    bool requestdata()
    {
        const char webcall[] = "GET /repos/gridcoin-community/Gridcoin-Research/releases/latest HTTP/1.1\r\n"
                               "Host: api.github.com\r\n"
                               "User-Agent: Gridcoin\r\n"
                               "Connection: Close\r\n"
                               "\r\n";

        sendlen = SSL_write(ssl, webcall, strlen(webcall));

        if (sendlen < 0)
        {
            statuscode = SSL_get_error(ssl, sendlen);

            return false;
        }

        return true;
    }

    bool recvreply()
    {
        while (true)
        {
            recvlen = SSL_read(ssl, buffer, 16384);

            if (recvlen < 0)
            {
                statuscode = SSL_get_error(ssl, recvlen);

                return false;
            }

            if (recvlen == 0)
                break;

            buffer[recvlen] = 0;

            reply << buffer;
        }

        return true;
    }

    void closeusedsocket()
    {
        close(sock);
        close(s);
    }

    void closessl()
    {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
    }

    void stripreply()
    {
        std::string temp;

        bool found = false;

        while (std::getline(reply, temp))
        {
            // The body is one line of json
            if (found)
            {
                strippedreply = temp;

                break;
            }

            // We know there is an empty line with a new line after HTTP header data
            if (temp[0] == 0x0D)
                found = true;
        }
    }
public:
    bool VersionCheck();
};

#endif // UPDATE_H
