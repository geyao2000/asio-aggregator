#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include "aggregator.pb.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>      // 用于格式化输出
#include <sstream>      // 用于字符串构建

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using aggregator::AggregatorService;
using aggregator::SubscribeRequest;
using aggregator::BookUpdate;

class VolumeBandsClient {
public:
    VolumeBandsClient(const std::string& target) : target_(target) {}

    void Run() {
        constexpr int MAX_BACKOFF_MS = 30000;
        constexpr int INITIAL_BACKOFF_MS = 1000;
        constexpr double BACKOFF_MULTIPLIER = 2.0;

        int retry_count = 0;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.8, 1.2);

        while (true) {
            std::shared_ptr<Channel> channel = grpc::CreateChannel(
                target_, grpc::InsecureChannelCredentials());

            std::unique_ptr<AggregatorService::Stub> stub(AggregatorService::NewStub(channel));

            ClientContext context;
            SubscribeRequest request;
            request.set_symbol("BTCUSDT");

            std::unique_ptr<grpc::ClientReader<BookUpdate>> reader(
                stub->SubscribeBook(&context, request));

            std::cout << "[VolumeBands] Connected to " << target_ << ", subscribing to BTCUSDT..." << std::endl;

            BookUpdate update;
            bool connected = true;

            while (reader->Read(&update)) {
                if (update.bids().empty() || update.asks().empty()) {
                    continue;
                }

                std::vector<double> bands = {0.01e6, 0.10e6, 1.00e6, 5.00e6, 10.00e6, 25.00e6, 50.00e6};

                // 获取当前本地时间
                auto now = std::chrono::system_clock::now();
                auto now_time_t = std::chrono::system_clock::to_time_t(now);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()) % 1000;

                std::ostringstream oss_time;
                oss_time << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S")
                         << "." << std::setfill('0') << std::setw(3) << ms.count();

                std::cout << "=== Volume Bands Update @ " << oss_time.str() << " JST ===\n"
                          << "Volume Bands:\n";

                // Bid side
                double cum_bid_notional = 0.0;
                double last_bid_price = 0.0;
                size_t band_idx = 0;
                for (const auto& level : update.bids()) {
                    cum_bid_notional += level.quantity() * level.price();
                    last_bid_price = level.price();

                    while (band_idx < bands.size() && cum_bid_notional >= bands[band_idx]) {
                        std::cout << "Bid " << std::fixed << std::setprecision(2) << bands[band_idx] / 1e6
                                  << "M USD @ " << std::fixed << std::setprecision(2) << last_bid_price
                                  << " (cum: " << std::fixed << std::setprecision(2) << cum_bid_notional << ")\n";
                        ++band_idx;
                    }
                }

                // 未达到的 band
                while (band_idx < bands.size()) {
                    std::cout << "Bid " << std::fixed << std::setprecision(2) << bands[band_idx] / 1e6
                              << "M USD: not reached (cum: " << std::fixed << std::setprecision(2) << cum_bid_notional
                              << ", nearest @ " << std::fixed << std::setprecision(2) << last_bid_price << ")\n";
                    ++band_idx;
                }

                // Ask side
                double cum_ask_notional = 0.0;
                double last_ask_price = 0.0;
                band_idx = 0;
                for (const auto& level : update.asks()) {
                    cum_ask_notional += level.quantity() * level.price();
                    last_ask_price = level.price();

                    while (band_idx < bands.size() && cum_ask_notional >= bands[band_idx]) {
                        std::cout << "Ask " << std::fixed << std::setprecision(2) << bands[band_idx] / 1e6
                                  << "M USD @ " << std::fixed << std::setprecision(2) << last_ask_price
                                  << " (cum: " << std::fixed << std::setprecision(2) << cum_ask_notional << ")\n";
                        ++band_idx;
                    }
                }

                while (band_idx < bands.size()) {
                    std::cout << "Ask " << std::fixed << std::setprecision(2) << bands[band_idx] / 1e6
                              << "M USD: not reached (cum: " << std::fixed << std::setprecision(2) << cum_ask_notional
                              << ", nearest @ " << std::fixed << std::setprecision(2) << last_ask_price << ")\n";
                    ++band_idx;
                }

                std::cout << "=========================================================\n";

                retry_count = 0;
            }

            Status status = reader->Finish();
            if (status.ok()) {
                std::cout << "[VolumeBands] Stream completed normally." << std::endl;
            } else {
                std::cerr << "[VolumeBands] RPC failed: " << status.error_code()
                          << ": " << status.error_message() << std::endl;
            }

            // 断线重连逻辑
            connected = false;
            retry_count++;

            int backoff_ms = std::min(
                static_cast<int>(INITIAL_BACKOFF_MS * std::pow(BACKOFF_MULTIPLIER, retry_count - 1)),
                MAX_BACKOFF_MS
            );

            backoff_ms = static_cast<int>(backoff_ms * dis(gen));

            std::cout << "[VolumeBands] Reconnecting in " << backoff_ms / 1000.0 << " seconds... (attempt "
                      << retry_count << ")" << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
        }
    }

private:
    std::string target_;
};

int main(int argc, char** argv) {
    std::string target_str = "localhost:50051";
    if (argc > 1) {
        target_str = argv[1];
    }
    std::cout << "VolumeBands Client connecting to: " << target_str << std::endl;

    VolumeBandsClient client(target_str);
    client.Run();
    return 0;
}