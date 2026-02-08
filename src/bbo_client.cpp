#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include "aggregator.pb.h"
#include <iostream>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using aggregator::AggregatorService;
using aggregator::SubscribeRequest;
using aggregator::BookUpdate;

class BBOClient {
public:
    BBOClient(std::shared_ptr<Channel> channel)
        : stub_(AggregatorService::NewStub(channel)) {}

    void Subscribe() {
        ClientContext context;
        SubscribeRequest request;
        request.set_symbol("BTCUSDT");  // 指定订阅的交易对

        std::unique_ptr<grpc::ClientReader<BookUpdate>> reader(
            stub_->SubscribeBook(&context, request));

        BookUpdate update;
        while (reader->Read(&update)) {
            if (update.bids().empty() || update.asks().empty()) {
                continue;
            }

            double best_bid_price = update.bids(0).price();
            double best_bid_qty   = update.bids(0).quantity();
            double best_ask_price = update.asks(0).price();
            double best_ask_qty   = update.asks(0).quantity();

            std::cout << "[BBO] " << update.timestamp_ms()
                      << " | Bid: " << best_bid_price << " (" << best_bid_qty << ")"
                      << " | Ask: " << best_ask_price << " (" << best_ask_qty << ")"
                      << std::endl;
        }

        Status status = reader->Finish();
        if (!status.ok()) {
            std::cerr << "Subscribe RPC failed: " << status.error_code() << ": "
                      << status.error_message() << std::endl;
        }
    }

private:
    std::unique_ptr<AggregatorService::Stub> stub_;
};

int main(int argc, char** argv) {
    auto channel = grpc::CreateChannel("localhost:50051",
                                       grpc::InsecureChannelCredentials());
    BBOClient client(channel);
    client.Subscribe();
    return 0;
}