#include "bybit_connector.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

bybit_connector::bybit_connector(net::io_context& ioc,
                                 Aggregator* aggregator,
                                 std::string name,
                                 std::string host,
                                 std::string port,
                                 std::string path,
                                 event_callback cb)
    : market_connector(ioc, aggregator, name, host, port, path, cb) {}

std::string bybit_connector::subscription_message() const {
    // Bybit 现货 50 档深度，100ms 推送
    return R"({
        "op": "subscribe",
        "args": ["orderbook.50.BTCUSDT"]
    })";
}

void bybit_connector::handle_message(const std::string& msg) {
    // callback_("Bybit", msg);
    market_connector::handle_message(msg);
}

void bybit_connector::parse_message(const std::string& msg) {
    try {
        auto j = json::parse(msg);

        // 订阅确认
        if (j.contains("op") && j["op"] == "subscribe") {
            std::cout << "[Bybit] Subscription confirmed\n";
            return;
        }

        // 心跳 pong
        if (j.contains("op") && j["op"] == "pong") {
            return;
        }

        if (!j.contains("topic") || j["topic"] != "orderbook.50.BTCUSDT") {
            return;
        }

        const auto& data = j["data"];
        if (!data.is_object()) return;

        // std::lock_guard<std::mutex> lock(book_mutex_);

        // ===== SNAPSHOT =====
        if (j.contains("type") && j["type"] == "snapshot") {
            local_bids_.clear();
            local_asks_.clear();

            for (const auto& level : data["b"]) {
                double price = std::stod(level[0].get<std::string>());
                double qty   = std::stod(level[1].get<std::string>());
                if (qty > 0.0) local_bids_[price] = qty;
            }

            for (const auto& level : data["a"]) {
                double price = std::stod(level[0].get<std::string>());
                double qty   = std::stod(level[1].get<std::string>());
                if (qty > 0.0) local_asks_[price] = qty;
            }
        }
        // ===== DELTA =====
        else if (j.contains("type") && j["type"] == "delta") {
            if (data.contains("b")) {
                for (const auto& level : data["b"]) {
                    double price = std::stod(level[0].get<std::string>());
                    double qty   = std::stod(level[1].get<std::string>());
                    if (qty == 0.0)
                        local_bids_.erase(price);
                    else
                        local_bids_[price] = qty;
                }
            }

            if (data.contains("a")) {
                for (const auto& level : data["a"]) {
                    double price = std::stod(level[0].get<std::string>());
                    double qty   = std::stod(level[1].get<std::string>());
                    if (qty == 0.0)
                        local_asks_.erase(price);
                    else
                        local_asks_[price] = qty;
                }
            }
        }

        // if (aggregator_) {
        //     aggregator_->on_book_updated(this);
        // }

    } catch (const std::exception& e) {
        std::cerr << "[Bybit] Parse error: " << e.what() << "\n";
    }
}
