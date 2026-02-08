#pragma once
#include "market_connector.h"

class okx_connector : public market_connector {
public:
    okx_connector(net::io_context& ioc, event_callback cb);

protected:
    std::string subscription_message() const override;
    void handle_message(const std::string& msg) override;
};
