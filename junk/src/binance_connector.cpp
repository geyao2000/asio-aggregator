// #include "binance_connector.h"
// #include <iostream>

// BinanceConnector::BinanceConnector(net::io_context& ioc,
//                                    ssl::context& ssl_ctx,
//                                    const std::string& symbol)
//     : Connector(ioc, ssl_ctx, "stream.binance.com", "443", "Binance"),
//       symbol_(symbol) {}

// std::string BinanceConnector::build_subscribe_message() {
//     return R"({
//         "method": "SUBSCRIBE",
//         "params": ["btcusdt@depth20@100ms"],
//         "id": 1
//     })";
// }

// void BinanceConnector::on_message(const std::string& msg) {
//     std::cout << "[Binance] " << msg << "\n";
//     // TODO: parse JSON and forward to Aggregator
// }

#include "binance_connector.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

BinanceConnector::BinanceConnector(net::io_context& ioc,
                                   net::ssl::context& ctx,
                                   const std::string& symbol)
    : Connector(ioc, ctx, "/ws"), symbol_(symbol) {
    // 如果 Connector 有 target_，可以在这里设置 target_ = "/ws/" + symbol + "@ticker";
    // 或保持原样，在 connect 时处理
}

std::string BinanceConnector::build_subscribe_message() {
    json sub = {
        {"method", "SUBSCRIBE"},
        {"params", {symbol_ + "@ticker"}},
        {"id", 1}
    };
    return sub.dump();
}

void BinanceConnector::on_message(const std::string& msg) {
    try {
        auto j = json::parse(msg);
        if (j.contains("result") && j["result"].is_null()) {
            std::cout << "Subscription acknowledged\n";
        } else {
            std::cout << "Ticker update: " << msg << "\n";
            // 可以在这里解析价格等
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON error: " << e.what() << " msg=" << msg << "\n";
    }
}