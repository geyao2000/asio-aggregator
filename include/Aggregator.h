#pragma once
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <vector>
#include <string>
#include <map>
// #include <mutex>
#include <condition_variable>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "aggregator.grpc.pb.h"  // Generated from proto

struct market_event {
    std::string exchange;
    std::string message;
};

class market_connector;

// class aggregator {
class Aggregator : public aggregator::AggregatorService::Service {
public:
    explicit Aggregator(boost::asio::io_context& ioc);
    ~Aggregator();

    void start();

    // 被 connector 调用，异步 post 到 strand 处理
    void on_book_updated(market_connector* connector);

private:
    void on_market_event(const market_event& evt);

    void start_grpc_server();

    // gRPC 服务实现
    grpc::Status SubscribeBook(grpc::ServerContext* context,
                               const aggregator::SubscribeRequest* request,
                               grpc::ServerWriter<aggregator::BookUpdate>* writer) override;
    
    // 在 strand 上执行的更新逻辑
    void update_consolidated_book(market_connector* connector);

    // 构建 proto 消息（在 strand 内调用）
    aggregator::BookUpdate build_book_update();
    
    boost::asio::io_context& ioc_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    
    std::vector<std::shared_ptr<market_connector>> connectors_;

    // consolidated 数据（只在 strand 线程访问）
    std::map<double, double, std::greater<double>> consolidated_bids_;
    std::map<double, double> consolidated_asks_;

    // 最新 proto 消息（只在 strand 线程写入，其他线程只读快照）
    // aggregator::BookUpdate latest_book_update_;
    std::atomic<uint64_t> version_{0};  // 用于客户端判断是否有新数据

    std::thread grpc_thread_;
    std::unique_ptr<grpc::Server> grpc_server_;
};
