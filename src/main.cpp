#include "binance_connector.h"
#include "aggregator.h"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context ioc;
        boost::asio::ssl::context ctx{boost::asio::ssl::context::tlsv12_client};
        ctx.set_default_verify_paths();

        // 1. 先创建 Aggregator 实例
        auto agg = std::make_unique<Aggregator>();

        // 2. 创建并启动 Binance 连接器
        // 传入 agg.get() 指针，满足 Connector 构造函数需求
        auto binance = std::make_shared<BinanceConnector>(ioc, ctx, agg.get(), "Binance");

        std::cout << "[Main] Starting Asio Event Loop..." << std::endl;
        binance->start();

        // 3. 进入事件循环，处理所有异步网络 IO
        ioc.run();

    } catch (const std::exception& e) {
        std::cerr << "[Critical] Error: " << e.what() << std::endl;
    }
    return 0;
}
