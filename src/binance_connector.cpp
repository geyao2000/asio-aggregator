#include "binance_connector.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

// binance_connector::binance_connector(net::io_context& ioc, event_callback cb)
//     : market_connector(ioc,
//                        "Binance",
//                        "stream.binance.com",
//                        "9443",
//                        "/ws/btcusdt@depth20@100ms",
//                        cb) {}

binance_connector::binance_connector(net::io_context& ioc, Aggregator* aggregator, std::string name, std::string host, std::string port, std::string path, event_callback cb)
    : market_connector(ioc, aggregator, name, host, port, path, cb) {}        

std::string binance_connector::subscription_message() const {
    // Binance auto-subscribes via URL path, no message needed
    return "";
}

void binance_connector::handle_message(const std::string& msg) {
    // callback_("Binance", msg);
    market_connector::handle_message(msg);  // Call base
}

void binance_connector::parse_message(const std::string& msg) {
  try {
    json j = json::parse(msg);
    if (j.contains("lastUpdateId")) {
      local_bids_.clear();
      local_asks_.clear();

      for (const auto& bid : j["bids"]) {
        double price = std::stod(bid[0].get<std::string>());
        double qty = std::stod(bid[1].get<std::string>());
        local_bids_[price] = qty;
      }

      for (const auto& ask : j["asks"]) {
        double price = std::stod(ask[0].get<std::string>());
        double qty = std::stod(ask[1].get<std::string>());
        local_asks_[price] = qty;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "[" << name_ << "] Parse error: " << e.what() << std::endl;
  }
}