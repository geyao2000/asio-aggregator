#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include <iostream>
#include <vector>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using aggregator::AggregatorService;
using aggregator::Empty;
using aggregator::OrderBook;

class VolumeBandsClient {
public:
  VolumeBandsClient(std::shared_ptr<Channel> channel) : stub_(AggregatorService::NewStub(channel)) {}

  void Subscribe() {
    ClientContext context;
    Empty request;
    std::unique_ptr<grpc::ClientReader<OrderBook>> reader(stub_->SubscribeOrderBook(&context, request));
    OrderBook book;
    while (reader->Read(&book)) {
      if (book.bids().empty() || book.asks().empty()) continue;

      std::vector<double> bands = {1e6, 5e6, 10e6, 25e6, 50e6};

      // Bids
      double cum_bid = 0;
      double last_bid_price = 0;
      auto bid_bands = bands;
      for (const auto& entry : book.bids()) {
        cum_bid += entry.quantity() * entry.price();
        last_bid_price = entry.price();
        for (auto it = bid_bands.begin(); it != bid_bands.end(); ) {
          if (cum_bid >= *it) {
            std::cout << "Bid price for " << *it / 1e6 << "M USD: " << last_bid_price << std::endl;
            it = bid_bands.erase(it);
          } else {
            ++it;
          }
        }
        if (bid_bands.empty()) break;
      }

      // Asks
      double cum_ask = 0;
      double last_ask_price = 0;
      auto ask_bands = bands;
      for (const auto& entry : book.asks()) {
        cum_ask += entry.quantity() * entry.price();
        last_ask_price = entry.price();
        for (auto it = ask_bands.begin(); it != ask_bands.end(); ) {
          if (cum_ask >= *it) {
            std::cout << "Ask price for " << *it / 1e6 << "M USD: " << last_ask_price << std::endl;
            it = ask_bands.erase(it);
          } else {
            ++it;
          }
        }
        if (ask_bands.empty()) break;
      }
    }
    Status status = reader->Finish();
    if (!status.ok()) {
      std::cout << "Subscribe RPC failed" << std::endl;
    }
  }

private:
  std::unique_ptr<AggregatorService::Stub> stub_;
};

int main(int argc, char** argv) {
  VolumeBandsClient client(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
  client.Subscribe();
  return 0;
}