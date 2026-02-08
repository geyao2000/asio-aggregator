#pragma once
#include "market_connector.h"

class Aggregator;  // 前向声明

class binance_connector : public market_connector {
public:
    // binance_connector(net::io_context& ioc, event_callback cb);
    // 与基类签名一致 + Aggregator*
    binance_connector(net::io_context& ioc,
                      Aggregator* aggregator,
                      std::string name,
                      std::string host,
                      std::string port,
                      std::string path,
                      event_callback cb);

protected:
    std::string subscription_message() const override;
    void handle_message(const std::string& msg) override;
    // 必须声明 override（基类有纯虚函数）
    void parse_message(const std::string& msg) override;
};
