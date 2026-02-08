#include "binance_connector.h"
#include <iostream>

using json = nlohmann::json;

void BinanceConnector::parse_message(const std::string& msg) {
    std::string trimmed = msg;
    // 去除空白字符
    trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(), ::isspace), trimmed.end());

    if (trimmed == "pong" ||
        trimmed == "{\"pong\":true}" ||
        trimmed == "{\"event\":\"pong\"}" ||
        trimmed.rfind("pong", 0) == 0) {  // 以 "pong" 开头兜底
        std::cout << "[" << name_ << "] Ignored pong response" << std::endl;
        return;  // 直接返回，不解析
    }

    try {
        // std::cout << "[Binance Debug] Raw message: " << msg << std::endl;  // 可选：调试时打开

        auto j = json::parse(msg);

        // Binance Depth Stream 数据格式示例:
        // {
        //   "lastUpdateId": 160,
        //   "bids": [ [ "0.0024", "10" ] ],
        //   "asks": [ [ "0.0026", "100" ] ]
        // }
        
        if (j.contains("bids") && j.contains("asks")) {
            // 解析 Bids (买单)
            // 注意：Binance 返回的是字符串数组 ["price", "quantity"]
            for (const auto& bid : j["bids"]) {
                double price = std::stod(bid[0].get<std::string>());
                double qty = std::stod(bid[1].get<std::string>());
                
                if (qty == 0) {
                    local_bids_.erase(price);
                } else {
                    local_bids_[price] = qty;
                }
            }

            // 解析 Asks (卖单)
            for (const auto& ask : j["asks"]) {
                double price = std::stod(ask[0].get<std::string>());
                double qty = std::stod(ask[1].get<std::string>());
                
                if (qty == 0) {
                    local_asks_.erase(price);
                } else {
                    local_asks_[price] = qty;
                }
            }

            // 打印简易日志（可选，生产环境建议移除）
            // std::cout << "[Binance] Top Bid: " << local_bids_.rbegin()->first << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Binance] JSON Parse Error: " << e.what() << " | Raw: " << msg.substr(0, 100) << std::endl;
    }
}
