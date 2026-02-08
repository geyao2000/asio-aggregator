#include "bitget_connector.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

// bitget_connector::bitget_connector(net::io_context& ioc, event_callback cb)
//     : market_connector(ioc,
//                        "Bitget",
//                        "ws.bitget.com",
//                        "443",
//                     //    "/spot/v1/stream",
//                        "/v2/ws/public",
//                        cb) {}
bitget_connector::bitget_connector(net::io_context& ioc, Aggregator* aggregator, std::string name, std::string host, std::string port, std::string path, event_callback cb)
    : market_connector(ioc, aggregator, name, host, port, path, cb) {}                       

std::string bitget_connector::subscription_message() const {
    return R"({
        "op":"subscribe",
        "args":[{"instType":"SPOT","channel":"books50","instId":"BTCUSDT"}]
    })";
}

void bitget_connector::handle_message(const std::string& msg) {
    // callback_("Bitget", msg);
    market_connector::handle_message(msg);
}

void bitget_connector::parse_message(const std::string& msg) {
  try {
    json j = json::parse(msg);
    if (j.contains("op") && j["op"] == "subscribe") return;

    if (j.contains("action") && j["action"] == "snapshot") {
      auto data = j["data"][0];
      local_bids_.clear();
      local_asks_.clear();

      for (const auto& bid : data["bids"]) {
        double price = std::stod(bid[0].get<std::string>());
        double qty = std::stod(bid[1].get<std::string>());
        local_bids_[price] = qty;
      }

      for (const auto& ask : data["asks"]) {
        double price = std::stod(ask[0].get<std::string>());
        double qty = std::stod(ask[1].get<std::string>());
        local_asks_[price] = qty;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "[" << name_ << "] Parse error: " << e.what() << std::endl;
  }
}