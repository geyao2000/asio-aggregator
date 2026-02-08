README.md
Aggregator Project
Overview
This project aggregates BTCUSDT spot order book data from Binance, OKX, and Bitget, consolidates into one book, and streams updates via gRPC to clients. Clients print BBO, volume bands, or price bands to stdout.
Technical Decisions

WS Connections: Boost.Asio for async WS.
Parsing: nlohmann/json for JSON messages.
Aggregation: Sum quantities at price levels across exchanges, top 50 levels streamed.
gRPC: Server-side streaming for updates, sync with CV for efficiency.
Scalability: Asio single-threaded, gRPC multi-threaded streams. For high load, add async gRPC.
Tests: Basic Catch2 tests for parsing (expand as needed).

How to Build

Install dependencies: sudo apt install libboost-all-dev libssl-dev libprotobuf-dev protobuf-compiler nlohmann-json3-dev libcatch2-dev cmake git build-essential
Build gRPC (see Dockerfile for script).
mkdir build && cd build && cmake .. && make -j

How to Run Locally

./aggregator
In separate terminals: ./bbo_client, ./volume_bands_client, ./price_bands_client

Docker

docker build -t aggregator_project .
docker compose up

Test Coverage

Run ./tests
Coverage ~70% (parsing, consolidation logic).

For questions, expand to more symbols by adding subclasses.