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

class PriceBandsClient {
public:
  PriceBandsClient(std::shared_ptr<Channel> channel) : stub_(AggregatorService::NewStub(channel)) {}

  void Subscribe() {
    ClientContext context;
    Empty request;
    std::unique_ptr<grpc::ClientReader<OrderBook>> reader(stub_->SubscribeOrderBook(&context, request));
    OrderBook book;
    while (reader->Read(&book)) {
      if (book.bids().empty() || book.asks().empty()) continue;

      std::vector<int> bps_levels = {50, 100, 200, 500, 1000};

      double best_bid = book.bids(0).price();
      double best_ask = book.asks(0).price();

      // Bids
      for (int bps : bps_levels) {
        double threshold = best_bid * (1 - bps / 10000.0);
        double cum_vol = 0;
        for (const auto& entry : book.bids()) {
          if (entry.price() >= threshold) {
            cum_vol += entry.quantity();
          } else {
            break;
          }
        }
        std::cout << "Bid volume at -" << bps << "bps: " << cum_vol << std::endl;
      }

      // Asks
      for (int bps : bps_levels) {
        double threshold = best_ask * (1 + bps / 10000.0);
        double cum_vol = 0;
        for (const auto& entry : book.asks()) {
          if (entry.price() <= threshold) {
            cum_vol += entry.quantity();
          } else {
            break;
          }
        }
        std::cout << "Ask volume at +" << bps << "bps: " << cum_vol << std::endl;
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
  PriceBandsClient client(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
  client.Subscribe();
  return 0;
}