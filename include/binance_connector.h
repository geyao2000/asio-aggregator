// #pragma once
// #include "connector.h"
// #include <nlohmann/json.hpp>

// class BinanceConnector : public Connector {
// public:
//     // 使用父类构造函数
//     using Connector::Connector;

// protected:
//     // --- 1. 网络配置 ---
//     std::string host() override { return "stream.binance.com"; }
//     std::string port() override { return "443"; }
    
//     // 订阅 BTCUSDT 的 Top 20 层深度，100ms 更新频率
//     std::string path() override { return "/ws"; }
//     // std::string path() override { return "/ws/btcusdt@depth20@100ms"; }
   
//     // Binance 的路径直接包含了流名称，通常握手后即自动订阅，
//     // 这里返回空或特定的 subscribe 指令（如果需要手动订阅更多）
//     std::string subscribe_message() override {
//         return R"({"method":"SUBSCRIBE","params":["btcusdt@depth20@100ms"],"id":1})";
//         // return "";
//     }

//     // Binance WebSocket 默认 3 分钟内无消息才需 ping，Asio 基类已处理
//     bool needs_ping() override { return true; }

//     // --- 2. 核心逻辑：数据解析 ---
//     void parse_message(const std::string& msg) override;
// };

#pragma once
#include "connector.h"
#include <nlohmann/json.hpp>

class BinanceConnector : public Connector {
public:
    explicit BinanceConnector(Aggregator* aggregator, const std::string& name = "Binance")
        : Connector(aggregator, name) {}  // 显式调用父类构造函数

protected:
    std::string host() const override { return "stream.binance.com"; }
    std::string port() const override { return "443"; }
    std::string path() const override { return "/ws"; }  // 修正为 /ws
    std::string subscribe_message() const override {
        return R"({"method":"SUBSCRIBE","params":["btcusdt@depth20@100ms"],"id":1})";
    }
    bool needs_ping() const override { return true; }

    void parse_message(const std::string& msg) override;

    // 允许测试访问 protected 成员（可选，推荐）
    friend class BinanceConnectorTest_ParseSnapshot_Test;
    friend class BinanceConnectorTest_ParseUpdateAdd_Test;
    friend class BinanceConnectorTest_InvalidJSON_Test;
};
