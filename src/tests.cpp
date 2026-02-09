#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include "binance_connector.h"
#include "okx_connector.h"
#include "bitget_connector.h"
#include "aggregator.h"  // 或 mock Aggregator
#include <nlohmann/json.hpp>
#include <map>

using json = nlohmann::json;

// Binance 解析测试
TEST_CASE("Binance parse snapshot", "[parser][binance]") {
    // ... 如上
    BinanceConnector connector(nullptr, "Binance", "host", "port", "path", nullptr);
    std::string msg = R"({
        "lastUpdateId": 123,
        "bids": [["70400.00", "1.5"], ["70390.00", "0.8"]],
        "asks": [["70410.00", "2.0"], ["70420.00", "1.2"]]
    })";

    connector.parse_message(msg);

    REQUIRE(connector.local_bids_.size() == 2);
    REQUIRE(connector.local_bids_[70400.0] == 1.5);
    REQUIRE(connector.local_bids_[70390.0] == 0.8);

    REQUIRE(connector.local_asks_.size() == 2);
    REQUIRE(connector.local_asks_[70410.0] == 2.0);
    REQUIRE(connector.local_asks_[70420.0] == 1.2);
}

// OKX 解析测试
TEST_CASE("OKX parse delta", "[parser][okx]") {
    // ... 如上
    OKXConnector connector(nullptr, "OKX", "host", "port", "path", nullptr);
    std::string msg = R"({
        "data": [{
            "bids": [["70400.00", "1.5"], ["70390.00", "0.0"]],  // 0.0 表示删除
            "asks": [["70410.00", "2.0"]]
        }]
    })";

    connector.parse_message(msg);

    // 假设之前有数据，这里验证更新后结果
    // REQUIRE(connector.local_bids_.count(70390.0) == 0); // 已删除
    REQUIRE(connector.local_bids_[70400.0] == 1.5);
}

// Aggregator 合并测试
TEST_CASE("Aggregator merges multiple books", "[aggregator]") {
    // ... 如上
    Aggregator agg(ioc);  // 需要 mock io_context 或简化

    // Mock 两个 connector
    auto conn1 = std::make_shared<BinanceConnector>(...);
    conn1->local_bids_ = {{70400.0, 1.0}, {70390.0, 2.0}};
    conn1->local_asks_ = {{70410.0, 3.0}};

    auto conn2 = std::make_shared<OKXConnector>(...);
    conn2->local_bids_ = {{70400.0, 1.5}, {70395.0, 0.5}};
    conn2->local_asks_ = {{70410.0, 1.0}, {70420.0, 2.0}};

    agg.connectors_ = {conn1, conn2};

    agg.update_consolidated_book(nullptr);  // 或 mock connector

    REQUIRE(agg.consolidated_bids_[70400.0] == 2.5);  // 1.0 + 1.5
    REQUIRE(agg.consolidated_bids_[70390.0] == 2.0);
    REQUIRE(agg.consolidated_bids_[70395.0] == 0.5);

    REQUIRE(agg.consolidated_asks_[70410.0] == 4.0);  // 3.0 + 1.0
}

// 边界：空订单簿
TEST_CASE("Aggregator empty book", "[aggregator][edge]") {
    // ... 如上
}

// 更多测试...