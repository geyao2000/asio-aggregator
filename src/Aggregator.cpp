#include "Aggregator.h"
#include "binance_connector.h"
#include "okx_connector.h"
#include "bitget_connector.h"
#include <iostream>
#include <grpcpp/server_builder.h>
#include <chrono>
#include <future>
#include <boost/asio/use_future.hpp>

Aggregator::Aggregator(boost::asio::io_context& ioc)
    : ioc_(ioc),
      strand_(boost::asio::make_strand(ioc)){}

Aggregator::~Aggregator() {
    if (grpc_server_) {
        grpc_server_->Shutdown();
    }
    if (grpc_thread_.joinable()) {
        grpc_thread_.join();
    }
}

void Aggregator::start() {
    
    connectors_.emplace_back(std::make_shared<binance_connector>(
        ioc_, this, "Binance", "stream.binance.com", "9443", "/ws/btcusdt@depth20@100ms",
        [this](const std::string& ex, const std::string& msg) {
            on_market_event({ex, msg});
        }));

    connectors_.emplace_back(std::make_shared<okx_connector>(
        ioc_, this, "OKX", "ws.okx.com", "8443", "/ws/v5/public",
        [this](const std::string& ex, const std::string& msg) {
            on_market_event({ex, msg});
        }));

    connectors_.emplace_back(std::make_shared<bitget_connector>(
        ioc_, this, "Bitget", "ws.bitget.com", "443", "/v2/ws/public",
        [this](const std::string& ex, const std::string& msg) {
            on_market_event({ex, msg});
        }));

    for (auto& c : connectors_) {
        c->start();
    }

    grpc_thread_ = std::thread([this] { start_grpc_server(); });
}

void Aggregator::on_market_event(const market_event& evt) {
    std::cout << "[" << evt.exchange << "] Raw: " << evt.message << std::endl;
}

// connector 回调时调用这个（异步 post）
void Aggregator::on_book_updated(market_connector* connector) {
    // 把实际更新操作 post 到 strand，保证串行、无锁
    boost::asio::post(strand_, [this, connector]() {
        update_consolidated_book(connector);
    });
}

void Aggregator::update_consolidated_book(market_connector* connector) {
    // strand 保证这里是单线程执行，无需锁
    consolidated_bids_.clear();
    consolidated_asks_.clear();

    for (const auto& c : connectors_) {
        for (const auto& [price, qty] : c->get_bids()) {
            consolidated_bids_[price] += qty;
        }
        for (const auto& [price, qty] : c->get_asks()) {
            consolidated_asks_[price] += qty;
        }
    }

    // latest_book_update_ = build_book_update();
    version_.fetch_add(1, std::memory_order_release);

    // 可以在这里加日志或其他通知
    // std::cout << "[" << name_ << "] Book updated, version: " << version_.load() << std::endl;
}

aggregator::BookUpdate Aggregator::build_book_update() {
    aggregator::BookUpdate update;
    update.set_timestamp_ms(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    // 限制深度，例如 50 档
    constexpr int MAX_DEPTH = 150;
    int count = 0;
    for (const auto& [price, qty] : consolidated_bids_) {
        if (count++ >= MAX_DEPTH) break;
        auto* level = update.add_bids();
        level->set_price(price);
        level->set_quantity(qty);
    }

    count = 0;
    for (const auto& [price, qty] : consolidated_asks_) {
        if (count++ >= MAX_DEPTH) break;
        auto* level = update.add_asks();
        level->set_price(price);
        level->set_quantity(qty);
    }

    return update;
}

void Aggregator::start_grpc_server() {
    std::string server_address("0.0.0.0:50051");

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);

    grpc_server_ = builder.BuildAndStart();
    std::cout << "gRPC server listening on " << server_address << std::endl;

    grpc_server_->Wait();
}

grpc::Status Aggregator::SubscribeBook(grpc::ServerContext* context,
                                       const aggregator::SubscribeRequest* request,
                                       grpc::ServerWriter<aggregator::BookUpdate>* writer) {
    
    uint64_t last_seen_version = 0;

    while (!context->IsCancelled()) {
        // 使用 promise / future 等待 strand 执行并获取最新消息
        std::promise<std::pair<aggregator::BookUpdate, uint64_t>> prom;
        auto fut = prom.get_future();

        boost::asio::post(strand_, [&prom, this]() {
            aggregator::BookUpdate update = build_book_update();
            uint64_t ver = version_.load(std::memory_order_acquire);
            prom.set_value({update, ver});
        });

        auto [update, current_version] = fut.get();  // 阻塞等待完成

        if (current_version > last_seen_version) {
            last_seen_version = current_version;

            // protobuf 序列化前确保状态一致（通常不需要，但保险）
            update.mutable_bids()->Reserve(update.bids_size());
            update.mutable_asks()->Reserve(update.asks_size());

            if (!writer->Write(update)) {
                break;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    return grpc::Status::OK;
}
