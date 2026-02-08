#include "connector.h"
#include "aggregator.h"
#include <iostream>
#include <thread>  // for sleep_for

using namespace std;

// Connector::Connector(net::io_context& ioc, ssl::context& ssl_ctx, Aggregator* agg, string name)
//     : ioc_(ioc), strand_(net::make_strand(ioc)), ssl_ctx_(ssl_ctx),
//       aggregator_(agg), name_(std::move(name)), resolver_(ioc), timer_(ioc) {}

// void Connector::start() {
//     cout << __FUNCTION__ << endl;
//     net::post(strand_, beast::bind_front_handler(&Connector::do_resolve, shared_from_this()));
// }
Connector::Connector(net::io_context& ioc, ssl::context& ssl_ctx, Aggregator* agg, string name)
    : ioc_(ioc), strand_(net::make_strand(ioc)), ssl_ctx_(ssl_ctx),
      aggregator_(agg), name_(std::move(name)), resolver_(ioc), timer_(ioc),
      ws_(std::make_unique<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(ioc_, ssl_ctx)) {}

void Connector::do_resolve() {
    cout << __FUNCTION__ << endl;
    resolver_.async_resolve(host(), port(),
        net::bind_executor(strand_, beast::bind_front_handler(&Connector::on_resolve, shared_from_this())));
}

void Connector::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        cerr << "[" << name_ << "] Resolve error: " << ec.message() << endl;
        schedule_reconnect();
        return;
    }

    cout << __FUNCTION__ << endl;

    // 直接创建 tcp_stream
    beast::tcp_stream stream(ioc_);
    stream.async_connect(results,
        net::bind_executor(strand_, beast::bind_front_handler(&Connector::on_connect, shared_from_this())));
}

void Connector::on_connect(beast::error_code ec, tcp::resolver::endpoint_type ep) {
    if (ec) {
        cerr << "[" << name_ << "] Connect error: " << ec.message() << endl;
        schedule_reconnect();
        return;
    }

    cout << __FUNCTION__ << endl;

    // 创建 websocket stream
    // ws_ = make_unique<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(ioc_, ssl_ctx_);

    beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));
    ws_->next_layer().async_handshake(ssl::stream_base::client,
        net::bind_executor(strand_, beast::bind_front_handler(&Connector::on_ssl_handshake, shared_from_this())));
}

void Connector::on_ssl_handshake(beast::error_code ec) {
    if (ec) {
        cerr << "[" << name_ << "] SSL handshake error: " << ec.message() << endl;
        schedule_reconnect();
        return;
    }

    cout << __FUNCTION__ << endl;

    beast::get_lowest_layer(*ws_).expires_never();

    ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws_->set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
        req.set(beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl");
    }));

    std::string ws_path = path();
    ws_->async_handshake(host(), ws_path,
        net::bind_executor(strand_, beast::bind_front_handler(&Connector::on_ws_handshake, shared_from_this())));
}

void Connector::on_ws_handshake(beast::error_code ec) {
    if (ec) {
        cerr << "[" << name_ << "] WS handshake error: " << ec.message() << endl;
        schedule_reconnect();
        return;
    }

    cout << __FUNCTION__ << endl;

    std::string subscribe_msg = subscribe_message();
    if (!subscribe_msg.empty()) {
        ws_->async_write(net::buffer(subscribe_msg),
            net::bind_executor(strand_, beast::bind_front_handler(&Connector::on_subscribe_write, shared_from_this())));
    } else {
        do_read();  // 直接读
    }

    run_ping_timer();  // 启动心跳
}

void Connector::on_subscribe_write(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        cerr << "[" << name_ << "] Subscribe write error: " << ec.message() << endl;
        schedule_reconnect();
        return;
    }

    cout << "[" << name_ << "] Subscribed." << endl;
    do_read();
}

void Connector::do_read() {
    buffer_.consume(buffer_.size());  // 清空旧数据
    ws_->async_read(buffer_,
        net::bind_executor(strand_, beast::bind_front_handler(&Connector::on_read, shared_from_this())));
}

void Connector::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        cerr << "[" << name_ << "] Read error: " << ec.message() << endl;
        schedule_reconnect();
        return;
    }

    std::string msg = beast::buffers_to_string(buffer_.data());
    parse_message(msg);

    // 通知 aggregator
    if (aggregator_) {
        aggregator_->on_book_updated(this);
    }

    do_read();  // 继续读
}

// void Connector::run_ping_timer() {
//     timer_.expires_after(std::chrono::minutes(2));  // Binance 推荐 3min 内 ping
//     timer_.async_wait(net::bind_executor(strand_, [this, self = shared_from_this()](beast::error_code ec) {
//         if (ec) return;

//         ws_->text(true);  // 确保文本模式
//         ws_->async_ping(websocket::ping_data{},
//             net::bind_executor(strand_, [this, self = shared_from_this()](beast::error_code ec) {
//                 if (ec) {
//                     cerr << "[" << name_ << "] Ping error: " << ec.message() << endl;
//                     schedule_reconnect();
//                     return;
//                 }

//                 cout << "[" << name_ << "] Ping sent" << endl;
//                 run_ping_timer();  // 递归下一个
//             }));
//     }));
// }
void Connector::run_ping_timer() {
    timer_.expires_after(std::chrono::minutes(2));
    timer_.async_wait(net::bind_executor(strand_, [this, self = shared_from_this()](beast::error_code ec) {
        if (ec) return;

        ws_->text(true);  // 确保文本模式
        ws_->async_write(net::buffer("ping"),
            net::bind_executor(strand_, [this, self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (ec) {
                    cerr << "[" << name_ << "] Ping error: " << ec.message() << endl;
                    schedule_reconnect();
                    return;
                }

                cout << "[" << name_ << "] Ping sent" << endl;
                run_ping_timer();
            }));
    }));
}

void Connector::schedule_reconnect() {
    cout << "[" << name_ << "] Error/Disconnect. Reconnecting in 5s..." << endl;

    if (ws_) {
        beast::error_code ec;
        ws_->close(websocket::close_code::going_away, ec);
    }

    timer_.expires_after(std::chrono::seconds(5));
    timer_.async_wait(net::bind_executor(strand_, [this, self = shared_from_this()](beast::error_code) {
        do_resolve();
    }));
}

BidsMap Connector::get_bids_snapshot() {
    return local_bids_;
}