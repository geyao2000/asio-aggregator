#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"
#include "aggregator.pb.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>      // 用于 std::put_time
#include <sstream>      // 用于格式化时间字符串

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using aggregator::AggregatorService;
using aggregator::SubscribeRequest;
using aggregator::BookUpdate;

// ANSI 颜色宏定义（必须放在文件顶部）
#define COLOR_RED     "\033[31m"     // 红色
#define COLOR_BLUE    "\033[34m"     // 蓝色
#define COLOR_RESET   "\033[0m"      // 重置颜色

class BBOClient {
public:
    BBOClient(const std::string& target) : target_(target) {}

    void Run() {
        constexpr int MAX_BACKOFF_MS = 30000;  // 最大退避 30 秒
        constexpr int INITIAL_BACKOFF_MS = 1000;
        constexpr double BACKOFF_MULTIPLIER = 2.0;

        int retry_count = 0;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.8, 1.2);  // jitter ±20%

        while (true) {
            std::shared_ptr<Channel> channel = grpc::CreateChannel(
                target_, grpc::InsecureChannelCredentials());

            std::unique_ptr<AggregatorService::Stub> stub(AggregatorService::NewStub(channel));

            ClientContext context;
            SubscribeRequest request;
            request.set_symbol("BTCUSDT");

            std::unique_ptr<grpc::ClientReader<BookUpdate>> reader(
                stub->SubscribeBook(&context, request));

            std::cout << "[BBO] Connected to " << target_ << ", subscribing to BTCUSDT..." << std::endl;

            BookUpdate update;
            bool connected = true;

            while (reader->Read(&update)) {
                if (update.bids().empty() || update.asks().empty()) {
                    continue;
                }

                double bid_price = update.bids(0).price();
                double bid_qty   = update.bids(0).quantity();
                double ask_price = update.asks(0).price();
                double ask_qty   = update.asks(0).quantity();

                // 计算 crossed 警告（保留小数点后 2 位）
                double spread = bid_price - ask_price;
                std::string warning;
                if (spread > 0) {
                    std::ostringstream oss_warning;
                    oss_warning << std::fixed << std::setprecision(2) << spread;
                    warning = ", warning: crossed: " + oss_warning.str();
                }

                // 获取当前本地时间并格式化为指定样式
                auto now = std::chrono::system_clock::now();
                auto now_time_t = std::chrono::system_clock::to_time_t(now);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()) % 1000;

                std::ostringstream oss;
                oss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S")
                    << "." << std::setfill('0') << std::setw(3) << ms.count();

                // 输出指定格式 + 颜色
                std::cout << "=== BBO Update @ " << oss.str() << " Local ===\n"
                          << "Best Ask: " << COLOR_BLUE << std::fixed << std::setprecision(2) << ask_price
                          << COLOR_RESET << " @ " << std::fixed << std::setprecision(8) << ask_qty << "\n"
                          << "Best Bid: " << COLOR_RED << std::fixed << std::setprecision(2) << bid_price
                          << COLOR_RESET << " @ " << std::fixed << std::setprecision(8) << bid_qty << warning << "\n"
                          << "==============================================\n";
                // 读取成功，重置重试计数
                retry_count = 0;
            }

            // Read 返回 false，通常是流结束或错误
            Status status = reader->Finish();

            if (status.ok()) {
                std::cout << "[BBO] Stream completed normally." << std::endl;
            } else {
                std::cerr << "[BBO] RPC failed: " << status.error_code()
                          << ": " << status.error_message() << std::endl;
            }

            // 断线重连逻辑
            connected = false;
            retry_count++;

            int backoff_ms = std::min(
                static_cast<int>(INITIAL_BACKOFF_MS * std::pow(BACKOFF_MULTIPLIER, retry_count - 1)),
                MAX_BACKOFF_MS
            );

            // 加随机抖动（jitter）避免 thundering herd
            backoff_ms = static_cast<int>(backoff_ms * dis(gen));

            std::cout << "[BBO] Reconnecting in " << backoff_ms / 1000.0 << " seconds... (attempt "
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

    std::cout << "BBO Client connecting to: " << target_str << std::endl;

    BBOClient client(target_str);
    client.Run();

    return 0;
}