#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>  // 必须，用于 SSL + WebSocket
#include <boost/beast/ssl.hpp>            // beast::get_lowest_layer 等
#include <boost/system/error_code.hpp>
#include <functional>
#include <string>
#include <memory>
#include <chrono>
#include <map>

class Aggregator;  // Forward declaration

namespace net   = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace websocket = beast::websocket;
namespace ssl   = boost::asio::ssl;
using tcp = net::ip::tcp;

class market_connector : public std::enable_shared_from_this<market_connector> {
public:
    using event_callback = std::function<void(const std::string&, const std::string&)>;

    // market_connector(net::io_context& ioc, std::string name, std::string host,
    //                  std::string port, std::string path, event_callback cb);
    market_connector(net::io_context& ioc, Aggregator* aggregator, std::string name,
                   std::string host, std::string port, std::string path, event_callback cb);
    
    virtual ~market_connector() = default;

    void start();
    // 新增 public getter（const 引用，避免拷贝）
    const auto& get_bids() const { return local_bids_; }
    const auto& get_asks() const { return local_asks_; }
    
protected:
    virtual std::string subscription_message() const = 0;
    virtual void handle_message(const std::string& msg) = 0;

    virtual void parse_message(const std::string& msg) = 0;  // Subclass implements parsing

    virtual void fail(const boost::system::error_code& ec, const char* what);

    Aggregator* aggregator_;  // To notify on update

    net::io_context& ioc_;
    std::string name_;
    std::string host_;
    std::string port_;
    std::string path_;
    event_callback callback_;

    tcp::resolver resolver_;
    ssl::context ssl_ctx_;
    websocket::stream<ssl::stream<beast::tcp_stream>> ws_;
    beast::flat_buffer buffer_;
    
    bool stopped_ = false;
    
    // net::strand<net::io_context::executor_type> strand_;

    std::map<double, double, std::greater<double>> local_bids_;
    std::map<double, double> local_asks_;

private:
    net::steady_timer ping_timer_;
    net::steady_timer reconnect_timer_;
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
    void on_ssl_handshake(beast::error_code ec);
    void on_ws_handshake(beast::error_code ec);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void do_ping();
};
