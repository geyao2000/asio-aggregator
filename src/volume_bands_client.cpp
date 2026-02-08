#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include "aggregator.pb.h"
#include <iostream>
#include <vector>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using aggregator::AggregatorService;
using aggregator::SubscribeRequest;
using aggregator::BookUpdate;

class VolumeBandsClient {
public:
    VolumeBandsClient(std::shared_ptr<Channel> channel)
        : stub_(AggregatorService::NewStub(channel)) {}

    void Subscribe() {
        ClientContext context;
        SubscribeRequest request;
        request.set_symbol("BTCUSDT");

        std::unique_ptr<grpc::ClientReader<BookUpdate>> reader(
            stub_->SubscribeBook(&context, request));

        BookUpdate update;
        while (reader->Read(&update)) {
            if (update.bids().empty() || update.asks().empty()) continue;

            std::vector<double> bands = {1000000.0, 5000000.0, 10000000.0, 25000000.0, 50000000.0};

            // Bids (从最高价开始累计 notional)
            double cum_bid_notional = 0.0;
            double last_bid_price = 0.0;
            auto remaining_bands = bands;
            for (const auto& level : update.bids()) {
                cum_bid_notional += level.quantity() * level.price();
                last_bid_price = level.price();

                auto it = remaining_bands.begin();
                while (it != remaining_bands.end()) {
                    if (cum_bid_notional >= *it) {
                        std::cout << "[Volume Bands] Bid price for " << *it / 1e6 << "M USD: "
                                  << last_bid_price << std::endl;
                        it = remaining_bands.erase(it);
                    } else {
                        ++it;
                    }
                }
                if (remaining_bands.empty()) break;
            }

            // Asks (从最低价开始累计 notional)
            double cum_ask_notional = 0.0;
            double last_ask_price = 0.0;
            auto remaining_ask_bands = bands;
            for (const auto& level : update.asks()) {
                cum_ask_notional += level.quantity() * level.price();
                last_ask_price = level.price();

                auto it = remaining_ask_bands.begin();
                while (it != remaining_ask_bands.end()) {
                    if (cum_ask_notional >= *it) {
                        std::cout << "[Volume Bands] Ask price for " << *it / 1e6 << "M USD: "
                                  << last_ask_price << std::endl;
                        it = remaining_ask_bands.erase(it);
                    } else {
                        ++it;
                    }
                }
                if (remaining_ask_bands.empty()) break;
            }
        }

        Status status = reader->Finish();
        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
        }
    }

private:
    std::unique_ptr<AggregatorService::Stub> stub_;
};

int main(int argc, char** argv) {
    auto channel = grpc::CreateChannel("localhost:50051",
                                       grpc::InsecureChannelCredentials());
    VolumeBandsClient client(channel);
    client.Subscribe();
    return 0;
}