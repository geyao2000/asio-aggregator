#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>   // ← important for SSL websocket support
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;
using json = nlohmann::json;

// Base class
class Connector {
public:  // temporarily public for simplicity; can make protected + add run()
    net::io_context ioc_;
protected:
    ssl::context ssl_ctx_{ssl::context::tlsv12_client};
    std::shared_ptr<websocket::stream<ssl::stream<beast::tcp_stream>>> ws_;
    std::string host_;
    std::string port_;
    std::string target_;

    virtual void on_open() {}
    virtual void on_message(const std::string& message) {}
    virtual void on_close() {}
    virtual void on_error(beast::error_code ec) {}

public:
    Connector(std::string host, std::string port, std::string target = "/")
        : host_(std::move(host)), port_(std::move(port)), target_(std::move(target)) {
        ssl_ctx_.set_verify_mode(ssl::verify_peer);
        ssl_ctx_.set_default_verify_paths();
    }

    virtual ~Connector() {
        close();
    }

    void connect() {
        try {
            tcp::resolver resolver(ioc_);
            auto results = resolver.resolve(host_, port_);

            ws_ = std::make_shared<websocket::stream<ssl::stream<beast::tcp_stream>>>(
                ioc_, ssl_ctx_);

            // Connect TCP + perform SSL handshake
            get_lowest_layer(*ws_).connect(results);
            ws_->next_layer().handshake(ssl::stream_base::client);

            // WebSocket handshake
            ws_->handshake(host_, target_);

            on_open();
            read();
        } catch (const std::exception& e) {
            std::cerr << "Connect failed: " << e.what() << "\n";
            on_error({});
        }
    }

    void send(const std::string& message) {
        if (ws_ && ws_->is_open()) {
            ws_->write(net::buffer(message));
        }
    }

    void close() {
        beast::error_code ec;
        if (ws_ && ws_->is_open()) {
            ws_->close(websocket::close_code::normal, ec);
            if (ec) std::cerr << "Close WS: " << ec.message() << "\n";
        }
        on_close();
    }

    net::io_context& io_context() { return ioc_; }  // helper to run from outside

private:
    void read() {
        ws_->async_read(buffer_,
            [this](beast::error_code ec, std::size_t) {
                if (ec == websocket::error::closed) {
                    on_close();
                    return;
                }
                if (ec) {
                    on_error(ec);
                    close();
                    return;
                }

                on_message(beast::buffers_to_string(buffer_.data()));
                buffer_.consume(buffer_.size());
                read();
            });
    }

    beast::flat_buffer buffer_;
};

// Binance subclass
class BinanceConnector : public Connector {
    std::string symbol_;

public:
    BinanceConnector(std::string symbol = "btcusdt")
        : Connector("stream.binance.com", "9443", "/ws/" + symbol + "@ticker")
        , symbol_(std::move(symbol)) {}

    void on_open() override {
        json sub = {
            {"method", "SUBSCRIBE"},
            {"params", {symbol_ + "@ticker"}},
            {"id", 1}
        };
        send(sub.dump());
        std::cout << "Subscribed to " << symbol_ << "@ticker\n";
    }

    void on_message(const std::string& msg) override {
        try {
            auto j = json::parse(msg);
            if (j.contains("result") && j["result"].is_null()) {
                std::cout << "Subscription ok\n";
            } else {
                std::cout << "Market data: " << msg << "\n";
            }
        } catch (...) {
            std::cerr << "Bad JSON: " << msg << "\n";
        }
    }

    void on_close() override {
        std::cout << "Connection closed.\n";
    }

    void on_error(beast::error_code ec) override {
        std::cerr << "Error: " << ec.message() << "\n";
    }
};

int main() {
    try {
        BinanceConnector conn("btcusdt");
        conn.connect();

        // Run the event loop
        conn.io_context().run();
    } catch (const std::exception& e) {
        std::cerr << "Main error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}