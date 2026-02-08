#include "aggregator.h"
#include "binance_connector.h"
#include "okx_connector.h"
#include "bitget_connector.h"
#include <iostream>

aggregator::aggregator(boost::asio::io_context& ioc) : ioc_(ioc) {}
aggregator::~aggregator() = default;

void aggregator::start() {
    connectors_.emplace_back(std::make_shared<binance_connector>(
        ioc_, [this](const std::string& ex, const std::string& msg) {
            on_market_event({ex, msg});
        }));

    connectors_.emplace_back(std::make_shared<okx_connector>(
        ioc_, [this](const std::string& ex, const std::string& msg) {
            on_market_event({ex, msg});
        }));

    connectors_.emplace_back(std::make_shared<bitget_connector>(
        ioc_, [this](const std::string& ex, const std::string& msg) {
            on_market_event({ex, msg});
        }));

    for (auto& c : connectors_) {
        c->start();
    }
}

void aggregator::on_market_event(const market_event& evt) {
    std::cout << "[" << evt.exchange << "] " << evt.message << std::endl;
}

