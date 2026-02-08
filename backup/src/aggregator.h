#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <string>

struct market_event {
    std::string exchange;
    std::string message;
};

class market_connector;

class aggregator {
public:
    explicit aggregator(boost::asio::io_context& ioc);
    ~aggregator();
    void start();

private:
    void on_market_event(const market_event& evt);

    boost::asio::io_context& ioc_;
    // std::vector<std::unique_ptr<market_connector>> connectors_;
    std::vector<std::shared_ptr<market_connector>> connectors_;
};
