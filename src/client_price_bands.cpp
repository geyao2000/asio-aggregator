#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include "aggregator.pb.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>

// ANSI 颜色宏（放在文件顶部）
#define COLOR_RED     "\033[31m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using aggregator::AggregatorService;
using aggregator::SubscribeRequest;
using aggregator::BookUpdate;

class PriceBandsClient {
public:
    PriceBandsClient(const std::string& target) : target_(target) {}

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

            std::cout << "[PriceBands] Connected to " << target_ << ", subscribing to BTCUSDT..." << std::endl;

            BookUpdate update;
            bool connected = true;

            while (reader->Read(&update)) {
                if (update.bids().empty() || update.asks().empty()) {
                    continue;
                }

                double best_bid = update.bids(0).price();
                double best_ask = update.asks(0).price();
                double mid = (best_bid + best_ask) / 2.0;

                double spread = best_bid - best_ask;
                std::string warning;
                if (spread > 0) {
                    std::ostringstream oss_warn;
                    oss_warn << std::fixed << std::setprecision(2) << spread;
                    warning = "⚠️ WARNING: Crossed market detected! " + oss_warn.str() + "\n";
                }

                // 时间戳
                auto now = std::chrono::system_clock::now();
                auto now_time_t = std::chrono::system_clock::to_time_t(now);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()) % 1000;

                std::ostringstream oss_time;
                oss_time << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S")
                         << "." << std::setfill('0') << std::setw(3) << ms.count();

                // 输出标题和 BBO
                std::cout << "=== Price Bands Update @ " << oss_time.str() << " JST ===\n"
                          << "BBO: Best Bid " << std::fixed << std::setprecision(2) << COLOR_RED << best_bid << COLOR_RESET
                          << " | Best Ask " << std::fixed << std::setprecision(2) << COLOR_BLUE << best_ask << COLOR_RESET
                          << " | Mid " << std::fixed << std::setprecision(2) << mid << "\n"
                          << warning;

                // 表格头部
                std::cout << "+ bps | Target Bid | Closest Bid |   Qty (BTC)  | Target Ask | Closest Ask | Qty (BTC)\n"
                          << "------|------------|-------------|--------------|------------|-------------|----------\n";

                std::vector<int> bps_levels = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000};

                for (int bps : bps_levels) {
                    // Bid side
                    double bid_target = mid * (1.0 - bps / 10000.0);
                    double bid_cum_vol = 0.0;
                    double bid_closest = 0.0;
                    bool bid_found = false;

                    for (const auto& level : update.bids()) {
                        if (level.price() >= bid_target) {
                            bid_cum_vol += level.quantity();
                            bid_closest = level.price();
                            bid_found = true;
                        } else {
                            break;
                        }
                    }

                    // Ask side
                    double ask_target = mid * (1.0 + bps / 10000.0);
                    double ask_cum_vol = 0.0;
                    double ask_closest = 0.0;
                    bool ask_found = false;

                    for (const auto& level : update.asks()) {
                        if (level.price() <= ask_target) {
                            ask_cum_vol += level.quantity();
                            ask_closest = level.price();
                            ask_found = true;
                        } else {
                            break;
                        }
                    }

                    // 输出一行（Target Bid/Ask 去掉前导 00，Closest 精确 2 位，宽度保持）
                    std::cout << std::right <<"+"<< std::setfill('0') << std::setw(4) << bps << " | "
                              << std::fixed << std::setprecision(2) << std::setw(10) << bid_target << " | ";

                    if (bid_found) {
                        std::cout << std::fixed << std::setprecision(2) << std::setw(11) << bid_closest;
                    } else {
                        std::cout << std::setw(11) << "N/A";
                    }

                    std::cout << " | "
                              << std::fixed << std::setprecision(10) << std::setw(10) << bid_cum_vol << " | "
                              << std::fixed << std::setprecision(2) << std::setw(10) << ask_target << " | ";

                    if (ask_found) {
                        std::cout << std::fixed << std::setprecision(2) << std::setw(11) << ask_closest;
                    } else {
                        std::cout << std::setw(11) << "N/A";
                    }

                    std::cout << " | "
                              << std::fixed << std::setprecision(10) << std::setw(10) << ask_cum_vol << "\n";
                }

                std::cout << "========================================================\n";

                retry_count = 0;
            }

            Status status = reader->Finish();
            if (status.ok()) {
                std::cout << "[PriceBands] Stream completed normally." << std::endl;
            } else {
                std::cerr << "[PriceBands] RPC failed: " << status.error_code()
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

            std::cout << "[PriceBands] Reconnecting in " << backoff_ms / 1000.0 << " seconds... (attempt "
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
    std::cout << "PriceBands Client connecting to: " << target_str << std::endl;

    PriceBandsClient client(target_str);
    client.Run();
    return 0;
}