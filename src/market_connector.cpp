#include "market_connector.h"
#include <iostream>
#include "Aggregator.h"  // For Aggregator*

market_connector::market_connector(net::io_context& ioc, Aggregator* aggregator, std::string name,
                                   std::string host, std::string port, std::string path, event_callback cb)
    : ioc_(ioc),
      aggregator_(aggregator),
      name_(std::move(name)),
      host_(std::move(host)),
      port_(std::move(port)),
      path_(std::move(path)),
      callback_(std::move(cb)),
      resolver_(ioc),
      ssl_ctx_(ssl::context::tls_client),
      ws_(ioc, ssl_ctx_),
      ping_timer_(ioc)
{
    ssl_ctx_.set_options(
        ssl::context::default_workarounds |
        ssl::context::no_sslv2 |
        ssl::context::no_sslv3 |
        ssl::context::single_dh_use
    );

    ssl_ctx_.set_default_verify_paths();

    SSL_CTX_set_security_level(ssl_ctx_.native_handle(), 1);  // 降到 level 1
    
    // ssl_ctx_.set_verify_mode(ssl::verify_none);
    
    ssl_ctx_.set_verify_mode(ssl::verify_peer);
}

void market_connector::start() {
    resolver_.async_resolve(host_, port_,
        beast::bind_front_handler(&market_connector::on_resolve, this));
}

void market_connector::fail(const beast::error_code& ec, const char* what) {
    std::cerr << "[" << name_ << "] " << what << ": " << ec.message() << "\n";
}

void market_connector::on_resolve(beast::error_code ec,
                                  tcp::resolver::results_type results) {
    if (ec) return fail(ec, "resolve");

    beast::get_lowest_layer(ws_).async_connect(
        results,
        beast::bind_front_handler(&market_connector::on_connect, this));
}

void market_connector::on_connect(beast::error_code ec,
                                  tcp::resolver::results_type::endpoint_type ep) {
    if (ec) return fail(ec, "connect");

    if(!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str()))
    {
        ec = beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
        return fail(ec, "set_tlsext_host_name");
    } 
    //above if deleted by grok, lead to bitget/OKX error                               

    ws_.next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&market_connector::on_ssl_handshake, this));
}

void market_connector::on_ssl_handshake(beast::error_code ec) {
    if (ec) return fail(ec, "ssl_handshake");

    ws_.async_handshake(host_, path_,
        beast::bind_front_handler(&market_connector::on_ws_handshake, this));
}

void market_connector::on_ws_handshake(beast::error_code ec) {
    if (ec) return fail(ec, "ws_handshake");

    auto msg = subscription_message();
    if (!msg.empty()) {
        ws_.async_write(net::buffer(msg),
            beast::bind_front_handler(&market_connector::on_write, this));  // 修复: 替换 ...
    } else {
        do_read();  // Binance 无需订阅消息，直接读
    }
    do_ping();
}

void market_connector::on_write(beast::error_code ec, std::size_t) {
    if (ec) return fail(ec, "write");
    do_read();
}

void market_connector::do_read() {
    ws_.async_read(buffer_,
        beast::bind_front_handler(&market_connector::on_read, this));
}

void market_connector::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) return fail(ec, "read");

    std::string msg = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());

    handle_message(msg);
    do_read();
}

void market_connector::do_ping() {
  if (stopped_) return;

  ping_timer_.expires_after(std::chrono::seconds(30));
  ping_timer_.async_wait([this](beast::error_code ec) {
    if (stopped_ || ec) return;
    
    std::string ping_payload;

    if (name_ == "Binance") {
        // Binance 使用 WebSocket ping (空 payload 即可)
        ws_.async_ping(websocket::ping_data("keep-alive"), [this](beast::error_code ping_ec) {
            if (ping_ec) fail(ping_ec, "ping");
            else do_ping();
        });
        return;
    } else if (name_ == "OKX" || name_ == "Bitget") {
        // OKX / Bitget 使用 JSON ping
        ping_payload = R"({"op": "ping"})";
        ws_.async_write(net::buffer(ping_payload), [this](beast::error_code write_ec, std::size_t) {
            if (write_ec) {
                fail(write_ec, "json ping write");
            } else {
                std::cout << "[" << name_ << "] Sent JSON ping" << std::endl;
                do_ping();
            }
        });
        return;
    }
    do_ping();


  });
}

void market_connector::handle_message(const std::string& msg) {
  callback_(name_, msg);  // Keep printing raw
  parse_message(msg);  // Parse and update book
  if (aggregator_) aggregator_->on_book_updated(this);  // Notify
}
