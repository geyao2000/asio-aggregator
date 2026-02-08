#include "okx_connector.h"

okx_connector::okx_connector(net::io_context& ioc, event_callback cb)
    : market_connector(ioc,
                       "OKX",
                       "ws.okx.com",
                       "8443",
                       "/ws/v5/public",
                       cb) {}

std::string okx_connector::subscription_message() const {
    return R"({
        "op":"subscribe",
        "args":[{"channel":"books5","instId":"BTC-USDT"}]
    })";
}

void okx_connector::handle_message(const std::string& msg) {
    callback_("OKX", msg);
}
