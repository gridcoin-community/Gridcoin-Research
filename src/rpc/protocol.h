// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_PROTOCOL_H
#define BITCOIN_RPC_PROTOCOL_H

#include <list>
#include <map>
#include <stdint.h>
#include <string>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <univalue.h>

// Boost Support for 1.70+
#if BOOST_VERSION >= 107000
    #define GetIOService(s) ((boost::asio::io_context&)(s).get_executor().context())
    #define GetIOServiceFromPtr(s) ((boost::asio::io_context&)(s->get_executor().context())) // this one
    typedef boost::asio::io_context ioContext;

#else
    #define GetIOService(s) ((s).get_io_service())
    #define GetIOServiceFromPtr(s) ((s)->get_io_service())
    typedef boost::asio::io_service ioContext;
#endif

// HTTP status codes
enum HTTPStatusCode
{
    HTTP_OK                    = 200,
    HTTP_BAD_REQUEST           = 400,
    HTTP_UNAUTHORIZED          = 401,
    HTTP_FORBIDDEN             = 403,
    HTTP_NOT_FOUND             = 404,
    HTTP_INTERNAL_SERVER_ERROR = 500,
};

//! Gridcoin RPC error codes
enum RPCErrorCode
{
    //! Standard JSON-RPC 2.0 errors
    // RPC_INVALID_REQUEST is internally mapped to HTTP_BAD_REQUEST (400).
    // It should not be used for application-layer errors.
    RPC_INVALID_REQUEST  = -32600,
    // RPC_METHOD_NOT_FOUND is internally mapped to HTTP_NOT_FOUND (404).
    // It should not be used for application-layer errors.
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS   = -32602,
    // RPC_INTERNAL_ERROR should only be used for genuine errors in gridcoind
    // (for example datadir corruption).
    RPC_INTERNAL_ERROR   = -32603,
    RPC_PARSE_ERROR      = -32700,

    //! General application defined errors
    RPC_MISC_ERROR                  = -1,  //!< std::exception thrown in command handling
    RPC_TYPE_ERROR                  = -3,  //!< Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY      = -5,  //!< Invalid address or key
    RPC_OUT_OF_MEMORY               = -7,  //!< Ran out of memory during operation
    RPC_INVALID_PARAMETER           = -8,  //!< Invalid, missing or duplicate parameter
    RPC_DATABASE_ERROR              = -20, //!< Database error
    RPC_DESERIALIZATION_ERROR       = -22, //!< Error parsing or validating structure in raw format
    RPC_VERIFY_ERROR                = -25, //!< General error during transaction or block submission
    RPC_VERIFY_REJECTED             = -26, //!< Transaction or block was rejected by network rules
    RPC_VERIFY_ALREADY_IN_CHAIN     = -27, //!< Transaction already in chain
    RPC_IN_WARMUP                   = -28, //!< Client still warming up
    RPC_METHOD_DEPRECATED           = -32, //!< RPC method is deprecated

    //! Aliases for backward compatibility
    RPC_TRANSACTION_ERROR           = RPC_VERIFY_ERROR,
    RPC_TRANSACTION_REJECTED        = RPC_VERIFY_REJECTED,
    RPC_TRANSACTION_ALREADY_IN_CHAIN= RPC_VERIFY_ALREADY_IN_CHAIN,

    //! P2P client errors
    RPC_CLIENT_NOT_CONNECTED        = -9,  //!< Gridcoin is not connected
    RPC_CLIENT_IN_INITIAL_DOWNLOAD  = -10, //!< Still downloading initial blocks
    RPC_CLIENT_NODE_ALREADY_ADDED   = -23, //!< Node is already added
    RPC_CLIENT_NODE_NOT_ADDED       = -24, //!< Node has not been added before
    RPC_CLIENT_NODE_NOT_CONNECTED   = -29, //!< Node to disconnect not found in connected nodes
    RPC_CLIENT_INVALID_IP_OR_SUBNET = -30, //!< Invalid IP/Subnet
    RPC_CLIENT_P2P_DISABLED         = -31, //!< No valid connection manager instance found
    RPC_CLIENT_NODE_CAPACITY_REACHED= -34, //!< Max number of outbound or block-relay connections already open

    //! Chain errors
    RPC_CLIENT_MEMPOOL_DISABLED     = -33, //!< No mempool instance found

    //! Wallet errors
    RPC_WALLET_ERROR                = -4,  //!< Unspecified problem with wallet (key not found etc.)
    RPC_WALLET_INSUFFICIENT_FUNDS   = -6,  //!< Not enough funds in wallet or account
    RPC_WALLET_INVALID_LABEL_NAME   = -11, //!< Invalid label name
    RPC_WALLET_KEYPOOL_RAN_OUT      = -12, //!< Keypool ran out, call keypoolrefill first
    RPC_WALLET_UNLOCK_NEEDED        = -13, //!< Enter the wallet passphrase with walletpassphrase first
    RPC_WALLET_PASSPHRASE_INCORRECT = -14, //!< The wallet passphrase entered was incorrect
    RPC_WALLET_WRONG_ENC_STATE      = -15, //!< Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
    RPC_WALLET_ENCRYPTION_FAILED    = -16, //!< Failed to encrypt the wallet
    RPC_WALLET_ALREADY_UNLOCKED     = -17, //!< Wallet is already unlocked
    RPC_WALLET_NOT_FOUND            = -18, //!< Invalid wallet specified
    RPC_WALLET_NOT_SPECIFIED        = -19, //!< No wallet specified (error when there are multiple wallets loaded)
    RPC_WALLET_ALREADY_LOADED       = -35, //!< This same wallet is already loaded
    RPC_WALLET_ALREADY_EXISTS       = -36, //!< There is already a wallet with the same name

    //! Backwards compatible aliases
    RPC_WALLET_INVALID_ACCOUNT_NAME = RPC_WALLET_INVALID_LABEL_NAME,

    //! Unused reserved codes, kept around for backwards compatibility. Do not reuse.
    RPC_FORBIDDEN_BY_SAFE_MODE      = -2,  //!< Server is in safe mode, and command is not allowed in safe mode
};

//
// IOStream device that speaks SSL but can also speak non-SSL
//
template <typename Protocol>
class SSLIOStreamDevice : public boost::iostreams::device<boost::iostreams::bidirectional> {
public:
    SSLIOStreamDevice(boost::asio::ssl::stream<typename Protocol::socket> &streamIn, bool fUseSSLIn) : stream(streamIn)
    {
        fUseSSL = fUseSSLIn;
        fNeedHandshake = fUseSSLIn;
    }

    //! \return false if the TLS negotiation failed. Non-throwing for the same
    //! reason read() and write() are: this runs on an RPC worker thread with no
    //! handler above it, so an escaping exception terminates the process. An
    //! earlier revision converted only the data-transfer overloads and left this
    //! one throwing, which merely moved the abort earlier -- a client that
    //! disconnects mid-negotiation reaches here, not read().
    bool handshake(boost::asio::ssl::stream_base::handshake_type role)
    {
        if (!fNeedHandshake) return true;
        fNeedHandshake = false;

        boost::system::error_code ec;
        stream.handshake(role, ec);
        if (!ec) return true;

        // The connection never became usable. Callers translate this into their own
        // end-of-sequence result rather than throwing.
        return false;
    }
    std::streamsize read(char* s, std::streamsize n)
    {
        // HTTPS servers read first. A failed negotiation is end of sequence: there
        // is no usable connection to read from, and throwing here would abort the
        // process from a thread that cannot catch.
        if (!handshake(boost::asio::ssl::stream_base::server)) return -1;

        boost::system::error_code ec;
        const std::size_t bytes = fUseSSL
            ? stream.read_some(boost::asio::buffer(s, n), ec)
            : stream.next_layer().read_some(boost::asio::buffer(s, n), ec);

        if (!ec) return static_cast<std::streamsize>(bytes);

        // Boost.IOStreams signals end of sequence by RETURNING -1. The throwing
        // read_some overload used here previously raised boost::system::system_error
        // instead, and this device is driven from an RPC worker thread with no
        // handler above it -- so a peer closing its connection terminated the
        // process with "read_some: End of file".
        //
        // That is not an edge case. It is the normal end of every RPC call, and
        // AcceptedConnectionImpl::interrupt() deliberately shuts the socket down to
        // break a worker blocked in this read during shutdown, which produces the
        // same error by design. Whether the throw escapes is a timing question,
        // which is why it surfaced on one platform and not the others.
        //
        // Any read error leaves the connection unusable, so there is nothing to
        // distinguish: report end of sequence and let the worker close it.
        return -1;
    }
    std::streamsize write(const char* s, std::streamsize n)
    {
        // HTTPS clients write first. Report the write as consumed on a failed
        // negotiation, matching how the error path below treats a dead connection.
        if (!handshake(boost::asio::ssl::stream_base::client)) return n;

        boost::system::error_code ec;
        const std::size_t bytes = fUseSSL
            ? boost::asio::write(stream, boost::asio::buffer(s, n), ec)
            : boost::asio::write(stream.next_layer(), boost::asio::buffer(s, n), ec);

        if (!ec) return static_cast<std::streamsize>(bytes);

        // Same hazard in the other direction: a client that closes before reading
        // the reply gives a broken pipe here, and boost::asio::write's throwing
        // overload would abort the process over it. Report the write as consumed --
        // the connection is finished either way -- rather than throwing from a
        // thread that cannot catch.
        return n;
    }
    bool connect(const std::string& server, const std::string& port)
    {
        boost::asio::ip::tcp::resolver resolver(GetIOService(stream));
        boost::system::error_code error;

        for (const auto& res : resolver.resolve(server, port)) {
            stream.lowest_layer().close();
            stream.lowest_layer().connect(res, error);

            if (!error) {
                return true;
            }
        }

        return false;
    }

private:
    bool fNeedHandshake;
    bool fUseSSL;
    boost::asio::ssl::stream<typename Protocol::socket>& stream;
};

class AcceptedConnection
{
public:
    virtual ~AcceptedConnection() {}

    virtual std::iostream& stream() = 0;
    virtual std::string peer_address_to_string() const = 0;
    virtual void close() = 0;
    //! Wake a worker thread parked in a synchronous read on this connection by
    //! shutting down the underlying socket. Safe to call from another thread
    //! while the owning worker is blocked in ServiceConnection(); used by
    //! StopRPCThreads() so join_all() cannot hang on an idle keep-alive client
    //! (issue #3123).
    virtual void interrupt() = 0;
};

template <typename Protocol>
class AcceptedConnectionImpl : public AcceptedConnection
{
public:
    AcceptedConnectionImpl(
            ioContext& io_context,
            boost::asio::ssl::context &context,
            bool fUseSSL) :
        sslStream(io_context, context),
        _d(sslStream, fUseSSL),
        _stream(_d)
    {
    }

    virtual std::iostream& stream()
    {
        return _stream;
    }

    virtual std::string peer_address_to_string() const
    {
        return peer.address().to_string();
    }

    virtual void close()
    {
        _stream.close();
    }

    virtual void interrupt()
    {
        // shutdown() (rather than close()) is the portable way to break a
        // synchronous read that another thread is blocked in: it wakes the
        // recv without racing on file-descriptor reuse. The owning worker
        // still performs the actual close()/delete on its own thread.
        boost::system::error_code ec;
        sslStream.lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ec);
    }

    typename Protocol::endpoint peer;
    boost::asio::ssl::stream<typename Protocol::socket> sslStream;

private:
    SSLIOStreamDevice<Protocol> _d;
    boost::iostreams::stream< SSLIOStreamDevice<Protocol> > _stream;
};

std::string HTTPPost(const std::string& strMsg, const std::map<std::string,std::string>& mapRequestHeaders);
std::string HTTPReply(int nStatus, const std::string& strMsg, bool keepalive);
bool ReadHTTPRequestLine(std::basic_istream<char>& stream, int &proto,
                         std::string& http_method, std::string& http_uri);
int ReadHTTPStatus(std::basic_istream<char>& stream, int &proto);
int ReadHTTPHeaders(std::basic_istream<char>& stream, std::map<std::string, std::string>& mapHeadersRet);
int ReadHTTPMessage(std::basic_istream<char>& stream, std::map<std::string, std::string>& mapHeadersRet,
                    std::string& strMessageRet, int nProto);
std::string JSONRPCRequest(const std::string& strMethod, const UniValue& params, const UniValue& id);
UniValue JSONRPCReplyObj(const UniValue& result, const UniValue& error, const UniValue& id);
std::string JSONRPCReply(const UniValue& result, const UniValue& error, const UniValue& id);
UniValue JSONRPCError(int code, const std::string& message);

#endif // BITCOIN_RPC_PROTOCOL_H
