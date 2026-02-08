#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <map>
#include <memory>
#include <string>

namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class Aggregator;

using BidsMap = std::map<double, double, std::greater<double>>;
using AsksMap = std::map<double, double>;

class Connector : public std::enable_shared_from_this<Connector> {
public:
    Connector(net::io_context& ioc, ssl::context& ssl_ctx, Aggregator* agg, std::string name);
    virtual ~Connector() = default;

    void start();

    virtual std::string host() const = 0;
    virtual std::string port() const = 0;
    virtual std::string path() const = 0;
    virtual std::string subscribe_message() const = 0;
    virtual bool needs_ping() const = 0;
    virtual void parse_message(const std::string& msg) = 0;

    BidsMap get_bids_snapshot();
    friend class BinanceConnectorTest_ParseSnapshot_Test;
    friend class BinanceConnectorTest_ParseUpdateAdd_Test;
    friend class BinanceConnectorTest_InvalidJSON_Test;
    friend class BinanceConnectorTest_ZeroQuantityRemoved_Test;
protected:
    void do_resolve();
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::endpoint_type ep);
    void on_ssl_handshake(beast::error_code ec);
    void on_ws_handshake(beast::error_code ec);
    void on_subscribe_write(beast::error_code ec, std::size_t bytes_transferred);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void run_ping_timer();
    void schedule_reconnect();

    net::io_context& ioc_;
    net::strand<net::io_context::executor_type> strand_;
    ssl::context& ssl_ctx_;
    Aggregator* aggregator_;
    std::string name_;

    // 关键：ws_ 在构造函数中初始化
    std::unique_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>> ws_;
    tcp::resolver resolver_;
    beast::flat_buffer buffer_;
    net::steady_timer timer_;

    BidsMap local_bids_;
    AsksMap local_asks_;
};