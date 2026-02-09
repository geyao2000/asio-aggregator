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
      ping_timer_(ioc),
      reconnect_timer_(ioc),
      handshake_timer_(ioc)
{
    std::cout << "[" << name_ << "] Initializing connector..." << std::endl;
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

    std::cout << "[" << name_ << "] SSL context configured (verify_peer + level 1)" << std::endl;
}

void market_connector::start() {
    std::cout << "[" << name_ << "] Starting connection to " << host_ << ":" << port_ << std::endl;
    
    stopped_ = false;
    // 重置所有状态
    beast::error_code ignore_ec;

    handshake_timer_.cancel(ignore_ec);
    reconnect_timer_.cancel(ignore_ec);
    ping_timer_.cancel(ignore_ec);
    buffer_.consume(buffer_.size());
    
    std::cout << "[" << name_ << "] Reset all WS state, buffer and timers" << std::endl;
    
    // 优雅关闭旧连接（异步）
    // ws_.async_close(websocket::close_code::normal, [](beast::error_code){});
    // std::cout<<"ws asyn close"<<std::endl;
    auto self = shared_from_this();
    resolver_.async_resolve(host_, port_,
        [self](beast::error_code ec, tcp::resolver::results_type results) {
            self->on_resolve(ec, results);
        });
    
    // retry_count_ = 0;
}

void market_connector::fail(const boost::system::error_code& ec, const char* what) {
    std::cerr << "[" << name_ << "] " << what << ": " << ec.message()
              << " (code: " << ec.value() << ")\n";

    if (stopped_) {
        std::cout << "[" << name_ << "] Already stopped, no reconnect\n";
        return;
    }

    stopped_ = true;

    beast::error_code ignore_ec;
    handshake_timer_.cancel(ignore_ec);
    ping_timer_.cancel(ignore_ec);

    // ❌ 不再同步 close socket / ws_
    // ✅ 让 Beast 自己通过错误传播关闭

    buffer_.consume(buffer_.size());

    bool should_reconnect =
        ec == net::error::connection_reset ||
        ec == net::error::connection_aborted ||
        ec == net::error::eof ||
        ec == net::error::timed_out ||
        ec == net::error::operation_aborted ||
        ec == websocket::error::closed ||
        ec.category() == net::ssl::error::get_stream_category();

    if (!should_reconnect) {
        std::cerr << "[" << name_ << "] Fatal error, no reconnect: " << ec.message() << "\n";
        return;
    }

    constexpr int MAX_RETRY = 10;
    constexpr int MAX_BACKOFF_MS = 30000;
    constexpr int INITIAL_BACKOFF_MS = 1000;
    constexpr double BACKOFF_MULTIPLIER = 2.0;

    retry_count_++;
    if (retry_count_ > MAX_RETRY) {
        std::cerr << "[" << name_ << "] Max retries reached, stopping reconnect.\n";
        return;
    }

    int backoff_ms = std::min(
        static_cast<int>(INITIAL_BACKOFF_MS * std::pow(BACKOFF_MULTIPLIER, retry_count_ - 1)),
        MAX_BACKOFF_MS
    );

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.8, 1.2);
    backoff_ms = static_cast<int>(backoff_ms * dis(gen));

    std::cout << "[" << name_ << "] Reconnecting in " << backoff_ms / 1000.0
              << " seconds... (attempt " << retry_count_ << "/" << MAX_RETRY << ")\n";

    auto self = shared_from_this();
    reconnect_timer_.expires_after(std::chrono::milliseconds(backoff_ms));
    reconnect_timer_.async_wait([self](beast::error_code timer_ec) {
        if (timer_ec) return;
        self->start();
    });
}


void market_connector::on_resolve(beast::error_code ec,
                                  tcp::resolver::results_type results) {
    if (ec) return fail(ec, "resolve");

    std::cout << "[" << name_ << "] DNS resolved successfully" << std::endl;
    beast::get_lowest_layer(ws_).async_connect(
        results,
        beast::bind_front_handler(&market_connector::on_connect, this));
}

void market_connector::on_connect(beast::error_code ec,
                                  tcp::resolver::results_type::endpoint_type ep) {
    if (ec) return fail(ec, "connect");

    std::cout << "[" << name_ << "] TCP connected to " << ep << std::endl;

    if(!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str()))
    {
        ec = beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
        return fail(ec, "set_tlsext_host_name");
    } 
    //above if deleted by grok, lead to bitget/OKX error                               
    std::cout << "[" << name_ << "] Starting SSL handshake..." << std::endl;
    ws_.next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&market_connector::on_ssl_handshake, this));
}

void market_connector::on_ssl_handshake(beast::error_code ec) {
    if (ec) return fail(ec, "ssl_handshake");

    std::cout << "[" << name_ << "] SSL handshake success" << std::endl;

    auto self = shared_from_this();

    handshake_timer_.expires_after(std::chrono::seconds(10));
    handshake_timer_.async_wait([self](beast::error_code t_ec) {
        if (t_ec != net::error::operation_aborted) {
            std::cerr << "[" << self->name_ << "] Handshake timeout\n";
            self->fail(net::error::timed_out, "handshake timeout");
        }
    });

    ws_.async_handshake(host_, path_,
        beast::bind_front_handler(&market_connector::on_ws_handshake, self));

}

void market_connector::on_ws_handshake(beast::error_code ec) {
    
    handshake_timer_.cancel();

    if (ec) {
        return fail(ec, "ws_handshake");
    }


    std::cout << "[" << name_ << "] WebSocket handshake success" << std::endl;

    auto msg = subscription_message();
    if (!msg.empty()) {
        std::cout << "[" << name_ << "] Sending subscription message: " << msg << std::endl;
        ws_.async_write(net::buffer(msg),
            beast::bind_front_handler(&market_connector::on_write, this));  // 修复: 替换 ...
    } else {
        std::cout << "[" << name_ << "] No subscription message needed, starting read..." << std::endl;
        do_read();  // Binance 无需订阅消息，直接读
    }
    std::cout << "[" << name_ << "] Starting ping loop..." << std::endl;
    do_ping();
}

void market_connector::on_write(beast::error_code ec, std::size_t bytes_transferred) {
    // if (ec) return fail(ec, "write");
    // std::cout << "[" << name_ << "] Subscription sent successfully (" << bytes_transferred << " bytes)" << std::endl;
    // do_read();
    if (ec) return fail(ec, "write");
    std::cout << "[" << name_ << "] Subscription sent successfully" << std::endl;
    do_read();
}

void market_connector::do_read() {
    // std::cout << "[" << name_ << "] Starting async read..." << std::endl;
    ws_.async_read(buffer_,
        beast::bind_front_handler(&market_connector::on_read, this));
}

void market_connector::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) return fail(ec, "read");

    std::string msg = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());

    // 判断是否是 pong 响应
    if (msg.find("pong") != std::string::npos ||
        msg.find("\"event\":\"pong\"") != std::string::npos ||
        msg.find("\"op\":\"pong\"") != std::string::npos ||
        msg == "pong" || msg == "{\"pong\":true}") {
        
        std::cout << "[" << name_ << "] Received pong response" << std::endl;
        do_read();
        return;
    }

    handle_message(msg);
    do_read();
}

void market_connector::do_ping() {
  if (stopped_) return;

//   std::cout << "[" << name_ << "] Preparing to send ping in 30s..." << std::endl;

  int ping_lantency = 15;
//   if (name_ == "Bitget") ping_lantency = 10;
  ping_timer_.expires_after(std::chrono::seconds(ping_lantency));
  ping_timer_.async_wait([this](beast::error_code ec) {
    if (stopped_ || ec) {
        std::cout << "[" << name_ << "] Ping timer canceled or stopped" << std::endl;
        return;
    }
    
    std::string ping_payload;

    if (name_ == "Binance") {
        // std::cout << "[" << name_ << "] Sending WebSocket ping..." << std::endl;
        // Binance 使用 WebSocket ping (空 payload 即可)
        ws_.async_ping(websocket::ping_data("keep-alive"), [this](beast::error_code ping_ec) {
            if (ping_ec) fail(ping_ec, "ping");
            else {
                std::cout << "[" << name_ << "] WebSocket ping sent" << std::endl;
                do_ping();
            }
        });
        return;
    } else if (name_ == "OKX" || name_ == "Bitget") {
        // OKX / Bitget 使用 JSON ping
        ws_.text(true);
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

    std::cout << "[" << name_ << "] No ping configured for this exchange, continuing timer" << std::endl;
    do_ping();


  });
}

void market_connector::handle_message(const std::string& msg) {
//   std::cout << "[" << name_ << "] Received market update (length: " << msg.length() << " bytes)" << std::endl;
  
  callback_(name_, msg);  // Keep printing raw
  parse_message(msg);  // Parse and update book
  if (aggregator_) aggregator_->on_book_updated(this);  // Notify
}
