#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket/ssl.hpp>  // MUST include this for SSL support

// ...

protected:
    // Recommended: ssl + tcp_stream
    using ws_type = websocket::stream<ssl::stream<beast::tcp_stream>>;
    std::shared_ptr<ws_type> ws_;

#include <memory>
#include <string>

namespace net = boost::asio;
namespace ssl = net::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class Connector : public std::enable_shared_from_this<Connector> {
public:
    Connector(net::io_context& ioc,
              ssl::context& ssl_ctx,
              const std::string& host,
              const std::string& port,
              const std::string& path);

    virtual ~Connector() = default;

    void start();
    void send(const std::string& msg);

protected:
    virtual void on_message(const std::string& msg) = 0;

private:
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
    void on_ssl_handshake(beast::error_code ec);
    void on_ws_handshake(beast::error_code ec);
    void do_read();

protected:
    net::io_context& ioc_;
    ssl::context& ssl_ctx_;
    tcp::resolver resolver_;
    websocket::stream<ssl::stream<beast::tcp_stream>> ws_;
    beast::flat_buffer buffer_;

    std::string host_;
    std::string port_;
    std::string path_;
};
