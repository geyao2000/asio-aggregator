#pragma once
#include "connector.h"  // 假设 Connector 在这里
#include <string>

class BinanceConnector : public Connector {
public:
    BinanceConnector(net::io_context& ioc,
                     net::ssl::context& ctx,
                     const std::string& symbol);

    std::string build_subscribe_message();  // ← 新增声明

protected:
    void on_message(const std::string& msg) override;
    // 其他 virtual 如 on_open, on_close, on_error 如果有也声明在这里

private:
    std::string symbol_;
};