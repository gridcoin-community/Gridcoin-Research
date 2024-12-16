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

    void handshake(boost::asio::ssl::stream_base::handshake_type role)
    {
        if (!fNeedHandshake) return;
        fNeedHandshake = false;
        stream.handshake(role);
    }
    std::streamsize read(char* s, std::streamsize n)
    {
        handshake(boost::asio::ssl::stream_base::server); // HTTPS servers read first
        if (fUseSSL) return stream.read_some(boost::asio::buffer(s, n));
        return stream.next_layer().read_some(boost::asio::buffer(s, n));
    }
    std::streamsize write(const char* s, std::streamsize n)
    {
        handshake(boost::asio::ssl::stream_base::client); // HTTPS clients write first
        if (fUseSSL) return boost::asio::write(stream, boost::asio::buffer(s, n));
        return boost::asio::write(stream.next_layer(), boost::asio::buffer(s, n));
    }
    bool connect(const std::string& server, const std::string& port)
    {
        boost::asio::ip::tcp::resolver resolver(GetIOService(stream));
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(server, port);
        boost::asio::ip::tcp::resolver::iterator end;
        boost::system::error_code error = boost::asio::error::host_not_found;
        while (error && endpoint_iterator != end)
        {
            stream.lowest_layer().close();
            stream.lowest_layer().connect(*endpoint_iterator++, error);
        }
        if (error)
            return false;
        return true;
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
