#include "bitget_connector.h"

bitget_connector::bitget_connector(net::io_context& ioc, event_callback cb)
    : market_connector(ioc,
                       "Bitget",
                       "ws.bitget.com",
                       "443",
                    //    "/spot/v1/stream",
                       "/v2/ws/public",
                       cb) {}

std::string bitget_connector::subscription_message() const {
    return R"({
        "op":"subscribe",
        "args":[{"instType":"SPOT","channel":"books50","instId":"BTCUSDT"}]
    })";
}

void bitget_connector::handle_message(const std::string& msg) {
    callback_("Bitget", msg);
}
