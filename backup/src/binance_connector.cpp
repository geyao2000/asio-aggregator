#include "binance_connector.h"

binance_connector::binance_connector(net::io_context& ioc, event_callback cb)
    : market_connector(ioc,
                       "Binance",
                       "stream.binance.com",
                       "9443",
                       "/ws/btcusdt@depth20@100ms",
                       cb) {}

std::string binance_connector::subscription_message() const {
    // Binance auto-subscribes via URL path, no message needed
    return "";
}

void binance_connector::handle_message(const std::string& msg) {
    callback_("Binance", msg);
}
