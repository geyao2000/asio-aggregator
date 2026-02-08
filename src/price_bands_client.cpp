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

class PriceBandsClient {
public:
    PriceBandsClient(std::shared_ptr<Channel> channel)
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

            double best_bid = update.bids(0).price();
            double best_ask = update.asks(0).price();

            std::vector<int> bps_levels = {50, 100, 200, 500, 1000};

            // Bid side (向下扩展)
            for (int bps : bps_levels) {
                double threshold = best_bid * (1.0 - bps / 10000.0);
                double cum_vol = 0.0;
                for (const auto& level : update.bids()) {
                    if (level.price() >= threshold) {
                        cum_vol += level.quantity();
                    } else {
                        break;
                    }
                }
                std::cout << "[Price Bands] Bid volume at -" << bps << "bps from BBO: "
                          << cum_vol << std::endl;
            }

            // Ask side (向上扩展)
            for (int bps : bps_levels) {
                double threshold = best_ask * (1.0 + bps / 10000.0);
                double cum_vol = 0.0;
                for (const auto& level : update.asks()) {
                    if (level.price() <= threshold) {
                        cum_vol += level.quantity();
                    } else {
                        break;
                    }
                }
                std::cout << "[Price Bands] Ask volume at +" << bps << "bps from BBO: "
                          << cum_vol << std::endl;
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
    PriceBandsClient client(channel);
    client.Subscribe();
    return 0;
}