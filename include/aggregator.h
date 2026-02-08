#pragma once
#include <iostream>

class Connector;

class Aggregator {
public:
    // 临时占位，后续在这里实现多交易所聚合逻辑
    void on_book_updated(Connector* conn) {
        // 验证阶段：打印收到的通知
        // std::cout << "[Aggregator] Update notified from: " << conn->name() << std::endl;
    }
};
