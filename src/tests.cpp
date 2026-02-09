#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../include/Aggregator.h"   // 调整路径
#include "../include/binance_connector.h"
#include "../include/okx_connector.h"
#include "../include/bitget_connector.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

TEST_CASE("Binance parse snapshot", "[parser][binance]") {
    // mock io_context（测试不需要真实运行 io）
    boost::asio::io_context mock_ioc;
    // mock Aggregator*（测试不需要 aggregator 回调，可以传 nullptr）
    Aggregator* mock_agg = nullptr;
    binance_connector connector(mock_ioc,mock_agg, "Binance", "host", "port", "path", nullptr);
    std::string msg = R"({
        "lastUpdateId": 123,
        "bids": [["70400.00", "1.5"], ["70390.00", "0.8"]],
        "asks": [["70410.00", "2.0"], ["70420.00", "1.2"]]
    })";

    connector.parse_message(msg);

    REQUIRE(connector.get_bids().size() == 2);
    REQUIRE(connector.get_bids()[70400.0] == Approx(1.5));
    REQUIRE(connector.get_bids()[70390.0] == Approx(0.8));

    REQUIRE(connector.get_asks().size() == 2);
    REQUIRE(connector.get_asks()[70410.0] == Approx(2.0));
    REQUIRE(connector.get_asks()[70420.0] == Approx(1.2));
}

TEST_CASE("OKX parse delta update", "[parser][okx]") {
    boost::asio::io_context mock_ioc;
    Aggregator* mock_agg = nullptr;
    okx_connector connector(mock_ioc,mock_agg, "OKX", "host", "port", "path", nullptr);
    std::string msg = R"({
        "data": [{
            "bids": [["70400.00", "1.5"], ["70390.00", "0.0"]],
            "asks": [["70410.00", "2.0"]]
        }]
    })";

    connector.parse_message(msg);

    REQUIRE(connector.get_bids().size() == 1);  // 0.0 被删除
    REQUIRE(connector.get_bids()[70400.0] == Approx(1.5));
}

TEST_CASE("Aggregator consolidates books", "[aggregator][consolidation]") {
    // 临时 mock io_context（实际测试中可简化）
    boost::asio::io_context ioc;
    Aggregator agg(ioc);

    auto conn1 = std::make_shared<binance_connector>(ioc, &agg, "Binance", "host", "port", "path", nullptr);
    conn1->get_bids() = {{70400.0, 1.0}, {70390.0, 2.0}};
    conn1->get_asks() = {{70410.0, 3.0}};

    auto conn2 = std::make_shared<okx_connector>(ioc, &agg, "OKX", "host", "port", "path", nullptr);
    conn2->get_bids() = {{70400.0, 1.5}, {70395.0, 0.5}};
    conn2->get_asks() = {{70410.0, 1.0}, {70420.0, 2.0}};

    agg.connectors_ = {conn1, conn2};  // 临时 public

    agg.update_consolidated_book(nullptr);

    REQUIRE(agg.consolidated_bids_[70400.0] == Approx(2.5));
    REQUIRE(agg.consolidated_bids_[70390.0] == Approx(2.0));
    REQUIRE(agg.consolidated_bids_[70395.0] == Approx(0.5));

    REQUIRE(agg.consolidated_asks_[70410.0] == Approx(4.0));
    REQUIRE(agg.consolidated_asks_[70420.0] == Approx(2.0));
}